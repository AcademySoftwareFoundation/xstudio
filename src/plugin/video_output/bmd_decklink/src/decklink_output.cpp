// SPDX-License-Identifier: Apache-2.0
#include "decklink_output.hpp"
#include "decklink_plugin.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/enums.hpp"
#include <iostream>
#include <half.h>

#ifdef __linux__
#include <dlfcn.h>
#define kDeckLinkAPI_Name "libDeckLinkAPI.so"
#endif

using namespace xstudio::bm_decklink_plugin_1_0;

// Uncomment to use this debug timer to see how the frame conversion is performing
namespace {

    class LogBot {
        public:
        std::map<std::string, std::vector<int64_t>> frame_times_;
        void log(const std::string l, int64_t t) {
            frame_times_[l].push_back(t);
            if ((frame_times_[l].size()%24) == 0) {
                int64_t total = 0;
                for (auto v : frame_times_[l]) total += v;
                std::cerr << "Average time for " << l << ": " << double(total/frame_times_[l].size())/1000.0 << "ms\n";
                frame_times_[l].clear();
            }
        }
    };
    static LogBot s_logBot;

    class TimeLogger {
        public:
        TimeLogger(const std::string &label) : label_(label), start_time_(std::chrono::high_resolution_clock::now()) {}
        ~TimeLogger() {
            s_logBot.log(label_, std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock::now() - start_time_).count());
        }
        std::string label_;
        std::chrono::high_resolution_clock::time_point start_time_;
    };
}

void DecklinkOutput::check_decklink_installation()
{

    // here we try to open the decklink driver libs. If they are not installed
    // on the system abort construction of the plugin (caught by plugin manager)
    // The reason we do this is that we don't want the plugin to be available at
    // all in the UI if the drivers aren't present, as it would just lead to 
    // user confusion and support requests about why the plugin doesn't work.
#ifdef __APPLE__
    CFURLRef bundleURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
        CFSTR("/Library/Frameworks/DeckLinkAPI.framework"), kCFURLPOSIXPathStyle, true);
    bool drivers_found = false;
    if (bundleURL) {
        CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, bundleURL);
        drivers_found = (bundle != NULL);
        if (bundle) CFRelease(bundle);
        CFRelease(bundleURL);
    }
    if (!drivers_found) {
		throw std::runtime_error("drivers not found.");		
        return;
    }
#elif defined(__linux__)
    if (!dlopen(kDeckLinkAPI_Name, RTLD_NOW | RTLD_GLOBAL)) {
		throw std::runtime_error("drivers not found.");
        return;
    }
#else // Windows
	IDeckLinkIterator* pDLIterator = NULL;
	HRESULT result; 
	result = CoCreateInstance(CLSID_CDeckLinkIterator, NULL, CLSCTX_ALL, IID_IDeckLinkIterator, (void**)&pDLIterator);
	if (FAILED(result))
	{
		throw std::runtime_error("drivers not found.");
	}

    // If no device found, that will be caught later.
    /*IDeckLink*	deckLink = nullptr;
	if (pDLIterator->Next(&deckLink) != S_OK)
	{
        if (deckLink != NULL)
		{
            deckLink->Release();
        } else {
            throw std::runtime_error("no DeckLink devices found.");
        }
	}*/
#endif	
}

DecklinkOutput::DecklinkOutput(BMDecklinkPlugin * decklink_xstudio_plugin)
	: pFrameBuf(NULL), decklink_interface_(NULL), decklink_output_interface_(NULL), decklink_xstudio_plugin_(decklink_xstudio_plugin)
{

    init_decklink();
    
}

DecklinkOutput::~DecklinkOutput()
{

	if (decklink_output_interface_ != NULL)
	{

        spdlog::info("Stopping Decklink output loop.");

    	decklink_output_interface_->StopScheduledPlayback(0, NULL, 0);
	    decklink_output_interface_->DisableVideoOutput();
	    decklink_output_interface_->DisableAudioOutput();

		decklink_output_interface_->Release();
	}
	if (decklink_interface_ != NULL)
	{
		decklink_interface_->Release();
	}
	if (output_callback_ != NULL)
	{
		output_callback_->Release();
	}

    if (frame_converter_ != NULL)
	{
		frame_converter_->Release();
	}

    if (intermediate_frame_ != nullptr) {
        intermediate_frame_->Release();
    }   
    spdlog::info("Closing Decklink Output");

}

