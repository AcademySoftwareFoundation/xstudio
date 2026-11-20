// SPDX-License-Identifier: Apache-2.0

// xstudio headers first: the Apple headers further down define a 'nil' macro that
// breaks caf headers included after them.
#include "vt_decoder.hpp"
#include "vt_repack.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/utility/logging.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <set>

extern "C" {
#include <libavutil/pixdesc.h>
}

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>

namespace xstudio::media_reader::ffmpeg {

namespace {

    // VideoToolbox output formats used here (design D4), and the planar format each
    // is repacked to.
    constexpr OSType ks4as = kCVPixelFormatType_444YpCbCr16VideoRange_16A_TriPlanar; // 's4as'

    enum class Packing { Bits8, Bits10InMsbs, Bits16, S4as };

    struct OutputFormat {
        OSType vt;
        AVPixelFormat target;
        Packing packing;
        vt_repack::Chroma chroma;
        bool full_range;
    };

    using vt_repack::Chroma;
    const OutputFormat k_output_formats[] = {
        {kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
         AV_PIX_FMT_YUV420P,
         Packing::Bits8,
         Chroma::C420,
         false},
        {kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
         AV_PIX_FMT_YUV420P,
         Packing::Bits8,
         Chroma::C420,
         true},
        {kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange,
         AV_PIX_FMT_YUV422P,
         Packing::Bits8,
         Chroma::C422,
         false},
        {kCVPixelFormatType_422YpCbCr8BiPlanarFullRange,
         AV_PIX_FMT_YUV422P,
         Packing::Bits8,
         Chroma::C422,
         true},
        {kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange,
         AV_PIX_FMT_YUV444P,
         Packing::Bits8,
         Chroma::C444,
         false},
        {kCVPixelFormatType_444YpCbCr8BiPlanarFullRange,
         AV_PIX_FMT_YUV444P,
         Packing::Bits8,
         Chroma::C444,
         true},
        {kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange,
         AV_PIX_FMT_YUV420P10LE,
         Packing::Bits10InMsbs,
         Chroma::C420,
         false},
        {kCVPixelFormatType_420YpCbCr10BiPlanarFullRange,
         AV_PIX_FMT_YUV420P10LE,
         Packing::Bits10InMsbs,
         Chroma::C420,
         true},
        {kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange,
         AV_PIX_FMT_YUV422P10LE,
         Packing::Bits10InMsbs,
         Chroma::C422,
         false},
        {kCVPixelFormatType_422YpCbCr10BiPlanarFullRange,
         AV_PIX_FMT_YUV422P10LE,
         Packing::Bits10InMsbs,
         Chroma::C422,
         true},
        {kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange,
         AV_PIX_FMT_YUV444P10LE,
         Packing::Bits10InMsbs,
         Chroma::C444,
         false},
        {kCVPixelFormatType_444YpCbCr10BiPlanarFullRange,
         AV_PIX_FMT_YUV444P10LE,
         Packing::Bits10InMsbs,
         Chroma::C444,
         true},
        {kCVPixelFormatType_422YpCbCr16BiPlanarVideoRange,
         AV_PIX_FMT_YUV422P12LE,
         Packing::Bits16,
         Chroma::C422,
         false},
        {kCVPixelFormatType_444YpCbCr16BiPlanarVideoRange,
         AV_PIX_FMT_YUV444P12LE,
         Packing::Bits16,
         Chroma::C444,
         false},
        // the target here is for 4444 with alpha; without, it's YUV444P12LE
        {ks4as, AV_PIX_FMT_YUVA444P12LE, Packing::S4as, Chroma::C444, false},
    };

    const OutputFormat *output_format(const OSType vt) {
        for (const auto &f : k_output_formats)
            if (f.vt == vt)
                return &f;
        return nullptr;
    }

