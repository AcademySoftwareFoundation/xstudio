// SPDX-License-Identifier: Apache-2.0
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#undef RGB

namespace xstudio {
namespace media_reader {

    class SimpleExrSampler {
      public:
        SimpleExrSampler(ImageBufPtr exr_buf, thumbnail::ThumbnailBufferPtr &buf)
            : exr_buf_(exr_buf), thumbuf_(buf) {
            exr_size     = exr_buf->image_size_in_pixels();
            exr_data_win = exr_buf->image_pixels_bounding_box();
            exr_chans    = exr_buf->shader_params()["num_channels"].get<int>();
            // Check only for the bitness of the R channel since RGB will be similar
            pix_type            = exr_buf->shader_params()["pix_type_r"].get<int>();
            exr_bytes_per_pixel = exr_buf->shader_params()["bytes_per_pixel"].get<int>();

            exr_bytes_per_line =
                (exr_data_win.max.x - exr_data_win.min.x) * exr_bytes_per_pixel;

            xscale = float(exr_size.x) / float(thumbuf_->width());
            yscale = float(exr_size.y) / float(thumbuf_->height());
        }

        ImageBufPtr exr_buf_;
        thumbnail::ThumbnailBufferPtr thumbuf_;
        Imath::V2i exr_size;
        Imath::Box2i exr_data_win;
        int exr_chans, pix_type;
        int exr_bytes_per_pixel, exr_bytes_per_line;
        float xscale, yscale;

        struct RGB {

            RGB() : r(0), g(0), b(0) {}
            RGB(const float l) {
                r = uint8_t(
                    round(std::max(std::min(pow(l, 1.0f / 2.2f) * 255.0f, 255.0f), 0.0f)));
                g = r;
                b = r;
            }

            RGB(const float _r, const float _g, const float _b) {
                r = uint8_t(
                    round(std::max(std::min(pow(_r, 1.0f / 2.2f) * 255.0f, 255.0f), 0.0f)));
                g = uint8_t(
                    round(std::max(std::min(pow(_g, 1.0f / 2.2f) * 255.0f, 255.0f), 0.0f)));
                b = uint8_t(
                    round(std::max(std::min(pow(_b, 1.0f / 2.2f) * 255.0f, 255.0f), 0.0f)));
            }

            RGB(const RGB &o) = default;

            uint8_t r, g, b;
        };

        inline RGB sample_16bit_exr(const int x, const int y) const {

            if (x < exr_data_win.min.x || x >= exr_data_win.max.x || y < exr_data_win.min.y ||
                y >= exr_data_win.max.y)
                return RGB();

            half * pix = (half *)(exr_buf_->buffer() + (x-exr_data_win.min.x)*exr_bytes_per_pixel + (y-exr_data_win.min.y)*exr_bytes_per_line);
            if (exr_chans <= 2) {
                return RGB(pix[0]);
            } else {
                return RGB(pix[0], pix[1], pix[2]);
            }
        }

        inline std::array<float, 3> sample_16bit_exr_float(const int x, const int y) const {
            if (x < exr_data_win.min.x || x >= exr_data_win.max.x || y < exr_data_win.min.y ||
                y >= exr_data_win.max.y)
                return std::array<float, 3>({0.0f, 0.0f, 0.0f});

            half * pix = (half *)(exr_buf_->buffer() + (x-exr_data_win.min.x)*exr_bytes_per_pixel + (y-exr_data_win.min.y)*exr_bytes_per_line);
            if (exr_chans <= 2)
                return std::array<float, 3>({pix[0], pix[0], pix[0]});

            return std::array<float, 3>({pix[0], pix[1], pix[2]});
        }

        inline RGB sample_32bit_exr(const int x, const int y) const {

            if (x < exr_data_win.min.x || x >= exr_data_win.max.x || y < exr_data_win.min.y ||
                y >= exr_data_win.max.y)
                return RGB();

            float * pix = (float *)(exr_buf_->buffer()
                + (x-exr_data_win.min.x)*exr_bytes_per_pixel
                + (y-exr_data_win.min.y)*exr_bytes_per_line
                );
            if (exr_chans <= 2) {
                return RGB(pix[0]);
            } else {
                return RGB(pix[0], pix[1], pix[2]);
            }
        }

        inline std::array<float, 3> sample_32bit_exr_float(const int x, const int y) const {

            if (x < exr_data_win.min.x || x >= exr_data_win.max.x || y < exr_data_win.min.y ||
                y >= exr_data_win.max.y)
                return std::array<float, 3>();

            float * pix = (float *)(exr_buf_->buffer()
                + (x-exr_data_win.min.x)*exr_bytes_per_pixel
                + (y-exr_data_win.min.y)*exr_bytes_per_line
                );
            if (exr_chans <= 2)
                return std::array<float, 3>({pix[0], pix[0], pix[0]});


            return std::array<float, 3>({pix[0], pix[1], pix[2]});
        }

        void fill_output() {
            if (thumbuf_->format() == thumbnail::TF_RGBF96) {
                auto buf = reinterpret_cast<float *>(thumbuf_->data().data());

                if (pix_type == Imf::PixelType::HALF) {

                    for (size_t ty = 0; ty < thumbuf_->height(); ty++) {
                        for (size_t tx = 0; tx < thumbuf_->width(); tx++) {

                            auto c = sample_16bit_exr_float(
                                (int)round(float(tx) * xscale), (int)round(float(ty) * yscale));
                            memcpy(buf, c.data(), 3 * sizeof(float));
                            buf += 3;
                        }
                    }

                } else if (pix_type == Imf::PixelType::FLOAT) {
                    for (size_t ty = 0; ty < thumbuf_->height(); ty++) {
                        for (size_t tx = 0; tx < thumbuf_->width(); tx++) {

                            auto c = sample_32bit_exr_float(
                                (int)round(float(tx) * xscale), (int)round(float(ty) * yscale));
                            memcpy(buf, c.data(), 3 * sizeof(float));
                            buf += 3;
                        }
                    }
                }
            } else if (thumbuf_->format() == thumbnail::TF_RGB24) {
                auto buf = reinterpret_cast<uint8_t *>(thumbuf_->data().data());

                if (pix_type == Imf::PixelType::HALF) {
                    for (size_t ty = 0; ty < thumbuf_->height(); ty++) {
                        for (size_t tx = 0; tx < thumbuf_->width(); tx++) {

                            RGB c = sample_16bit_exr(
                                (int)round(float(tx) * xscale), (int)round(float(ty) * yscale));
                            memcpy(buf, &c, 3);
                            buf += 3;
                        }
                    }
                } else if (pix_type == Imf::PixelType::FLOAT) {
                    for (size_t ty = 0; ty < thumbuf_->height(); ty++) {
                        for (size_t tx = 0; tx < thumbuf_->width(); tx++) {

                            RGB c = sample_32bit_exr(
                                (int)round(float(tx) * xscale), (int)round(float(ty) * yscale));
                            memcpy(buf, &c, 3);
                            buf += 3;
                        }
                    }
                }
            }
        }
    };

} // namespace media_reader
} // namespace xstudio