void DecklinkOutput::set_preroll()
{
	IDeckLinkMutableVideoFrame* decklink_video_frame = NULL;

	// Set 3 frame preroll
    try {

        for (uint32_t i=0; i < 3; i++)
        {

            int32_t rowBytes;
            if (decklink_output_interface_->RowBytesForPixelFormat(
                current_pix_format_, 
                frame_width_, 
                &rowBytes
                ) != S_OK) {
                throw std::runtime_error("Failed on call to RowBytesForPixelFormat.");
            }

            // Flip frame vertical, because OpenGL rendering starts from left bottom corner
            if (decklink_output_interface_->CreateVideoFrame(frame_width_, frame_height_, rowBytes, current_pix_format_, bmdFrameFlagFlipVertical, &decklink_video_frame) != S_OK)
                throw std::runtime_error("Failed on CreateVideoFrame");

            update_frame_metadata(decklink_video_frame);

            if (decklink_output_interface_->ScheduleVideoFrame(decklink_video_frame, (uiTotalFrames * frame_duration_), frame_duration_, frame_timescale_) != S_OK)
                throw std::runtime_error("Failed on ScheduleVideoFrame");

            /* The local reference to the IDeckLinkVideoFrame is released here, as the ownership has now been passed to
            *  the DeckLinkAPI via ScheduleVideoFrame.
            *
            * After the API has finished with the frame, it is returned to the application via ScheduledFrameCompleted.
            * In ScheduledFrameCompleted, this application updates the video frame and passes it to ScheduleVideoFrame,
            * returning ownership to the DeckLink API.
            */
            decklink_video_frame->Release();
            decklink_video_frame = NULL;

            uiTotalFrames++;
        }
    } catch (std::exception & e) {

        if (decklink_video_frame)
        {
            decklink_video_frame->Release();
            decklink_video_frame = NULL;
        }
        report_error(e.what());
    }
}

void DecklinkOutput::make_intermediate_frame() {

    // We need to support some othe pixel formats less likely to be needed but 
    // nevertheless possibly required. For this we convert to BMD 12 bit RGB
    // (which seems to be the most efficient conversion on our side) and then
    // use Decklink API to convert from that to the desired output format.
    int32_t referenceBytesPerRow;

    HRESULT result = decklink_output_interface_->RowBytesForPixelFormat(
        bmdFormat12BitRGBLE, 
        frame_width_, 
        &referenceBytesPerRow
    );

    if (result != S_OK)
    {
        throw std::runtime_error("Failed to get row bytes for reference video frame");
    }

    if (intermediate_frame_) {
        if (intermediate_frame_->GetWidth() != frame_width_ ||
            intermediate_frame_->GetHeight() != frame_height_) {
            intermediate_frame_->Release();
            intermediate_frame_ = nullptr;
        }
    }

    if (!intermediate_frame_) {
        // Create a black frame in 8 bit YUV and convert to desired format
        result = decklink_output_interface_->CreateVideoFrame(
            frame_width_,
            frame_height_,
            referenceBytesPerRow,
            bmdFormat12BitRGBLE, 
            bmdFrameFlagDefault, 
            &intermediate_frame_
            );

        if (result != S_OK)
        {
            throw std::runtime_error("Failed to create reference video frame");
        }
    }
}