    // The VideoToolbox format for a bit depth, chroma format and range, 0 if none.
    OSType
    output_format_for(const Packing packing, const Chroma chroma, const bool full_range) {
        for (const auto &f : k_output_formats)
            if (f.packing == packing && f.chroma == chroma && f.full_range == full_range)
                return f.vt;
        return 0;
    }

    std::string fourcc_string(const uint32_t f) {
        std::string s;
        for (int shift = 24; shift >= 0; shift -= 8) {
            const char c = char((f >> shift) & 0xff);
            s += (c >= 32 && c < 127) ? c : '?';
        }
        return s;
    }

    // ProRes codec_tag ('apch' etc., stored little-endian by FFmpeg) to the
    // big-endian CMVideoCodecType VideoToolbox wants. 0 if it isn't a ProRes tag
    // VideoToolbox decodes.
    CMVideoCodecType prores_codec_type(const uint32_t tag) {
        const uint32_t t =
            ((tag & 0xff) << 24) | ((tag & 0xff00) << 8) | ((tag >> 8) & 0xff00) | (tag >> 24);
        switch (t) {
        case kCMVideoCodecType_AppleProRes422Proxy:
        case kCMVideoCodecType_AppleProRes422LT:
        case kCMVideoCodecType_AppleProRes422:
        case kCMVideoCodecType_AppleProRes422HQ:
        case kCMVideoCodecType_AppleProRes4444:
        case kCMVideoCodecType_AppleProRes4444XQ:
            return t;
        default:
            return 0;
        }
    }

    bool is_prores_4444(const CMVideoCodecType t) {
        return t == kCMVideoCodecType_AppleProRes4444 ||
               t == kCMVideoCodecType_AppleProRes4444XQ;
    }

    struct PictureFormat {
        int bit_depth = 0;
        int chroma    = 0; // 420, 422 or 444
    };

    // Bit depth and chroma format of an H.264 stream, from the avcC record. The
    // High profile extension that carries them is optional; without it the
    // profile tells us whether it can be anything but 8 bit 4:2:0.
    bool avcc_picture_format(const uint8_t *d, const int size, PictureFormat &fmt) {
        if (size < 7 || d[0] != 1)
            return false;
        const int profile = d[1];
        int pos           = 5;
        const int num_sps = d[pos++] & 0x1f;
        for (int i = 0; i < num_sps; ++i) {
            if (pos + 2 > size)
                return false;
            pos += 2 + ((d[pos] << 8) | d[pos + 1]);
        }
        if (pos + 1 > size)
            return false;
        const int num_pps = d[pos++];
        for (int i = 0; i < num_pps; ++i) {
            if (pos + 2 > size)
                return false;
            pos += 2 + ((d[pos] << 8) | d[pos + 1]);
        }
        if (pos > size)
            return false;

        const bool high_profile = profile == 100 || profile == 110 || profile == 122 ||
                                  profile == 144 || profile == 244;
        if (high_profile && pos + 3 <= size) {
            static const int chroma_formats[] = {400, 420, 422, 444};
            fmt.chroma                        = chroma_formats[d[pos] & 3];
            fmt.bit_depth                     = 8 + (d[pos + 1] & 7);
            return true;
        }
        if (profile == 100 || !high_profile) {
            // Baseline/Main/Extended, and High (not High 10/4:2:2/4:4:4), are
            // always 8 bit 4:2:0.
            fmt = {8, 420};
            return true;
        }
        return false;
    }

    // Bit depth and chroma format of an HEVC stream, from the hvcC record header.
    bool hvcc_picture_format(const uint8_t *d, const int size, PictureFormat &fmt) {
        if (size < 23 || d[0] != 1)
            return false;
        static const int chroma_formats[] = {400, 420, 422, 444};
        fmt.chroma                        = chroma_formats[d[16] & 3];
        fmt.bit_depth                     = 8 + (d[17] & 7);
        return true;
    }

