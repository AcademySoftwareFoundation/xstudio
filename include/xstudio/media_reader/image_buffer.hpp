// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/buffer.hpp"
#include "xstudio/media_reader/audio_buffer.hpp"
#include "xstudio/media_reader/pixel_info.hpp"
#include "xstudio/ui/viewport/shader.hpp"
#include "xstudio/colour_pipeline/colour_pipeline.hpp"

namespace xstudio {
namespace media_reader {

    class ImageBuffer : public Buffer {
      public:
        ImageBuffer(
            const utility::Uuid &uuid               = utility::Uuid(),
            const utility::JsonStore &shader_params = utility::JsonStore(),
            const utility::JsonStore &params        = utility::JsonStore())
            : Buffer(params), shader_id_(uuid), shader_params_(shader_params) {}

        ImageBuffer(const std::string &error_message) : Buffer(error_message) {
            // provide fallback image size for error image of 16:9
            set_image_dimensions(Imath::V2i(1920, 1080));
        }

        ~ImageBuffer() override = default;

        byte *allocate(const size_t size) override;

        void set_shader(const ui::viewport::GPUShaderPtr &shader) { shader_ = shader; }
        [[nodiscard]] ui::viewport::GPUShaderPtr shader() const { return shader_; }

        void set_shader_params(const utility::JsonStore &params) { shader_params_ = params; }
        [[nodiscard]] const utility::JsonStore &shader_params() const { return shader_params_; }

        void set_metadata(const utility::JsonStore &metadata) { metadata_ = metadata; }
        [[nodiscard]] const utility::JsonStore &metadata() const { return metadata_; }

        [[nodiscard]] Imath::V2i image_size_in_pixels() const { return image_size_in_pixels_; }
        [[nodiscard]] Imath::Box2i image_pixels_bounding_box() const { return pixels_bounds_; }
        void set_image_dimensions(
            const Imath::V2i pix_size, const Imath::Box2i bounds = Imath::Box2i()) {
            image_size_in_pixels_ = pix_size;
            if (bounds.isEmpty()) {
                pixels_bounds_ = Imath::Box2i(Imath::V2i(0, 0), pix_size);
            } else {
                pixels_bounds_ = bounds;
            }
        }

        [[nodiscard]] const media::MediaKey &media_key() const { return media_key_; }
        void set_media_key(const media::MediaKey &key) { media_key_ = key; }

        [[nodiscard]] int decoder_frame_number() const { return frame_num_; }
        void set_decoder_frame_number(const int f) { frame_num_ = f; }

        [[nodiscard]] bool has_alpha() const { return has_alpha_; }
        void set_has_alpha(const bool b) { has_alpha_ = b; }

        typedef std::function<PixelInfo(
            const ImageBuffer &buf,
            const Imath::V2i &pixel_location,
            const std::vector<Imath::V2i> &extra_pixel_locationss)>
            PixelPickerFunc;
        void set_pixel_picker_func(PixelPickerFunc func) { pixel_picker_ = func; }

        PixelInfo pixel_info(
            const Imath::V2i &pixel_location,
            const std::vector<Imath::V2i> &extra_pixel_locationss =
                std::vector<Imath::V2i>()) const {
            if (pixel_picker_)
                return pixel_picker_(*this, pixel_location, extra_pixel_locationss);
            return PixelInfo(pixel_location);
        }

      private:
        utility::Uuid shader_id_;
        utility::JsonStore shader_params_;
        utility::JsonStore metadata_;
        Imath::V2i image_size_in_pixels_;
        Imath::Box2i pixels_bounds_;
        media::MediaKey media_key_;
        int frame_num_ = -1;
        ui::viewport::GPUShaderPtr shader_;
        PixelPickerFunc pixel_picker_;
        bool has_alpha_ = false;
    };

    /* Extending std::shared_ptr<ImageBuffer> by adding a pointer to colour pipe
    data allows us to easily 'attach' colour management data to an image buffer.
    This is particularly useful as ImageBufPtrs are rapidly passed from the cache,
    to the playhead, to the viewer during playback alongside color data */
    class ImageBufPtr : public std::shared_ptr<ImageBuffer> {
      public:
        using Base = std::shared_ptr<ImageBuffer>;