bool DecklinkOutput::init_decklink()
{
	bool bSuccess = false;

    IDeckLinkIterator* decklink_iterator = NULL;

    try {

#ifdef _WIN32        
       	HRESULT result; 
    	result = CoCreateInstance(CLSID_CDeckLinkIterator, NULL, CLSCTX_ALL, IID_IDeckLinkIterator, (void**)&decklink_iterator);
        if (FAILED(result))
        {
            throw std::runtime_error("Please install the Blackmagic DeckLink drivers to use the features of this application.This application requires the DeckLink drivers installed.");
            return false;
        }
#else
        decklink_iterator = CreateDeckLinkIteratorInstance();
#endif
        if (decklink_iterator == NULL)
        {
            throw std::runtime_error("This plugin requires the DeckLink drivers installed. Please install the Blackmagic DeckLink drivers to use the features of this plugin.");
        }

        if (decklink_iterator->Next(&decklink_interface_) != S_OK)
        {
            throw std::runtime_error("This plugin requires a DeckLink device. You will not be able to use the features of this plugin until a DeckLink device is installed.");
        }
        
        if (decklink_interface_->QueryInterface(IID_IDeckLinkOutput, (void**)&decklink_output_interface_) != S_OK) {
           throw std::runtime_error("QueryInterface failed.");
        }

        output_callback_ = new AVOutputCallback(this);
        if (output_callback_ == NULL)
            throw std::runtime_error("Failed to create Video Output Callback.");

        if (decklink_output_interface_->SetScheduledFrameCompletionCallback(output_callback_) != S_OK)
            throw std::runtime_error("SetScheduledFrameCompletionCallback failed.");

        if (decklink_output_interface_->SetAudioCallback(output_callback_) != S_OK)
            throw std::runtime_error("SetAudioCallback failed.");

#ifdef _WIN32
        // Create an IDeckLinkVideoConversion interface object to provide pixel format conversion of video frame.
        result = CoCreateInstance(CLSID_CDeckLinkVideoConversion, NULL, CLSCTX_ALL, IID_IDeckLinkVideoConversion, (void**)&frame_converter_);
        if (FAILED(result))
        {
            throw std::runtime_error("A DeckLink Video Conversion interface could not be created.");
        }

#else

        frame_converter_ = CreateVideoConversionInstance();

#endif

        bSuccess = true;

        query_display_modes();

    } catch (std::exception & e) {

        std::cerr << "DecklinkOutput::init_decklink() failed: " << e.what() << "\n";

        report_error(e.what());

        if (decklink_output_interface_ != NULL)
        {
            decklink_output_interface_->Release();
            decklink_output_interface_ = NULL;
        }
        if (decklink_interface_ != NULL)
        {
            decklink_interface_->Release();
            decklink_interface_ = NULL;
        }
        if (output_callback_ != NULL)
        {
            output_callback_->Release();
            output_callback_ = NULL;
        }

        if (decklink_iterator != NULL)
        {
            decklink_iterator->Release();
            decklink_iterator = NULL;
        }

    }


	return bSuccess;
}

void DecklinkOutput::query_display_modes() {

    IDeckLinkDisplayModeIterator*		display_mode_iterator = NULL;
    IDeckLinkDisplayMode*				display_mode = NULL;

    try {

        // Get first avaliable video mode for Output
        if (decklink_output_interface_->GetDisplayModeIterator(&display_mode_iterator) == S_OK)
        {

            while (display_mode_iterator->Next(&display_mode) == S_OK) {

                DECKLINK_STR modeName = nullptr;
                display_mode->GetName(&modeName);
                std::string buf = decklink_string_to_std(modeName);
                decklink_free_string(modeName);
                display_mode->GetFrameRate(&frame_duration_, &frame_timescale_);

                // only names with 'i' in are interalaced as far as I can tell
                const bool interlaced = buf.find("i") != std::string::npos;

                // I've decided that support for interlaced modes is not useful!
                if (interlaced) continue;

                const std::string resolution_string = fmt::format("{} x {}", display_mode->GetWidth(), display_mode->GetHeight());
                std::string refresh_rate = fmt::format("{:.3f}", double(frame_timescale_)/double(frame_duration_));
                // erase all but the last trailing zero
                while (refresh_rate.back() == '0' && refresh_rate.rfind(".0") != (refresh_rate.size()-2)) {
                    refresh_rate.pop_back();
                }

                refresh_rate_per_output_resolution_[resolution_string].push_back(refresh_rate);
                display_modes_[std::make_pair(resolution_string, refresh_rate)] = display_mode->GetDisplayMode();

            }
        }
    } catch (std::exception & e) {

        report_error(e.what());
        
    }        
   
    if (display_mode)
    {
        display_mode->Release();
        display_mode = NULL;
    }

    if (display_mode_iterator)
    {
        display_mode_iterator->Release();
        display_mode_iterator = NULL;
    }        


}

std::vector<std::string> DecklinkOutput::get_available_refresh_rates(const std::string & output_resolution) const
{
    auto p = refresh_rate_per_output_resolution_.find(output_resolution);
    if (p != refresh_rate_per_output_resolution_.end()) {
        return p->second;
    }
    return std::vector<std::string>({"Bad Resolution"});
}

void DecklinkOutput::set_display_mode(
    const std::string & resolution,
    const std::string  &refresh_rate,
    const BMDPixelFormat pix_format) 
{

    auto p = display_modes_.find(std::make_pair(resolution, refresh_rate));
    if (p == display_modes_.end()) {
        throw std::runtime_error(fmt::format("Failed to find a display mode for {} @ {}", resolution, refresh_rate));
    }
    current_pix_format_ = pix_format;
    current_display_mode_ = p->second;
}