    PictureFormat picture_format_from_pix_fmt(const int format) {
        const AVPixFmtDescriptor *d = av_pix_fmt_desc_get(AVPixelFormat(format));
        if (!d || (d->flags & AV_PIX_FMT_FLAG_RGB))
            return {};
        PictureFormat fmt;
        fmt.bit_depth = d->comp[0].depth;
        fmt.chroma    = d->log2_chroma_w == 0 ? 444 : (d->log2_chroma_h ? 420 : 422);
        return fmt;
    }

    struct Choice {
        CMVideoCodecType codec_type = 0;
        OSType output_format        = 0;
    };

    // Decides whether VideoToolbox should decode the stream, and with which output
    // format. Returns false with 'reason' set if not.
    bool choose(const AVStream *stream, Choice &choice, std::string &reason) {

        const AVCodecParameters *par = stream->codecpar;
        if (par->codec_type != AVMEDIA_TYPE_VIDEO) {
            reason = "not a video stream";
            return false;
        }
        if (par->width <= 0 || par->height <= 0) {
            reason = "unknown picture size";
            return false;
        }
        if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
            reason = "attached picture";
            return false;
        }

        const bool full_range = par->color_range == AVCOL_RANGE_JPEG;

        if (par->codec_id == AV_CODEC_ID_PRORES) {
            choice.codec_type = prores_codec_type(par->codec_tag);
            if (!choice.codec_type) {
                reason = "unrecognised ProRes codec tag";
                return false;
            }
            choice.output_format = is_prores_4444(choice.codec_type)
                                       ? ks4as
                                       : kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange;
            return true;
        }

        if (par->codec_id != AV_CODEC_ID_H264 && par->codec_id != AV_CODEC_ID_HEVC) {
            reason = std::string("codec ") + avcodec_get_name(par->codec_id) +
                     " is not decoded with VideoToolbox";
            return false;
        }

        // A length-prefixed stream carries an avcC/hvcC record (first byte 1). An
        // Annex-B stream (raw .h264/.ts) has start codes instead, or nothing.
        if (par->extradata_size < 7 || par->extradata[0] != 1) {
            reason = "no avcC/hvcC decoder configuration (Annex-B stream)";
            return false;
        }

        PictureFormat fmt;
        const bool parsed = par->codec_id == AV_CODEC_ID_H264
                                ? avcc_picture_format(par->extradata, par->extradata_size, fmt)
                                : hvcc_picture_format(par->extradata, par->extradata_size, fmt);
        if (!parsed)
            fmt = picture_format_from_pix_fmt(par->format);

        choice.codec_type =
            par->codec_id == AV_CODEC_ID_H264 ? kCMVideoCodecType_H264 : kCMVideoCodecType_HEVC;
        const char *codec = par->codec_id == AV_CODEC_ID_H264 ? "H.264" : "HEVC";

        Chroma chroma;
        switch (fmt.chroma) {
        case 420:
            chroma = Chroma::C420;
            break;
        case 422:
            chroma = Chroma::C422;
            break;
        case 444:
            chroma = Chroma::C444;
            break;
        default:
            reason =
                fmt::format("{} with chroma format {} (e.g. monochrome)", codec, fmt.chroma);
            return false;
        }

        if (fmt.bit_depth == 8)
            choice.output_format = output_format_for(Packing::Bits8, chroma, full_range);
        else if (fmt.bit_depth == 10)
            choice.output_format = output_format_for(Packing::Bits10InMsbs, chroma, full_range);
        else if (fmt.bit_depth == 12 && par->codec_id == AV_CODEC_ID_HEVC) {
            // VideoToolbox decodes these in software. Its 12 bit 4:2:0 chroma doesn't
            // match FFmpeg's and is further from the source (design D4, spike 1.8),
            // and there are no full range 16 bit formats to ask for.
            if (chroma == Chroma::C420) {
                reason = "12 bit 4:2:0 HEVC (VideoToolbox's chroma is inaccurate)";
                return false;
            }
            if (full_range) {
                reason = "full range 12 bit HEVC";
                return false;
            }
            choice.output_format = output_format_for(Packing::Bits16, chroma, false);
        }