        ImageBufPtr() = default;
        ImageBufPtr(ImageBuffer *imbuf) : Base(imbuf) {}
        ImageBufPtr(const ImageBufPtr &o)
            : Base(static_cast<const Base &>(o)),
              colour_pipe_data_(o.colour_pipe_data_),
              colour_pipe_uniforms_(o.colour_pipe_uniforms_),
              when_to_display_(o.when_to_display_),
              plugin_blind_data_(o.plugin_blind_data_),
              tts_(o.tts_),
              frame_id_(o.frame_id_),
              bookmarks_(o.bookmarks_),
              intrinsic_transform_(o.intrinsic_transform_),
              layout_transform_(o.layout_transform_),
              playhead_logical_frame_(o.playhead_logical_frame_),
              playhead_logical_duration_(o.playhead_logical_duration_),
              error_details_(o.error_details_) {}

        ImageBufPtr &operator=(const ImageBufPtr &o) {
            Base &b                    = static_cast<Base &>(*this);
            b                          = static_cast<const Base &>(o);
            colour_pipe_data_          = o.colour_pipe_data_;
            colour_pipe_uniforms_      = o.colour_pipe_uniforms_;
            when_to_display_           = o.when_to_display_;
            plugin_blind_data_         = o.plugin_blind_data_;
            tts_                       = o.tts_;
            frame_id_                  = o.frame_id_;
            bookmarks_                 = o.bookmarks_;
            intrinsic_transform_       = o.intrinsic_transform_;
            layout_transform_          = o.layout_transform_;
            playhead_logical_frame_    = o.playhead_logical_frame_;
            playhead_logical_duration_ = o.playhead_logical_duration_;
            error_details_             = o.error_details_;
            return *this;
        }

        ~ImageBufPtr() = default;

        bool operator==(const ImageBufPtr &o) const {
            if (this->get() != o.get()) {
                return false;
            }

            if (colour_pipe_data_ && o.colour_pipe_data_) {
                if (colour_pipe_data_->cache_id() != o.colour_pipe_data_->cache_id()) {
                    return false;
                }
            } else if (colour_pipe_data_ || o.colour_pipe_data_) {
                return false;
            }

            if (tts_ != o.tts_) {
                return false;
            }

            if (colour_pipe_uniforms_ != o.colour_pipe_uniforms_) {
                return false;
            }

            if (bookmarks_ != o.bookmarks_) {
                return false;
            }

            return true;
        }

        bool operator<(const ImageBufPtr &o) const { return tts_ < o.tts_; }

        bool operator<(const utility::time_point &t) const { return when_to_display_ < t; }

        bool operator<(const timebase::flicks &t) const { return tts_ < t; }

        bool operator>(const timebase::flicks &t) const { return tts_ > t; }

        colour_pipeline::ColourPipelineDataPtr colour_pipe_data_;
        utility::JsonStore colour_pipe_uniforms_;

        utility::time_point when_to_display_;

        // TODO: drop add_plugin_blind_data when all plugins are using
        // of add_plugin_blind_data2 instead
        void add_plugin_blind_data(
            const utility::Uuid &plugin_uuid, const utility::BlindDataObjectPtr &data) {
            plugin_blind_data_[plugin_uuid] = data;
        }

        [[nodiscard]] utility::BlindDataObjectPtr
        plugin_blind_data(const utility::Uuid plugin_uuid) const {
            auto p = plugin_blind_data_.find(plugin_uuid);
            if (p != plugin_blind_data_.end())
                return p->second;
            return utility::BlindDataObjectPtr();
        }

        template <typename T>
        [[nodiscard]] const T *plugin_blind_data(const utility::Uuid &plugin_uuid) const {
            auto p = plugin_blind_data_.find(plugin_uuid);
            if (p != plugin_blind_data_.end())
                return dynamic_cast<T *>(p->second.get());
            return nullptr;
        }

        std::map<utility::Uuid, utility::BlindDataObjectPtr> plugin_blind_data_;

        [[nodiscard]] const timebase::flicks &timeline_timestamp() const { return tts_; }
        void set_timline_timestamp(const timebase::flicks tts) { tts_ = tts; }

        [[nodiscard]] const int &playhead_logical_frame() const {
            return playhead_logical_frame_;
        }
        void set_playhead_logical_frame(const int frame) { playhead_logical_frame_ = frame; }