bool DecklinkOutput::start_sdi_output()
{

	bool								bSuccess = false;

    IDeckLinkDisplayModeIterator*		display_mode_iterator = NULL;
    IDeckLinkDisplayMode*				display_mode = NULL;

    try {
        
        bool mode_matched = false;
        // Get first avaliable video mode for Output
        if (decklink_output_interface_->GetDisplayModeIterator(&display_mode_iterator) == S_OK)
        {
            while (display_mode_iterator->Next(&display_mode) == S_OK) {
                if (display_mode->GetDisplayMode() == current_display_mode_) {

                    mode_matched = true;
                    {
                        // get the name of the display mode, for display only
                        DECKLINK_STR modeName = nullptr;
                        display_mode->GetName(&modeName);
                        display_mode_name_ = decklink_string_to_std(modeName);
                        decklink_free_string(modeName);
                    }

                    report_status(fmt::format("Starting Decklink output loop in mode {}.", display_mode_name_), false);

                    frame_width_ = display_mode->GetWidth();
                    frame_height_ = display_mode->GetHeight();
                    display_mode->GetFrameRate(&frame_duration_, &frame_timescale_);
                    
                    uiFPS = ((frame_timescale_ + (frame_duration_-1))  /  frame_duration_);
                    
                    if (decklink_output_interface_->EnableVideoOutput(display_mode->GetDisplayMode(), bmdVideoOutputFlagDefault) != S_OK) {
                        throw std::runtime_error("EnableVideoOutput call failed.");
                    }


                }
            }
        }

        if (!mode_matched) {
            throw std::runtime_error("Failed to find display mode.");
        }

        uiTotalFrames = 0;
        
        // Set the audio output mode
        if (decklink_output_interface_->EnableAudioOutput(
            bmdAudioSampleRate48kHz,
            bmdAudioSampleType16bitInteger,
            2, // num channels
            bmdAudioOutputStreamTimestamped) != S_OK) 
        {
            throw std::runtime_error("Failed to enable audio output.");
        }


        set_preroll();

        samples_delivered_ = 0;

        if (decklink_output_interface_->BeginAudioPreroll() != S_OK) {
            throw std::runtime_error("Failed to pre-roll audio output.");
        }

        decklink_output_interface_->StartScheduledPlayback(0, 100, 1.0);
        
        bSuccess = true;

    } catch (std::exception & e) {


        report_error(e.what());

    }

    if (display_mode)
    {
        display_mode->Release();
        display_mode = NULL;
    }

    if (display_mode_iterator)
    {
        display_mode_iterator->Release();
        display_mode_iterator = NULL;
    }        

	return bSuccess;
}

bool DecklinkOutput::stop_sdi_output(const std::string &error_message)
{

    running_ = false;
    decklink_xstudio_plugin_->stop();

    if (!error_message.empty()) {
        report_error(error_message);
    } else {
        report_status("SDI Output Paused.", false);
    }

    spdlog::info("Stopping Decklink output loop. {}", error_message);

	decklink_output_interface_->StopScheduledPlayback(0, NULL, 0);
	decklink_output_interface_->DisableVideoOutput();
	decklink_output_interface_->DisableAudioOutput();

	mutex_.lock();
	
	free(pFrameBuf);
	pFrameBuf = NULL;

	mutex_.unlock();

    spdlog::info("Stopping Decklink output loop done. {}", error_message);

	return true;
}

void DecklinkOutput::StartStop() {
    if (!running_) start_sdi_output();
    else stop_sdi_output();
}

void DecklinkOutput::incoming_frame(const media_reader::ImageBufPtr &incoming) {

    // this is called from xstudio managed thread, which is independent of
    // the decklink output thread control
	frames_mutex_.lock();
    current_frame_ = incoming;
	frames_mutex_.unlock();

}

namespace {
    void multithreadMemCopy(
        void * _dst,
        void * _src,
        size_t buf_size,
        const int n_threads) 
    {

        // Note: my instinct tells me that spawning threads for
        // every copy operation (which might happen 60 times a second)
        // is not efficient but it seems that having a threadpool doesn't
        // make any real difference, the overhead of thread creation
        // is tiny.
        std::vector<std::thread> memcpy_threads;
        size_t step = ((buf_size / n_threads) / 4096) * 4096;

        uint8_t *dst = (uint8_t *)_dst;
        uint8_t *src = (uint8_t *)_src;

        for (int i = 0; i < n_threads; ++i) {
            memcpy_threads.emplace_back(memcpy, dst, src, std::min(buf_size, step));
            dst += step;
            src += step;
            buf_size -= step;
        }

        // ensure any threads still running to copy data to this texture are done
        for (auto &t : memcpy_threads) {
            if (t.joinable())
                t.join();
        }
    }

}

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

