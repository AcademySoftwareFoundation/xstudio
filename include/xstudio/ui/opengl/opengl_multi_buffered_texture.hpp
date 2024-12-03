// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "opengl_texture_base.hpp"
#include "xstudio/media_reader/image_buffer_set.hpp"

namespace xstudio {
namespace ui {
    namespace opengl {

        class TextureTransferWorker;

        class GLDoubleBufferedTexture {

          public:

            typedef std::shared_ptr<GLBlindTex> GLBlindTexturePtr;

            template<class TexType>
            static GLDoubleBufferedTexture * create() {
              // we are creating 8 textures. This allows us to do asynchronous uploads,
              // so that some of the textures can start uploading pixel data for upcoming
              // redraws while others are being used to draw the current frame.
              // TODO: some rigorous testing for best number of textures and upload threads.
              // Also provide user preferences to manually tweak these settings if needed.
              auto result = new GLDoubleBufferedTexture();
              for (int i = 0; i < 48; ++i) {
                result->textures_.emplace_back(new TexType());
              }
              return result;
            }

            virtual ~GLDoubleBufferedTexture() = default;

            void bind(const media_reader::ImageBufPtr &image, int &tex_index, Imath::V2i &dims);

            void release(const media_reader::ImageBufPtr &image);

            void queue_image_set_for_upload(const media_reader::ImageBufDisplaySetPtr &images);

          private:

            GLDoubleBufferedTexture();

            GLBlindTexturePtr get_free_texture();

            class TexSet : public std::vector<GLBlindTexturePtr> {
              public:
              TexSet::iterator find(const media_reader::ImageBufPtr &image) {
                return std::find_if(begin(), end(), [=](const auto & t) {
                  return t->media_key() == image->media_key();
                });
              }
              TexSet::iterator find_pending(const media_reader::ImageBufPtr &image) {
                return std::find_if(begin(), end(), [=](const auto & t) {
                  return t->pending_media_key() == image->media_key();
                });
              }
            };

            void queue_for_upload(TexSet &available_textures, const media_reader::ImageBufPtr &image);

            TexSet textures_;

            std::deque<media_reader::ImageBufPtr> image_queue_;

            TextureTransferWorker * worker_;


        };
    } // namespace opengl
} // namespace ui
} // namespace xstudio

