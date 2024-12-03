// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "colour_lut.hpp"
#include "colour_texture.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/module/module.hpp"
#include "xstudio/ui/viewport/shader.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace xstudio {

namespace thumbnail {
    class ThumbnailBuffer;
    typedef std::shared_ptr<ThumbnailBuffer> ThumbnailBufferPtr;
} // namespace thumbnail

namespace colour_pipeline {

    struct ColourOperationData {
        ColourOperationData()                             = default;
        ColourOperationData(const ColourOperationData &o) = default;
        ColourOperationData(const utility::Uuid &uuid, const std::string name)
            : uuid_(uuid), name_(name) {}
        ColourOperationData(const std::string name) : name_(name) {}
        utility::Uuid uuid_;
        std::string name_;
        std::vector<ColourLUTPtr> luts_;
        std::vector<ColourTexture> textures_;
        ui::viewport::GPUShaderPtr shader_;
        float order_index_;
        [[nodiscard]] size_t size() const;
        std::any user_data_;

        void set_cache_id(const std::string &id) { cache_id_ = id; }
        [[nodiscard]] const std::string & cache_id() const { return cache_id_; }

        private:
        std::string cache_id_;

    };

    typedef std::shared_ptr<ColourOperationData> ColourOperationDataPtr;

    struct ColourPipelineData {

        ColourPipelineData()                            = default;
        ColourPipelineData(const ColourPipelineData &o) = default;

        void add_operation(const ColourOperationDataPtr &op) {
            auto p = std::lower_bound(
                ordered_colour_operations_.begin(),
                ordered_colour_operations_.end(),
                op->order_index_,
                [](const ColourOperationDataPtr &o, const float v) {
                    return o->order_index_ < v;
                });
            ordered_colour_operations_.insert(p, op);
        }

        void overwrite_operation_data(const ColourOperationDataPtr &op) {
            for (auto &op_data : ordered_colour_operations_) {
                if (op_data->uuid_ == op->uuid_) {
                    op_data = op;
                    break;
                }
            }
        }

        ColourOperationDataPtr get_operation_data(const utility::Uuid &uuid) {
            for (auto &op_data : ordered_colour_operations_) {
                if (op_data->uuid_ == uuid)
                    return op_data;
            }
            return ColourOperationDataPtr();
        }

        // user_data can be used by colour pipeline plugin to add any data specific
        // to a media source that the colour pipeline wants xstudio to cache and
        // pass back into the colour pipeline when re-evaluating shader, LUTs and
        // uniforms for final video display. We need this for OCIO, where the
        // OCIO shader object is required to extract shader uniforms

        [[nodiscard]] const std::vector<ColourOperationDataPtr> &operations() const {
            return ordered_colour_operations_;
        }

        void set_cache_id(const std::string &id) { cache_id_ = id; }
        [[nodiscard]] const std::string & cache_id() const { return cache_id_; }

      private:

        std::string cache_id_;

        /*Apply grades and other colour manipulations after stage_zero_operation_*/
        std::vector<ColourOperationDataPtr> ordered_colour_operations_;
    };

    typedef std::shared_ptr<ColourPipelineData> ColourPipelineDataPtr;

    // xStudio module implementing OpenColorIO driven colour management.
    // Note that the methods below must be thread safe.

    class ColourPipeline : public plugin::StandardPlugin {

      public:
        enum Pipeline { OpenColorIO };

        ColourPipeline(caf::actor_config &cfg, const utility::JsonStore &init_settings);

        virtual ~ColourPipeline();

        /* Create the ColourOperationDataPtr containing the necessary LUT and
        shader data for linearising the source colourspace RGB data from the
        given media source on the screen.

        You MUST add an appropriate cache_id key value that is unqique for a
        given transform for efficient cache retrieval to prevent computing this
        data again unnecessarily.

        See OCIO plugin for rederence implementation. */
        virtual void linearise_op_data(
            caf::typed_response_promise<ColourOperationDataPtr> &rp,
            const media::AVFrameID &media_ptr) = 0;

        /* If desired, override to return one or more colour operations that
        are applied to the linear colour value before the display space is applied.
        This could be employed for gamma/saturation viewer controls, for example */
        virtual std::vector<ColourOperationDataPtr> intermediate_operations(
            const utility::Uuid &source_uuid,
            const utility::JsonStore &media_source_colour_metadata) {
            return std::vector<ColourOperationDataPtr>{};
        }

        /* Create the ColourOperationDataPtr containing the necessary LUT and
        shader data for transforming linear colour values into display space.
        rp.deliver() MUST be called by your function to deliver the resulting
        ColourOperationDataPtr OR the rp must be passed to a worker actor that
        will call rp.deliver(). See OCIO plugin for reference implementation.

        You MUST add an appropriate cache_id key value that is unqique for a
        given transform for efficient cache retrieval to prevent computing this
        data again unnecessarily. */
        virtual void linear_to_display_op_data(
            caf::typed_response_promise<ColourOperationDataPtr> &rp,
            const media::AVFrameID &media_ptr) = 0;