        if (!choice.output_format) {
            reason = fmt::format("{} at {} bit {}", codec, fmt.bit_depth, fmt.chroma);
            return false;
        }
        return true;
    }

    // Does the first ProRes frame carry alpha? From the frame header, the same
    // way FFmpeg's decoder decides between yuv444p12 and yuva444p12.
    bool prores_frame_has_alpha(const AVPacket *pkt) {
        // 4 byte frame size, 'icpf', then the frame header; alpha_info is the low
        // nibble of header byte 17.
        if (!pkt || pkt->size < 8 + 18)
            return false;
        if (memcmp(pkt->data + 4, "icpf", 4))
            return false;
        return (pkt->data[8 + 17] & 0x0f) != 0;
    }

    AVPixelFormat target_pix_fmt(const OSType vt, const bool alpha) {
        const OutputFormat *f = output_format(vt);
        if (!f)
            return AV_PIX_FMT_NONE;
        if (f->packing == Packing::S4as && !alpha)
            return AV_PIX_FMT_YUV444P12LE;
        return f->target;
    }

    bool cf_equal(CFTypeRef value, CFStringRef key) {
        return value && CFGetTypeID(value) == CFStringGetTypeID() &&
               CFStringCompare((CFStringRef)value, key, 0) == kCFCompareEqualTo;
    }

    // Colour metadata VideoToolbox attached to a decoded buffer, for fields the
    // container left unspecified (design D6).
    void colour_from_attachments(CVPixelBufferRef pb, AVFrame *frame) {
        if (frame->colorspace == AVCOL_SPC_UNSPECIFIED) {
            CFTypeRef m = CVBufferCopyAttachment(pb, kCVImageBufferYCbCrMatrixKey, nullptr);
            if (cf_equal(m, kCVImageBufferYCbCrMatrix_ITU_R_709_2))
                frame->colorspace = AVCOL_SPC_BT709;
            else if (cf_equal(m, kCVImageBufferYCbCrMatrix_ITU_R_601_4))
                frame->colorspace = AVCOL_SPC_SMPTE170M;
            else if (cf_equal(m, kCVImageBufferYCbCrMatrix_ITU_R_2020))
                frame->colorspace = AVCOL_SPC_BT2020_NCL;
            else if (cf_equal(m, kCVImageBufferYCbCrMatrix_SMPTE_240M_1995))
                frame->colorspace = AVCOL_SPC_SMPTE240M;
            if (m)
                CFRelease(m);
        }
        if (frame->color_primaries == AVCOL_PRI_UNSPECIFIED) {
            CFTypeRef p = CVBufferCopyAttachment(pb, kCVImageBufferColorPrimariesKey, nullptr);
            if (cf_equal(p, kCVImageBufferColorPrimaries_ITU_R_709_2))
                frame->color_primaries = AVCOL_PRI_BT709;
            else if (cf_equal(p, kCVImageBufferColorPrimaries_ITU_R_2020))
                frame->color_primaries = AVCOL_PRI_BT2020;
            else if (cf_equal(p, kCVImageBufferColorPrimaries_P3_D65))
                frame->color_primaries = AVCOL_PRI_SMPTE432;
            else if (cf_equal(p, kCVImageBufferColorPrimaries_DCI_P3))
                frame->color_primaries = AVCOL_PRI_SMPTE431;
            else if (cf_equal(p, kCVImageBufferColorPrimaries_EBU_3213))
                frame->color_primaries = AVCOL_PRI_BT470BG;
            else if (cf_equal(p, kCVImageBufferColorPrimaries_SMPTE_C))
                frame->color_primaries = AVCOL_PRI_SMPTE170M;
            if (p)
                CFRelease(p);
        }
        if (frame->color_trc == AVCOL_TRC_UNSPECIFIED) {
            CFTypeRef t =
                CVBufferCopyAttachment(pb, kCVImageBufferTransferFunctionKey, nullptr);
            if (cf_equal(t, kCVImageBufferTransferFunction_ITU_R_709_2))
                frame->color_trc = AVCOL_TRC_BT709;
            else if (cf_equal(t, kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ))
                frame->color_trc = AVCOL_TRC_SMPTE2084;
            else if (cf_equal(t, kCVImageBufferTransferFunction_ITU_R_2100_HLG))
                frame->color_trc = AVCOL_TRC_ARIB_STD_B67;
            else if (cf_equal(t, kCVImageBufferTransferFunction_ITU_R_2020))
                frame->color_trc = AVCOL_TRC_BT2020_10;
            else if (cf_equal(t, kCVImageBufferTransferFunction_SMPTE_240M_1995))
                frame->color_trc = AVCOL_TRC_SMPTE240M;
            if (t)
                CFRelease(t);
        }
    }