void DecklinkOutput::report_status(const std::string & status_message, const bool sdi_output_is_active) {

    utility::JsonStore j;
    j["status_message"] = status_message;
    j["sdi_output_is_active"] = sdi_output_is_active;    
    j["error_state"] = false;    
    decklink_xstudio_plugin_->send_status(j);

}

void DecklinkOutput::report_error(const std::string & status_message) {

    utility::JsonStore j;
    j["status_message"] = status_message;
    j["sdi_output_is_active"] = false;    
    j["error_state"] = true;    
    decklink_xstudio_plugin_->send_status(j);
}

void DecklinkOutput::fill_decklink_video_frame(IDeckLinkVideoFrame* decklink_video_frame)
{

    // this function (fill_decklink_video_frame) is called by the Decklink API at a steady beat
    // matching the refresh rate of the SDI output. We can therefore use it to tell our offscreen
    // viewport to render a new frame ready for the subsequent call to this function. Remember
    // The frame rendered by xstduio is delivered to us via the incoming_frame callback and, as
    // long as xstudio is able to render the video frame in somthing less than the refresh period
    // for the SDI output, it should have been delivered before we re-enter this function.
    //
    // The time value passed into this request is our best estimate of when the frame that we are
    // requesting will actually be put on the screen.
    decklink_xstudio_plugin_->request_video_frame(utility::clock::now());


    // We also need to make this crucial call to tell xstudio's offscreen viewport when the
    // last video frame was put on screen. It uses the regular beat of these calls to work
    // out the refresh rate of the video output and therefore do an accurate 'pulldown' when
    // evaluating the playhead position for the next frame to go on screen.
    // In the case of the Decklink, we know that this function (fill_decklink_video_frame) is being
    // called with a beat matching the SDI refresh (as long as our code immediately below 
    // completes well inside/ that period)
    decklink_xstudio_plugin_->video_frame_consumed(utility::clock::now());

    static auto tp = utility::clock::now();
    auto tp1 = utility::clock::now();
    tp = tp1;

	mutex_.lock();

	// SDK v15.3: GetBytes() moved from IDeckLinkVideoFrame to IDeckLinkVideoBuffer
	IDeckLinkVideoBuffer* video_buffer = nullptr;
	decklink_video_frame->QueryInterface(IID_IDeckLinkVideoBuffer, (void**)&video_buffer);

	frames_mutex_.lock();
    media_reader::ImageBufPtr the_frame = current_frame_;
	frames_mutex_.unlock();

    if (the_frame) {

        auto tp = utility::clock::now();

        if (the_frame->size() >= decklink_video_frame->GetRowBytes()*frame_height_) {

            int xstudio_buf_pixel_format = the_frame->params().value("pixel_format", 0);

            if (xstudio_buf_pixel_format == ui::viewport::RGBA_16 && video_buffer) {

                // On macOS, DMA buffers may need StartAccess to map into CPU memory
                HRESULT sa_hr = video_buffer->StartAccess(bmdBufferAccessReadAndWrite);
                try {

                    void*	pFrame = nullptr;
                    HRESULT gb_hr = video_buffer->GetBytes((void**)&pFrame);
                    int num_pix = decklink_video_frame->GetWidth() * decklink_video_frame->GetHeight();
                    void* src_buf = the_frame->buffer();

                    // Validate pointers and ensure source buffer is large enough
                    // for num_pix RGBA_16 pixels (8 bytes each)
                    if (!pFrame || !src_buf) {
                        throw std::runtime_error(fmt::format("Decklink: null buffer in fill_decklink_video_frame "
                            "(pFrame={}, src_buf={}, num_pix={}, StartAccess=0x{:x}, GetBytes=0x{:x})",
                            pFrame, src_buf, num_pix, (unsigned long)sa_hr, (unsigned long)gb_hr));
                    } else if (the_frame->size() < (size_t)num_pix * 8) {
                        throw std::runtime_error(fmt::format("Decklink: source buffer too small "
                            "(size={}, need={}, frame={}x{}, src_format=RGBA_16)",
                            the_frame->size(), (size_t)num_pix * 8,
                            decklink_video_frame->GetWidth(), decklink_video_frame->GetHeight()));
                    } else if (decklink_video_frame->GetPixelFormat() == bmdFormat10BitRGB) {

                        //TimeLogger l("RGBA16_to_10bitRGB");
                        pixel_swizzler_.copy_frame_buffer_10bit<RGBA16_to_10bitRGB>(pFrame, src_buf, num_pix);

                    } else if (decklink_video_frame->GetPixelFormat() == bmdFormat10BitRGBXLE) {

                        //TimeLogger l("RGBA16_to_10bitRGBXLE");
                        pixel_swizzler_.copy_frame_buffer_10bit<RGBA16_to_10bitRGBXLE>(pFrame, src_buf, num_pix);

                    } else if (decklink_video_frame->GetPixelFormat() == bmdFormat10BitRGBX) {

                        //TimeLogger l("RGBA16_to_10bitRGBX");
                        pixel_swizzler_.copy_frame_buffer_10bit<RGBA16_to_10bitRGBX>(pFrame, src_buf, num_pix);

                    } else if (decklink_video_frame->GetPixelFormat() == bmdFormat12BitRGB) {

                        //TimeLogger l("RGBA16_to_12bitRGB");
                        pixel_swizzler_.copy_frame_buffer_12bit<RGBA16_to_12bitRGB>(pFrame, src_buf, num_pix);

                    } else if (decklink_video_frame->GetPixelFormat() == bmdFormat12BitRGBLE) {

                        //TimeLogger l("RGBA16_to_12bitRGBLE");
                        pixel_swizzler_.copy_frame_buffer_12bit<RGBA16_to_12bitRGBLE>(pFrame, src_buf, num_pix);

                    } else {

                        // here we do our own conversion from 16 bit RGBA to 12 bit RGB
                        // TimeLogger l("RGBA16_to_12bitRGBLE");
                        make_intermediate_frame();

                        IDeckLinkVideoBuffer* intermediate_video_buffer = nullptr;
                        auto r = intermediate_frame_->QueryInterface(IID_IDeckLinkVideoBuffer, (void **)&intermediate_video_buffer);
                        if (r != S_OK || !intermediate_video_buffer) {
                            throw std::runtime_error("Failed to get conversion video buffer");
                        }

                        r = intermediate_video_buffer->StartAccess(bmdBufferAccessWrite);
                        if (r != S_OK)
                        {
                            throw std::runtime_error("Could not access the video frame byte buffer");
                        }

                        void*	pFrame2 = nullptr;

                        r = intermediate_video_buffer->GetBytes((void**)&pFrame2);
                        if (r != S_OK || !pFrame2) {
                            throw std::runtime_error("Conversion video buffer has no bytes pointer");
                        }
                        pixel_swizzler_.copy_frame_buffer_12bit<RGBA16_to_12bitRGBLE>(pFrame2, src_buf, num_pix);

                        intermediate_video_buffer->EndAccess(bmdBufferAccessWrite);

                        // Now we use Decklink's conversion to convert from our intermediate 12 bit RGB frame 
                        // to the desired output format (e.g. 10 bit YUV)
                        r = frame_converter_->ConvertFrame(intermediate_frame_, decklink_video_frame);
                        if (r != S_OK)
                        {
                            throw std::runtime_error("Failed to convert frame");
                        }

                    }

                } catch (std::exception & e) {

                    // reduce log spamming if we're getting errors on every frame
                    static int error_count = 0;
                    static std::string last_error; 
                    if (last_error != e.what() || (++error_count) == 120) {
                        spdlog::error("{} {}", __PRETTY_FUNCTION__, e.what());
                        last_error = e.what();
                        error_count = 0;
                    }
                }
                video_buffer->EndAccess(bmdBufferAccessReadAndWrite);
                
            }

        }
    }

    try {

        IDeckLinkMutableVideoFrame* mutable_video_buffer = nullptr;
        auto r = decklink_video_frame->QueryInterface(IID_IDeckLinkMutableVideoFrame, (void **)&mutable_video_buffer);
        if (r != S_OK || !mutable_video_buffer) {
            throw std::runtime_error("Failed to get mutable video buffer");
        }
        update_frame_metadata(mutable_video_buffer);

    } catch (std::exception & e) {
        // reduce log spamming if we're getting errors on every frame
        static int error_count = 0;
        static std::string last_error; 
        if (last_error != e.what() || (++error_count) == 120) {
            spdlog::error("{} {}", __PRETTY_FUNCTION__, e.what());
            last_error = e.what();
            error_count = 0;
        }
    }

	if (video_buffer) video_buffer->Release();

	if (decklink_output_interface_->ScheduleVideoFrame(decklink_video_frame, (uiTotalFrames * frame_duration_), frame_duration_, frame_timescale_) != S_OK)
	{
		mutex_.unlock();
        running_ = false;
        decklink_xstudio_plugin_->stop();
        report_error("Failed to schedule video frame.");
		return;
	}

    if (!running_) {
        running_ = true;
        report_status(fmt::format("Running in mode {}.", display_mode_name_), running_);
        decklink_xstudio_plugin_->start(frameWidth(), frameHeight());
    }
	uiTotalFrames++;
	mutex_.unlock();

}

