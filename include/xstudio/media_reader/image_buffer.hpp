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
            const utility::JsonStore &pixel_unpack_uniforms,
            const Imath::V2i &pixel_location,
            const std::vector<Imath::V2i> &extra_pixel_locations)>
            PixelPickerFunc;

        void set_pixel_picker_func(PixelPickerFunc func) { pixel_picker_ = func; }

        PixelInfo pixel_info(
            const Imath::V2i &pixel_location,
            const utility::JsonStore &pixel_unpack_uniforms,
            const std::vector<Imath::V2i> &extra_pixel_locationss =
                std::vector<Imath::V2i>()) const {
            if (pixel_picker_)
                return pixel_picker_(
                    *this, pixel_unpack_uniforms, pixel_location, extra_pixel_locationss);
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

    /* ImageBuffer is used to store the decoded image (single frame) data in
    the xSTUDIO cache. There is other data that is required to display the
    given image, like colour management data (luts, shaders, uniforms), image
    metadata, information about the corresponding timeline frame for the image
    and so-on. Most of this image sidecar data is transient, and can change
    depending on the context for viewing the image and other xSTUDIO session
    state such as bookmark data that is required when drawing the image to
    screen (for example, Annotations stoke and caption data). As such, we
    combine the ImageBuffer and ImageSidecar data in a single object using
    shared pointers. Since the ImageBufPtr object is passed around with high
    frequency in the playback engine during playback we want to minimise the
    overhead of copying the objects */

    class ImageBufPtr : public std::shared_ptr<ImageBuffer> {
      public:
        using Base = std::shared_ptr<ImageBuffer>;

        ImageBufPtr() : transient_data_(std::make_shared<SidecarData>()) {}
        ImageBufPtr(ImageBuffer *imbuf)
            : Base(imbuf), transient_data_(std::make_shared<SidecarData>()) {}
        ImageBufPtr(const ImageBufPtr &o) : Base(static_cast<const Base &>(o)) {
            transient_data_ = o.transient_data_;

            // Note - this debug output is telling me that ImageBufPtrs are
            // being duplicated 100s of times for every viewport redraw!!
            // Need to investigate as this is clearly inefficient, there is
            // some overkill when we prepare the ImageBufPtr for display
            // in the Viewport etc.
            /*mmm.lock();
            std::cerr << copy_count++ << " " << use_count() << "   ";
            mmm.unlock();*/
        }

        ImageBufPtr &operator=(const ImageBufPtr &o) {
            Base &b         = static_cast<Base &>(*this);
            b               = static_cast<const Base &>(o);
            transient_data_ = o.transient_data_;

            /*mmm.lock();
            std::cerr << copy_count++ << " " << use_count() << "   ";
            mmm.unlock();*/

            return *this;
        }

        ~ImageBufPtr() = default;

        static std::mutex mmm;
        static int copy_count;
        static int t_copy_count;

        bool operator==(const ImageBufPtr &o) const {
            if (this->get() != o.get()) {
                return false;
            }

            if (colour_pipe_data() && o.colour_pipe_data()) {
                if (colour_pipe_data()->cache_id() != o.colour_pipe_data()->cache_id()) {
                    return false;
                }
            } else if (colour_pipe_data() || o.colour_pipe_data()) {
                return false;
            }

            if (timeline_timestamp() != o.timeline_timestamp()) {
                return false;
            }

            if (colour_pipe_uniforms() != o.colour_pipe_uniforms()) {
                return false;
            }

            if (bookmarks() != o.bookmarks()) {
                return false;
            }

            return true;
        }

        bool operator<(const ImageBufPtr &o) const {
            return transient_data().tts_ < o.timeline_timestamp();
        }

        bool operator<(const utility::time_point &t) const {
            return transient_data().when_to_display_ < t;
        }

        bool operator<(const timebase::flicks &t) const { return transient_data().tts_ < t; }

        bool operator>(const timebase::flicks &t) const { return transient_data().tts_ > t; }

        // TODO: drop add_plugin_blind_data when all plugins are using
        // of add_plugin_blind_data2 instead
        void add_plugin_blind_data(
            const utility::Uuid &plugin_uuid, const utility::BlindDataObjectPtr &data) {
            transient_data().plugin_blind_data_[plugin_uuid] = data;
        }

        [[nodiscard]] utility::BlindDataObjectPtr
        plugin_blind_data(const utility::Uuid plugin_uuid) const {
            auto p = transient_data().plugin_blind_data_.find(plugin_uuid);
            if (p != transient_data().plugin_blind_data_.end())
                return p->second;
            return utility::BlindDataObjectPtr();
        }

        template <typename T>
        [[nodiscard]] const T *plugin_blind_data(const utility::Uuid &plugin_uuid) const {
            auto p = transient_data().plugin_blind_data_.find(plugin_uuid);
            if (p != transient_data().plugin_blind_data_.end())
                return dynamic_cast<T *>(p->second.get());
            return nullptr;
        }

        [[nodiscard]] const timebase::flicks &timeline_timestamp() const {
            return transient_data().tts_;
        }
        void set_timline_timestamp(const timebase::flicks tts) { transient_data().tts_ = tts; }

        [[nodiscard]] const int &playhead_logical_frame() const {
            return transient_data().playhead_logical_frame_;
        }
        void set_playhead_logical_frame(const int frame) {
            transient_data().playhead_logical_frame_ = frame;
        }

        [[nodiscard]] const int &playhead_logical_duration() const {
            return transient_data().playhead_logical_duration_;
        }
        void set_playhead_logical_duration(const int frame) {
            transient_data().playhead_logical_duration_ = frame;
        }

        [[nodiscard]] const bookmark::BookmarkAndAnnotations &bookmarks() const {
            return transient_data().bookmarks_;
        }
        void set_bookmarks(const bookmark::BookmarkAndAnnotations &bookmarks) {
            transient_data().bookmarks_ = bookmarks;
        }

        [[nodiscard]] const media::AVFrameID &frame_id() const {
            return transient_data().frame_id_;
        }
        void set_frame_id(const media::AVFrameID &id) { transient_data().frame_id_ = id; }

        [[nodiscard]] const Imath::M44f &intrinsic_transform() const {
            return transient_data().intrinsic_transform_;
        }
        void set_intrinsic_transform(const Imath::M44f &t) {
            transient_data().intrinsic_transform_ = t;
        }

        [[nodiscard]] const Imath::M44f layout_transform() const {
            return transient_data().frame_id_.transform_matrix() *
                   transient_data().layout_transform_;
        }
        void set_layout_transform(const Imath::M44f &t) {
            transient_data().layout_transform_ = t;
        }

        [[nodiscard]] const std::string &error_details() const {
            return transient_data().error_details_;
        }
        void set_error_details(const std::string &err) {
            transient_data().error_details_ = err;
        }

        friend float image_aspect(const ImageBufPtr &value);

        friend float image_layout_aspect(const ImageBufPtr &value);

        utility::JsonStore metadata() const;

        [[nodiscard]] bool invisible() const { return transient_data().invisible_; }
        void set_invisible(const bool invis) { transient_data().invisible_ = invis; }

        [[nodiscard]] const utility::JsonStore &colour_pipe_uniforms() const {
            return transient_data().colour_pipe_uniforms_;
        }
        utility::JsonStore &colour_pipe_uniforms() {
            return transient_data().colour_pipe_uniforms_;
        }

        [[nodiscard]] const colour_pipeline::ColourPipelineDataPtr &colour_pipe_data() const {
            return transient_data().colour_pipe_data_;
        }
        colour_pipeline::ColourPipelineDataPtr &colour_pipe_data() {
            return transient_data().colour_pipe_data_;
        }

        [[nodiscard]] const utility::time_point &when_to_display() const {
            return transient_data().when_to_display_;
        }
        [[nodiscard]] utility::time_point &when_to_display() {
            return transient_data().when_to_display_;
        }

      private:
        struct SidecarData {

            std::map<utility::Uuid, utility::BlindDataObjectPtr> plugin_blind_data_;
            colour_pipeline::ColourPipelineDataPtr colour_pipe_data_;
            utility::JsonStore colour_pipe_uniforms_;
            utility::time_point when_to_display_;
            Imath::M44f intrinsic_transform_;
            Imath::M44f layout_transform_;
            std::string error_details_;
            timebase::flicks tts_ = timebase::flicks{0};
            media::AVFrameID frame_id_;
            bookmark::BookmarkAndAnnotations bookmarks_;
            int playhead_logical_frame_    = 0;
            int playhead_logical_duration_ = 0;
            bool invisible_                = {false};
        };

        std::shared_ptr<SidecarData> transient_data_;

        const SidecarData &transient_data() const { return *transient_data_.get(); }

        SidecarData &transient_data() {
            // we are to modify transient_data_ - if there are other ImageBufPtrs
            // sharing the object we need to make a copy
            if (transient_data_.use_count() > 1) {
                transient_data_.reset(new SidecarData(*transient_data_.get()));
            }
            return *transient_data_.get();
        }
    };

    inline float image_aspect(const ImageBufPtr &v) {

        return v ? v->image_size_in_pixels().y
                       ? v.frame_id().pixel_aspect() * v->image_size_in_pixels().x /
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
                           ? v.frame_id().pixel_aspect() * v->image_size_in_pixels().x /
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