    // Same ownership trick as setup_video_buffer() in ffmpeg_stream.cpp: the
    // AVBufferRef in frame->buf[0] owns a heap-allocated ImageBufPtr that keeps the
    // pixel memory alive and lets FFMpegStream take the ImageBuffer over.
    void image_buf_ptr_deleter(void *, uint8_t *data) { delete (ImageBufPtr *)data; }

} // namespace

struct VtDecoder::Impl {
    VTDecompressionSessionRef session  = nullptr;
    CMVideoFormatDescriptionRef format = nullptr;
    Choice choice;
    AVRational time_base = {1, 1};
    int width            = 0;
    int height           = 0;
    bool full_range      = false;
    bool has_alpha       = false;
    bool first_packet    = true;
    bool hardware        = false;
    AVColorSpace colorspace;
    AVColorPrimaries primaries;
    AVColorTransferCharacteristic trc;

    std::mutex mutex;
    // Presentation timestamps of packets submitted that VideoToolbox hasn't
    // called back for yet
    std::set<int64_t> pending;
    // Frames VideoToolbox has returned, by presentation timestamp
    std::map<int64_t, CVPixelBufferRef> decoded;
    // Highest decode timestamp submitted. Every later packet has pts >= its
    // dts >= this, so a decoded frame with pts <= max_dts can't be preceded by
    // anything still to come (design D8).
    int64_t max_dts = std::numeric_limits<int64_t>::min();
    bool draining   = false;

    ~Impl() {
        clear_decoded();
        if (session) {
            VTDecompressionSessionWaitForAsynchronousFrames(session);
            VTDecompressionSessionInvalidate(session);
            CFRelease(session);
        }
        if (format)
            CFRelease(format);
    }

    void clear_decoded() {
        std::lock_guard<std::mutex> l(mutex);
        for (auto &p : decoded)
            CVPixelBufferRelease(p.second);
        decoded.clear();
        pending.clear();
        max_dts = std::numeric_limits<int64_t>::min();
    }

    static void output_callback(
        void *refcon,
        void *source_frame_refcon,
        OSStatus status,
        VTDecodeInfoFlags flags,
        CVImageBufferRef image,
        CMTime,
        CMTime) {
        auto *self        = static_cast<Impl *>(refcon);
        const int64_t pts = int64_t(reinterpret_cast<intptr_t>(source_frame_refcon));
        std::lock_guard<std::mutex> l(self->mutex);
        self->pending.erase(pts);
        if (status != noErr || (flags & kVTDecodeInfo_FrameDropped) || !image) {
            // Undecodable (e.g. an open-GOP leading picture after a seek); it is
            // simply never output.
            return;
        }
        auto it = self->decoded.find(pts);
        if (it != self->decoded.end())
            CVPixelBufferRelease(it->second);
        self->decoded[pts] = CVPixelBufferRetain(image);
    }
};