void DecklinkOutput::update_frame_metadata(IDeckLinkMutableVideoFrame* mutableFrame)
{

    if (hdr_metadata_.EOTF == 0) {
        // SDR Mode - we don't need to set any metadata, but we do need to make sure to clear the HDR flag if it was set by a previous HDR frame
        mutableFrame->SetFlags(mutableFrame->GetFlags() & ~bmdFrameContainsHDRMetadata);
        return;
    }

	mutableFrame->SetFlags(mutableFrame->GetFlags() | bmdFrameContainsHDRMetadata);

	IDeckLinkVideoFrameMutableMetadataExtensions * frameMeta = nullptr;
    if (mutableFrame->QueryInterface(IID_IDeckLinkVideoFrameMutableMetadataExtensions, (void**)&frameMeta) != S_OK) {
        // This can fail if the drivers are old and don't support HDR metadata, in which case we just won't send any metadata
        throw std::runtime_error("Failed to get mutable metadata extensions for HDR metadata update.");
    }

    frameMeta->AddRef();
	frameMeta->SetInt(bmdDeckLinkFrameMetadataColorspace, hdr_metadata_.colourspace_);
	frameMeta->SetInt(bmdDeckLinkFrameMetadataHDRElectroOpticalTransferFunc, hdr_metadata_.EOTF);
	frameMeta->SetFloat(bmdDeckLinkFrameMetadataHDRDisplayPrimariesRedX, hdr_metadata_.referencePrimaries[0]);
	frameMeta->SetFloat(bmdDeckLinkFrameMetadataHDRDisplayPrimariesRedY, hdr_metadata_.referencePrimaries[1]);
	frameMeta->SetFloat(bmdDeckLinkFrameMetadataHDRDisplayPrimariesGreenX, hdr_metadata_.referencePrimaries[2]);
	frameMeta->SetFloat(bmdDeckLinkFrameMetadataHDRDisplayPrimariesGreenY, hdr_metadata_.referencePrimaries[3]);
	frameMeta->SetFloat(bmdDeckLinkFrameMetadataHDRDisplayPrimariesBlueX, hdr_metadata_.referencePrimaries[4]);
	frameMeta->SetFloat(bmdDeckLinkFrameMetadataHDRDisplayPrimariesBlueY, hdr_metadata_.referencePrimaries[5]);
	frameMeta->SetFloat(bmdDeckLinkFrameMetadataHDRWhitePointX, hdr_metadata_.referencePrimaries[6]);
	frameMeta->SetFloat(bmdDeckLinkFrameMetadataHDRWhitePointY, hdr_metadata_.referencePrimaries[7]);
	frameMeta->SetFloat(bmdDeckLinkFrameMetadataHDRMaxDisplayMasteringLuminance, hdr_metadata_.luminanceSettings[0]);
	frameMeta->SetFloat(bmdDeckLinkFrameMetadataHDRMinDisplayMasteringLuminance, hdr_metadata_.luminanceSettings[1]);
	frameMeta->SetFloat(bmdDeckLinkFrameMetadataHDRMaximumContentLightLevel, hdr_metadata_.luminanceSettings[2]);
	frameMeta->SetFloat(bmdDeckLinkFrameMetadataHDRMaximumFrameAverageLightLevel, hdr_metadata_.luminanceSettings[3]);
    frameMeta->Release();

}

