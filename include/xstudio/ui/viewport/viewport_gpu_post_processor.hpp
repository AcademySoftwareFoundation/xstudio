// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/ui/viewport/enums.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        /* This class can be overridden to allow a plugin to intervene after
        the viewport draw/render call. Post render GPU draw routines can be
        executed to do some post-processing on the rendered viewport image.

        OpenGL:

        Note that when the callback is activated you will have a valid OpenGL
        context active and the viewport will be rendered into a texture with
        id 'tex_id' with associated framebuffer oject 'fbo_id' provided.

        To activate this behaviour, we instance our derived class and store
        in a ViewportFramePostProcessorPtr. This pointer is then sent as a message
        to the offscreen viewport.

        The basic framebuffer grabber is implemented by OffscreenViewport class.

        A reference implementation of this behaviour where GPU post processing
        is applies is also avaiable in the 'video_render' plugin.*/
        class ViewportFramePostProcessor {
          public:
            // This method can be overridden to execute custom code to
            // capture the viewport to an ImageBuffer in a specific way.
            // For example, the viewport may be rendered to a 16bti RGBA
            // target. One could implement a shader and re-render the
            // viewport texture to an RGB-10-10-10 buffer, say, and put
            // that data into destination_image. This could be used by
            // a video output plugin driving an SDI output card that needs
            // the buffers in specific pixel layouts and bit depths that
            // you would enact on the GPU rather than the CPU
            virtual void viewport_capture_gl_framebuffer(
                uint32_t tex_id,
                uint32_t fbo_id,
                const int fb_width,
                const int fb_height,
                const ImageFormat format,
                media_reader::ImageBufPtr &destination_image){};

            // This method can be overridden to execute custom GPU code (or
            // anything else) when an offscreen viewport re-draws. This is
            // applicable to a 'live' offscreen viewport that might feed
            // frames to an encoder, for example the NVIDIA Encode SDK, where
            // a GPU texture is required by the NVIDIA API.
            virtual void viewport_post_process_framebuffer(
                uint32_t tex_id,
                uint32_t fbo_id,
                const int fb_width,
                const int fb_height,
                const ImageFormat format,
                const media_reader::ImageBufDisplaySetPtr &onscreen_images,
                const Imath::M44f projection_matrix){};

            // Override this method to enact any cleanup of graphics resources
            // etc. The graphics context (OpenGL) will be valid when this
            // method is called.
            virtual void cleanup() {}

          protected:
            /*
            Helper function to provide re-useable video frames so that buffer
            allocation does not need to be repeated on every redraw.

            The assumption is that calls to viewport_capture_gl_framebuffer that
            happen on every refresh (for example for live video SDI output)
            need an image of a fixed size to download the viewport frame buffer
            into. Instead of allocating a new frame and it being destroyed
            when the ImageBufPtr goes out of scope after use, we re-use the
            image buffers by keeping a copy of the ImageBufPtr here.
            The allocation routine will pass if the buffer has already been
            allocated.
            */
            media_reader::ImageBufPtr get_video_output_frame() {

                media_reader::ImageBufPtr new_frame;
                for (auto &buf : output_buffers_) {
                    if (buf.use_count() == 1) {
                        // 'buf' is not referenced anywhere else in the
                        // application so we can re-use it.
                        new_frame = buf;
                        break;
                    }
                }
                if (!new_frame) {
                    new_frame.reset(new media_reader::ImageBuffer());
                    output_buffers_.push_back(new_frame);
                }
                return new_frame;
            }

          private:
            std::vector<media_reader::ImageBufPtr> output_buffers_;
        };

        typedef std::shared_ptr<ViewportFramePostProcessor> ViewportFramePostProcessorPtr;

    } // namespace viewport
} // namespace ui
} // namespace xstudio