VtDecoder::VtDecoder(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

VtDecoder::~VtDecoder() = default;

bool VtDecoder::eligible(const AVStream *stream, std::string &reason) {
    Choice choice;
    return choose(stream, choice, reason);
}

std::unique_ptr<VtDecoder> VtDecoder::create(const AVStream *stream, std::string &error) {

    auto impl = std::make_unique<Impl>();
    if (!choose(stream, impl->choice, error))
        return nullptr;

    // Debugging aid, and how the fallback/failure paths are tested: pretend
    // VideoToolbox can't create a session.
    if (getenv("XSTUDIO_VIDEOTOOLBOX_FORCE_FAILURE")) {
        error = "session creation disabled by XSTUDIO_VIDEOTOOLBOX_FORCE_FAILURE";
        return nullptr;
    }

    const AVCodecParameters *par = stream->codecpar;
    impl->time_base              = stream->time_base;
    impl->width                  = par->width;
    impl->height                 = par->height;
    impl->full_range             = par->color_range == AVCOL_RANGE_JPEG;
    impl->colorspace             = par->color_space;
    impl->primaries              = par->color_primaries;
    impl->trc                    = par->color_trc;
    // Until the first ProRes frame header says otherwise, 4444 is assumed to have alpha.
    impl->has_alpha = impl->choice.output_format == ks4as;

    CFMutableDictionaryRef extensions = CFDictionaryCreateMutable(
        nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (par->codec_id == AV_CODEC_ID_H264 || par->codec_id == AV_CODEC_ID_HEVC) {
        CFMutableDictionaryRef atoms = CFDictionaryCreateMutable(
            nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDataRef data = CFDataCreate(nullptr, par->extradata, par->extradata_size);
        CFDictionarySetValue(
            atoms, par->codec_id == AV_CODEC_ID_H264 ? CFSTR("avcC") : CFSTR("hvcC"), data);
        CFDictionarySetValue(
            extensions, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, atoms);
        CFRelease(data);
        CFRelease(atoms);
    }
    OSStatus status = CMVideoFormatDescriptionCreate(
        nullptr, impl->choice.codec_type, par->width, par->height, extensions, &impl->format);
    CFRelease(extensions);
    if (status != noErr) {
        error = fmt::format("CMVideoFormatDescriptionCreate failed ({})", int(status));
        return nullptr;
    }

    CFMutableDictionaryRef spec = CFDictionaryCreateMutable(
        nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    // Use hardware when there is some; VideoToolbox falls back to its own software
    // decoder (e.g. ProRes on Intel) otherwise.
    CFDictionarySetValue(
        spec,
        kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder,
        kCFBooleanTrue);

    CFMutableDictionaryRef attributes = CFDictionaryCreateMutable(
        nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const SInt32 pixel_format = SInt32(impl->choice.output_format);
    CFNumberRef number        = CFNumberCreate(nullptr, kCFNumberSInt32Type, &pixel_format);
    CFDictionarySetValue(attributes, kCVPixelBufferPixelFormatTypeKey, number);
    CFRelease(number);

    VTDecompressionOutputCallbackRecord callback{&Impl::output_callback, impl.get()};
    status = VTDecompressionSessionCreate(
        nullptr, impl->format, spec, attributes, &callback, &impl->session);
    CFRelease(spec);
    CFRelease(attributes);
    if (status != noErr) {
        impl->session = nullptr;
        error         = fmt::format("VTDecompressionSessionCreate failed ({})", int(status));
        return nullptr;
    }

    CFBooleanRef hw = nullptr;
    if (VTSessionCopyProperty(
            impl->session,
            kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder,
            nullptr,
            &hw) == noErr &&
        hw) {
        impl->hardware = hw == kCFBooleanTrue;
        CFRelease(hw);
    }

    return std::unique_ptr<VtDecoder>(new VtDecoder(std::move(impl)));
}

int VtDecoder::send_packet(const AVPacket *packet) {

    Impl &d = *impl_;

    if (!packet || !packet->size) {
        // Start draining: everything submitted so far comes out of receive_frame,
        // then AVERROR_EOF.
        VTDecompressionSessionFinishDelayedFrames(d.session);
        VTDecompressionSessionWaitForAsynchronousFrames(d.session);
        std::lock_guard<std::mutex> l(d.mutex);
        d.draining = true;
        return 0;
    }

    {
        std::lock_guard<std::mutex> l(d.mutex);
        if (d.draining)
            return AVERROR_EOF;
    }

    // Reordering relies on every packet having both timestamps (D8).
    if (packet->pts == AV_NOPTS_VALUE || packet->dts == AV_NOPTS_VALUE)
        return AVERROR(ENOTSUP);

    if (d.first_packet) {
        d.first_packet = false;
        if (d.choice.output_format == ks4as)
            d.has_alpha = prores_frame_has_alpha(packet);
    }

    // Wrap the packet data without copying: decoding is synchronous (D9), so it is
    // finished with before this returns.
    CMBlockBufferRef block = nullptr;
    OSStatus status        = CMBlockBufferCreateWithMemoryBlock(
        nullptr,
        packet->data,
        size_t(packet->size),
        kCFAllocatorNull,
        nullptr,
        0,
        size_t(packet->size),
        0,
        &block);
    if (status != noErr)
        return AVERROR(ENOMEM);

    const AVRational tb = d.time_base;
    CMSampleTimingInfo timing;
    timing.duration =
        packet->duration > 0 ? CMTimeMake(packet->duration * tb.num, tb.den) : kCMTimeInvalid;
    timing.presentationTimeStamp = CMTimeMake(packet->pts * tb.num, tb.den);
    timing.decodeTimeStamp       = CMTimeMake(packet->dts * tb.num, tb.den);
    const size_t sample_size     = size_t(packet->size);

    CMSampleBufferRef sample = nullptr;
    status                   = CMSampleBufferCreateReady(
        nullptr, block, d.format, 1, 1, &timing, 1, &sample_size, &sample);
    CFRelease(block);
    if (status != noErr)
        return AVERROR(ENOMEM);

    {
        std::lock_guard<std::mutex> l(d.mutex);
        d.pending.insert(packet->pts);
        d.max_dts = std::max(d.max_dts, packet->dts);
    }

    VTDecodeInfoFlags info = 0;
    status                 = VTDecompressionSessionDecodeFrame(
        d.session,
        sample,
        0, // synchronous
        reinterpret_cast<void *>(intptr_t(packet->pts)),
        &info);
    CFRelease(sample);

    if (status != noErr) {
        // VideoToolbox may or may not have called back with the error; either way
        // this frame won't be output.
        std::lock_guard<std::mutex> l(d.mutex);
        d.pending.erase(packet->pts);
        if (status == kVTInvalidSessionErr)
            return AVERROR_EXTERNAL;
        spdlog::debug(
            "VideoToolbox could not decode packet at pts {}: {}", packet->pts, int(status));
    }
    return 0;
}

int VtDecoder::receive_frame(AVFrame *frame) {

    Impl &d             = *impl_;
    CVPixelBufferRef pb = nullptr;
    int64_t pts         = 0;
    {
        std::lock_guard<std::mutex> l(d.mutex);
        if (d.decoded.empty())
            return d.draining ? AVERROR_EOF : AVERROR(EAGAIN);

        // The earliest decoded frame can go out once no frame with an earlier
        // timestamp can still turn up: none is in flight, and no packet yet to be
        // sent can have one (pts <= max_dts).
        auto first = d.decoded.begin();
        if (!d.draining && (first->first > d.max_dts ||
                            (!d.pending.empty() && first->first > *d.pending.begin())))
            return AVERROR(EAGAIN);

        pts = first->first;
        pb  = first->second;
        d.decoded.erase(first);
    }

    const AVPixelFormat target     = target_pix_fmt(d.choice.output_format, d.has_alpha);
    const int w                    = int(CVPixelBufferGetWidth(pb));
    const int h                    = int(CVPixelBufferGetHeight(pb));
    const vt_repack::Layout layout = vt_repack::planar_layout(target, w, h);

    ImageBufPtr image(new ImageBuffer());
    auto *base = reinterpret_cast<uint8_t *>(image->allocate(layout.total_size + 4096));

    auto dst = [&](const int i) -> vt_repack::DstPlane {
        return {base + layout.offsets[i], layout.linesizes[i]};
    };

    CVPixelBufferLockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
    auto src = [&](const size_t i) -> vt_repack::SrcPlane {
        return {
            static_cast<const uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(pb, i)),
            CVPixelBufferGetBytesPerRowOfPlane(pb, i)};
    };

    const OSType got       = CVPixelBufferGetPixelFormatType(pb);
    const OutputFormat *of = got == d.choice.output_format ? output_format(got) : nullptr;
    const bool ok          = of != nullptr;
    if (of) {
        switch (of->packing) {
        case Packing::Bits8:
            vt_repack::biplanar8_to_planar8(
                src(0), src(1), dst(0), dst(1), dst(2), w, h, of->chroma);
            break;
        case Packing::Bits10InMsbs:
            vt_repack::biplanar10_to_planar10(
                src(0), src(1), dst(0), dst(1), dst(2), w, h, of->chroma);
            break;
        case Packing::Bits16:
            vt_repack::biplanar16_to_planar12(
                src(0), src(1), dst(0), dst(1), dst(2), w, h, of->chroma);
            break;
        case Packing::S4as:
            vt_repack::s4as_to_yuva444p12(
                src(0),
                src(1),
                src(2),
                dst(0),
                dst(1),
                dst(2),
                d.has_alpha ? dst(3) : vt_repack::DstPlane{nullptr, 0},
                w,
                h);
            break;
        }
    }

    if (ok) {
        av_frame_unref(frame);
        frame->format                = target;
        frame->width                 = w;
        frame->height                = h;
        frame->pts                   = pts;
        frame->best_effort_timestamp = pts;
        frame->color_range           = of->full_range ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;
        frame->colorspace            = d.colorspace;
        frame->color_primaries       = d.primaries;
        frame->color_trc             = d.trc;
        colour_from_attachments(pb, frame);

        for (int i = 0; i < layout.planes; ++i) {
            frame->data[i]     = base + layout.offsets[i];
            frame->linesize[i] = int(layout.linesizes[i]);
        }
        frame->extended_data = frame->data;
        frame->buf[0]        = av_buffer_create(
            reinterpret_cast<uint8_t *>(new ImageBufPtr(image)),
            sizeof(ImageBufPtr),
            image_buf_ptr_deleter,
            nullptr,
            0);
    }

    CVPixelBufferUnlockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
    CVPixelBufferRelease(pb);

    if (!ok) {
        spdlog::warn(
            "VideoToolbox returned pixel format {}, expected {}",
            fourcc_string(got),
            fourcc_string(d.choice.output_format));
        return AVERROR_EXTERNAL;
    }
    return 0;
}

void VtDecoder::flush() {
    if (impl_->session)
        VTDecompressionSessionWaitForAsynchronousFrames(impl_->session);
    impl_->clear_decoded();
    std::lock_guard<std::mutex> l(impl_->mutex);
    impl_->draining = false;
}

AVPixelFormat VtDecoder::output_pix_fmt() const {
    return target_pix_fmt(impl_->choice.output_format, impl_->has_alpha);
}

std::string VtDecoder::output_format_name() const {
    return fourcc_string(impl_->choice.output_format);
}

bool VtDecoder::using_hardware() const { return impl_->hardware; }

} // namespace xstudio::media_reader::ffmpeg