void DecklinkOutput::receive_samples_from_xstudio(int16_t * samples, unsigned long num_samps) 
{
    // note this method is called by the xstudio audio output thread in a loop
    // that streams chunks of samples to an audio output device (i.e. this class)
    // The xstudio audio output actor expects us to return from here only when the
    // samples have been 'consumed'.
    {
        // lock mutex and immediately copy our samples to the buffer ready to
        // send to Decklink
        std::unique_lock lk2(audio_samples_buf_mutex_);
        const size_t buf_size = audio_samples_buffer_.size();
        audio_samples_buffer_.resize(buf_size+num_samps);
        memcpy(audio_samples_buffer_.data()+buf_size, samples, num_samps*sizeof(int16_t));
    }

    // now WAIT until the samples have been played (RenderAudioSamples is called
    // from a Decklink driver thread - we only want to return from THIS function
    // when Decklink needs a top-up of audio samples)
    std::unique_lock lk(audio_samples_cv_mutex_);
    audio_samples_cv_.wait(lk, [=] { return fetch_more_samples_from_xstudio_; });
    fetch_more_samples_from_xstudio_ = false;

}

long DecklinkOutput::num_samples_in_buffer() {

    // note this method is called by the xstudio audio output thread 
    // Have to assume that GetBufferedAudioSampleFrameCount is not thread safe. BMD SDK
    // does not tell us otherwise
    std::unique_lock lk0(bmd_mutex_);
    uint32_t prerollAudioSampleCount;
	if (decklink_output_interface_->GetBufferedAudioSampleFrameCount(&prerollAudioSampleCount) == S_OK) {
        return (long)prerollAudioSampleCount - (audio_sync_delay_milliseconds_*48000)/1000;
    }
    return 0;
}