        /* For the given image build a dictionary of shader uniform names and
        their corresponding values to be used to set the uniform values in your
        shader at draw-time - keys should match the names of uniforms in your
        shader and values should match the type of your uniform.
        For vec3 types etc us Imath::V3f for example.*/
        virtual utility::JsonStore
        update_shader_uniforms(const media_reader::ImageBufPtr &image, std::any &user_data) = 0;

        virtual void media_source_changed(
            const utility::Uuid &source_uuid,
            const utility::JsonStore &media_source_colour_metadata) = 0;

        virtual void screen_changed(
            const std::string &name,
            const std::string &model,
            const std::string &manufacturer,
            const std::string &serialNumber) = 0;

        /* implement this method to extend information about a pixel that is under the
        mouse pointer. For example, pixel_info might include linear RGB information, but
        we want to add RGB information after a viewing transform has been applied, say. */
        virtual void extend_pixel_info(
            media_reader::PixelInfo &pixel_info, const media::AVFrameID &frame_id) {}

        /* Implement this method for converting the colourspace of a thumbnail
        buffer from media source colourspace to a DISPLAY colourspace.

        The incoming buffer is RGB float 32 pixel format and in the native
        colourspace of the source media.

        Information about the source colourspace should be available in the json
        dict returned by 'params()' method on the media_ptr (as well as other
        info provided by AVFrameID such as the URI, frame number etc iuf you
        need it.)

        For example, a thumbnail from an EXR source would most likely be in a
        linear colourspace which we want to convert into a display space.

        The resulting buffer, or an error if one occurs, MUST be delivered by
        calling rp.deliver(). You can call rp.deliver within your function or
        delegate the work to a worker actor which then MUST call rp.deliver.

        See the OCIO plugin for a reference implementation. */
        virtual void process_thumbnail(
            caf::typed_response_promise<thumbnail::ThumbnailBufferPtr> &rp,
            const media::AVFrameID &media_ptr,
            const thumbnail::ThumbnailBufferPtr &buf) = 0;

        /* This function should return a unique string based on the current
        statue of the plugin plus RELEVANT properties of the media_ptr that
        influence its colourpsace transforms to display the corresponding image
        on screen correctly.

        For example, you can make a hash from the OCIO View & Display values
        plus the source colourspace and optionally grading data attached to
        the media. This should be sufficient to tell the calling class when it
        needs to compute new colour opration data and when it can use cached
        data.

        This function must be as fast as possible as it is called on every frame
        refresh.

        See the OCIO plugin for a reference implementation. */
        virtual size_t fast_display_transform_hash(const media::AVFrameID &media_ptr) = 0;

      protected:
        void make_pre_draw_gpu_hook(caf::typed_response_promise<plugin::GPUPreDrawHookPtr> rp);

        void attribute_changed(const utility::Uuid &attr_uuid, const int role) override;

        caf::message_handler message_handler_extensions() override;

        utility::Uuid uuid_;

      private:
        bool make_colour_pipe_data_from_cached_data(
            const media::AVFrameID &media_ptr, const std::string &transform_id);

        bool add_in_flight_request(
            const std::string &transform_id,
            caf::typed_response_promise<ColourPipelineDataPtr> rp);

        void add_colour_op_plugin_data(
            const media::AVFrameID media_ptr,
            ColourPipelineDataPtr result,
            const std::string transform_id);

        void deliver_on_reponse_promises(
            ColourPipelineDataPtr &result, const std::string &transform_id);

        void
        deliver_on_reponse_promises(const caf::error &err, const std::string &transform_id);

        void finalise_colour_pipeline_data(
            const media::AVFrameID &media_ptr,
            const std::string &transform_id,
            ColourOperationDataPtr &linearise_data,
            ColourOperationDataPtr &to_display_data);

        void add_cache_keys(
            const std::string &transform_id,
            const std::string &linearise_transform_cache_id,
            const std::string &display_transform_cache_id);

        void load_colour_op_plugins();

        std::map<std::string, std::vector<caf::typed_response_promise<ColourPipelineDataPtr>>>
            in_flight_requests_;
        std::map<std::string, std::pair<std::string, std::string>> cache_keys_cache_;

        caf::actor thumbnail_processor_pool_;
        caf::actor pixel_probe_worker_;
        caf::actor cache_;
        std::vector<caf::actor> workers_;
        bool colour_ops_loaded_ = false;

        std::vector<caf::actor> colour_op_plugins_;
        std::vector<caf::typed_response_promise<plugin::GPUPreDrawHookPtr>> hook_requests_;
    };

} // namespace colour_pipeline
} // namespace xstudio