        [[nodiscard]] const int &playhead_logical_duration() const {
            return playhead_logical_duration_;
        }
        void set_playhead_logical_duration(const int frame) {
            playhead_logical_duration_ = frame;
        }

        [[nodiscard]] const bookmark::BookmarkAndAnnotations &bookmarks() const {
            return bookmarks_;
        }
        void set_bookmarks(const bookmark::BookmarkAndAnnotations &bookmarks) {
            bookmarks_ = bookmarks;
        }

        [[nodiscard]] const media::AVFrameID &frame_id() const { return frame_id_; }
        void set_frame_id(const media::AVFrameID &id) { frame_id_ = id; }

        [[nodiscard]] const Imath::M44f &intrinsic_transform() const {
            return intrinsic_transform_;
        }
        void set_intrinsic_transform(const Imath::M44f &t) { intrinsic_transform_ = t; }

        [[nodiscard]] const Imath::M44f layout_transform() const {
            return frame_id_.transform_matrix() * layout_transform_;
        }
        void set_layout_transform(const Imath::M44f &t) { layout_transform_ = t; }

        [[nodiscard]] const std::string &error_details() const { return error_details_; }
        void set_error_details(const std::string &err) { error_details_ = err; }

        friend float image_aspect(const ImageBufPtr &value);

        friend float image_layout_aspect(const ImageBufPtr &value);

        utility::JsonStore metadata() const;

      private:
        Imath::M44f intrinsic_transform_;
        Imath::M44f layout_transform_;

        std::string error_details_;

        timebase::flicks tts_ = timebase::flicks{0};
        media::AVFrameID frame_id_;
        bookmark::BookmarkAndAnnotations bookmarks_;
        int playhead_logical_frame_    = 0;
        int playhead_logical_duration_ = 0;
    };

    inline float image_aspect(const ImageBufPtr &v) {

        return v ? v->image_size_in_pixels().y
                       ? v.frame_id_.pixel_aspect() * v->image_size_in_pixels().x /
                             v->image_size_in_pixels().y
                       : 16.0f / 9.0f
                 : 16.0f / 9.0f;
    }

    inline float image_layout_aspect(const ImageBufPtr &v) {

        // This is used to work out the image aspect *after* the image layout
        // transform is applied. Thus if an image is rotated 90 degrees, the
        // aspect returned here reflects the rotation so that the image can
        // be width/height 'fitted' into the viewport correct etc.

        float af = v ? v->image_size_in_pixels().y
                           ? v.frame_id_.pixel_aspect() * v->image_size_in_pixels().x /
                                 v->image_size_in_pixels().y
                           : 16.0f / 9.0f
                     : 16.0f / 9.0f;

        if (v.layout_transform() != Imath::M44f()) {

            // the layout_aspect_ is used to apply Width/Height/Best 'Fit' modes by
            // the xSTUDIO viewport. But what if the image is rotated? We need to
            // account for rotation here so that fit modes still work

            // these are the 4 corners of the image:
            std::array<Imath::V4f, 4> corners = {
                Imath::V4f(-1.0f, -1.0f / af, 0.0f, 1.0f),
                Imath::V4f(1.0f, -1.0f / af, 0.0f, 1.0f),
                Imath::V4f(1.0f, 1.0f / af, 0.0f, 1.0f),
                Imath::V4f(-1.0f, 1.0f / af, 0.0f, 1.0f)};

            // transform the image corners, and then get the bounding rectangle
            // for the transformed corners
            float y_min = std::numeric_limits<float>::max();
            float y_max = std::numeric_limits<float>::min();
            float x_min = std::numeric_limits<float>::max();
            float x_max = std::numeric_limits<float>::min();
            for (int i = 0; i < 4; ++i) {
                corners[i] *= v.layout_transform();
                if (corners[i].w)
                    corners[i] *= 1.0f / corners[i].w;
                y_min = std::min(y_min, corners[i].y);
                y_max = std::max(y_max, corners[i].y);
                x_min = std::min(x_min, corners[i].x);
                x_max = std::max(x_max, corners[i].x);
            }

            // use the image cor
            if (x_max != x_min && y_min != y_max) {
                af = (x_max - x_min) / (y_max - y_min);
            }
        }

        return af;
    }


} // namespace media_reader
} // namespace xstudio