// Note, I have not yet understood the significance of the preroll flag
void DecklinkOutput::copy_audio_samples_to_decklink_buffer(const bool /*preroll*/) 
{

    std::unique_lock lk0(bmd_mutex_);

    // How many samples are sitting on the SDI card ready to be played?
	uint32_t prerollAudioSampleCount;
	if (decklink_output_interface_->GetBufferedAudioSampleFrameCount(&prerollAudioSampleCount) == S_OK) {
        if (prerollAudioSampleCount > samples_water_level_) {
            // plenty of samples already in the bmd buffer ready to be played, 
            // let's do nothing here
            return;
        } else {
            // We need to top-up the samples in the buffer.

            // the xstudio audio output thread is probably waiting in
            // receive_samples_from_xstudio ... because the number of samples
            // in the BMD buffer is below our target 'water_level' we now 
            // release the lock in 'receive_samples_from_xstudio' so that the
            // xstudio audio sample streaming loop can continue and fetch 
            // more samples to give to us
            {
                std::lock_guard m(audio_samples_cv_mutex_);
                fetch_more_samples_from_xstudio_ = true;
            }
            audio_samples_cv_.notify_one();
        }
    }

    std::unique_lock lk(audio_samples_buf_mutex_);

    if (audio_samples_buffer_.empty())
    { 
        // 512 samples of silence to start filling buffer in the absence
        // of audio samples streaming from xstudio
        audio_samples_buffer_.resize(4096);
        memset(audio_samples_buffer_.data(), 0, audio_samples_buffer_.size()*sizeof(uint16_t));
    }
	
	if (decklink_output_interface_->ScheduleAudioSamples(
        audio_samples_buffer_.data(),
        audio_samples_buffer_.size()/2,
        samples_delivered_,
        bmdAudioSampleRate48kHz,
        nullptr) != S_OK) {
        throw std::runtime_error("Failed to shedule audio out.");
    }

    samples_delivered_ += audio_samples_buffer_.size()/2;
    audio_samples_buffer_.clear();

}

////////////////////////////////////////////
// Render Delegate Class
////////////////////////////////////////////
AVOutputCallback::AVOutputCallback (DecklinkOutput* pOwner)
{
	owner_ = pOwner;
}

HRESULT AVOutputCallback::QueryInterface (REFIID /*iid*/, LPVOID *ppv)
{
	*ppv = NULL;
	return E_NOINTERFACE;
}

ULONG AVOutputCallback::AddRef ()
{
	int		oldValue;

	oldValue = ref_count_.fetchAndAddAcquire(1);
	return (ULONG)(oldValue + 1);
}

ULONG AVOutputCallback::Release ()
{
	int		oldValue;

	oldValue = ref_count_.fetchAndAddAcquire(-1);
	if (oldValue == 1)
	{
		delete this;
	}

	return (ULONG)(oldValue - 1);
}

HRESULT	AVOutputCallback::ScheduledFrameCompleted (IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult /*result*/)
{
	owner_->fill_decklink_video_frame(completedFrame);
	return S_OK;
}

HRESULT	AVOutputCallback::ScheduledPlaybackHasStopped ()
{
	return S_OK;
}

HRESULT AVOutputCallback::RenderAudioSamples (BOOL preroll) {
    // decklink driver is calling this at regular intervals. There may be 
    // plenty of samples in the buffer for it to render, we check that in
    // our own function
	owner_->copy_audio_samples_to_decklink_buffer(preroll);
    return S_OK;

}
