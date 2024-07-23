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
        std::string cache_id_;
        std::vector<ColourLUTPtr> luts_;
        std::vector<ColourTexture> textures_;
        ui::viewport::GPUShaderPtr shader_;
        float order_index_;
        [[nodiscard]] size_t size() const;
        std::any user_data_;
    };

    typedef std::shared_ptr<ColourOperationData> ColourOperationDataPtr;

    struct ColourPipelineData {

        ColourPipelineData()                            = default;
        ColourPipelineData(const ColourPipelineData &o) = default;

        std::string cache_id_;

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

      private:
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

        /* Given the colour related metadata of a media source, evaluate a hash
        that is unique for any unique set of LUTs and/or GPU shaders necessary
        to display the given source on the screen. You must also vary the hash
        with the attributes/state of the ColourPipeline itself when this will
        change the GPU shaders or LUTs required for the display of an image
        coming from the given source. For example, if your display shader applies
        a simple Exposure operation you would probably omit this from the hash
        evaluation. However, the 'Display' attribute will probably affect the
        shader/LUTs so it should be factored into the hash computation.

        This function should be as fast as possible as it is called every time
        an image is displayed. Implement your own cacheing system if necessary
        to avoid expensive evaluations that can otherwise be stored.
        */
        [[nodiscard]] virtual std::string linearise_op_hash(
            const utility::Uuid &source_uuid,
            const utility::JsonStore &media_source_colour_metadata) = 0;

        [[nodiscard]] virtual std::string linear_to_display_op_hash(
            const utility::Uuid &source_uuid,
            const utility::JsonStore &media_source_colour_metadata) = 0;

        /* Create the ColourOperationDataPtr containing the necessary LUT and
        shader data for linearising the source colourspace RGB data from the
        given media source on the screen */
        virtual ColourOperationDataPtr linearise_op_data(
            const utility::Uuid &source_uuid,
            const utility::JsonStore &media_source_colour_metadata) = 0;

        /* If desired, override to return one or more colour operations that
        are applied to the linear colour value before the display space is applied.
        This could be employed for gamma/saturation viewer controls, for example */
        virtual std::vector<ColourOperationDataPtr> intermediate_operations(
            const utility::Uuid &source_uuid,
            const utility::JsonStore &media_source_colour_metadata) {
            return std::vector<ColourOperationDataPtr>{};
        }

        /* Create the ColourOperationDataPtr containing the necessary LUT and
        shader data for transforming linear colour values into display space */
        virtual ColourOperationDataPtr linear_to_display_op_data(
            const utility::Uuid &source_uuid,
            const utility::JsonStore &media_source_colour_metadata) = 0;

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

        /* Re-implement and return true if your class supports a design that
        allows for 'workers'. Workers are clones of your main class instance
        that expensive 'work' can be delegated to. In this case the following
        five functions could be called on the workers instead of the main class:
            - linearise_op_hash
            - linear_to_display_op_hash
            - linearise_op_data
            - linear_to_display_op_data
            - intermediate_operations
        The mechanism relies on (automatic) syncing the Attributes on the
        workers with the main instance: if the above five functions will always
        return the same result for a given state of the Attributes belonging to
        your class then workers can be used to paralellise colour operations.
        If these functions in your ColourPipeline rely on state data other than
        Attributes, however, you cannot use workers. */
        virtual bool allow_workers() const { return false; }

        /* In conjunction with the allow_wokers method, you will also need to
        re-implement this function. If your class is called MyColourPipe the
        reimplementation would look exactly like this:
        caf::actor self_spawn(const utility::JsonStore &s) override { return
        spawn<MyColourPipe>(s); }
        */
        virtual caf::actor self_spawn(const utility::JsonStore &s) { return caf::actor(); }

        /* implement this method to extend information about a pixel that is under the
        mouse pointer. For example, pixel_info might include linear RGB information, but
        we want to add RGB information after a viewing transform has been applied, say. */
        virtual void extend_pixel_info(
            media_reader::PixelInfo &pixel_info, const media::AVFrameID &frame_id) {}

        virtual thumbnail::ThumbnailBufferPtr process_thumbnail(
            const media::AVFrameID &media_ptr, const thumbnail::ThumbnailBufferPtr &buf) = 0;

        virtual std::string fast_display_transform_hash(const media::AVFrameID &media_ptr) = 0;

      protected:
        void make_pre_draw_gpu_hook(
            caf::typed_response_promise<plugin::GPUPreDrawHookPtr> rp, const int viewer_index);

        void attribute_changed(const utility::Uuid &attr_uuid, const int role) override;

        caf::message_handler message_handler_extensions() override;

        bool is_worker() const { return is_worker_; }

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

        utility::JsonStore init_data_;
        caf::actor worker_pool_;
        caf::actor thumbnail_processor_pool_;
        caf::actor pixel_probe_worker_;
        caf::actor cache_;
        std::vector<caf::actor> workers_;
        bool is_worker_         = false;
        bool colour_ops_loaded_ = false;

        std::vector<caf::actor> colour_op_plugins_;
        std::vector<std::pair<caf::typed_response_promise<plugin::GPUPreDrawHookPtr>, int>>
            hook_requests_;
    };

} // namespace colour_pipeline
} // namespace xstudio
