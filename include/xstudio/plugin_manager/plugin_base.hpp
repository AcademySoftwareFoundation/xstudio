// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/module/module.hpp"
#include "xstudio/utility/blind_data.hpp"
#include <caf/all.hpp>

namespace xstudio {
namespace plugin {

    class ViewportOverlayRenderer {

      public:
        enum RenderPass { BeforeImage, AfterImage };

        /* An overlay can render before the image is rendered - the
        image is themn plotted with an 'Under' blend operation. This
        allows for alpha blending on a black background. Alternatively
        the overlay can render ontop of the image, after it is drawn.
        If 'have_alpha_buffer' is false, the BeforeImage pass is not
        executed. */
        virtual void render_opengl(
            const Imath::M44f &transform_window_to_viewport_space,
            const Imath::M44f &transform_viewport_to_image_space,
            const float viewport_du_dpixel,
            const xstudio::media_reader::ImageBufPtr &frame,
            const bool have_alpha_buffer){};

        [[nodiscard]] virtual RenderPass preferred_render_pass() const { return AfterImage; }
    };

    typedef std::shared_ptr<ViewportOverlayRenderer> ViewportOverlayRendererPtr;

    class StandardPlugin : public caf::event_based_actor, public module::Module {

      public:
        StandardPlugin(
            caf::actor_config &cfg, std::string name, const utility::JsonStore &init_settings);

        ~StandardPlugin() override = default;

        caf::behavior make_behavior() override {
            set_parent_actor_addr(actor_cast<caf::actor_addr>(this));
            return message_handler_extensions().or_else(
                message_handler_.or_else(module::Module::message_handler()));
        }

      protected:
        virtual caf::message_handler message_handler_extensions() {
            return caf::message_handler();
        }

        caf::message_handler message_handler_;

        virtual utility::BlindDataObjectPtr prepare_render_data(
            const media_reader::ImageBufPtr & /*image*/, const bool /*offscreen*/
        ) const {
            return utility::BlindDataObjectPtr();
        }

        virtual ViewportOverlayRendererPtr make_overlay_renderer(const int /*viewer_index*/) {
            return ViewportOverlayRendererPtr();
        }

        utility::Uuid create_bookmark_on_current_frame(bookmark::BookmarkDetail bmd);

        // reimplement this function in an annotations plugin to return your
        // custom annotation class, based on bookmark::AnnotationBase base class.
        virtual std::shared_ptr<bookmark::AnnotationBase>
        build_annotation(const utility::JsonStore &anno_data) {
            return std::shared_ptr<bookmark::AnnotationBase>();
        }

        void push_annotation_to_bookmark(std::shared_ptr<bookmark::AnnotationBase> annotation);

        std::shared_ptr<bookmark::AnnotationBase>
        fetch_annotation(const utility::Uuid &bookmark_uuid);

        std::map<utility::Uuid, utility::JsonStore>
        clear_annotations_and_bookmarks(std::vector<utility::Uuid> bookmark_ids);

        void restore_annotations_and_bookmarks(
            const std::map<utility::Uuid, utility::JsonStore> &bookmarks_data);

        /* Function signature for on screen frame change callback - reimplement to
        receive this event */
        virtual void on_screen_frame_changed(
            const timebase::flicks,   // playhead position
            const int,                // playhead logical frame
            const int,                // media frame
            const int,                // media logical frame
            const utility::Timecode & // media frame timecode
        ) {}

        /* Function signature for on screen annotation change - reimplement to
        receive this event */
        virtual void on_screen_annotation_changed(
            std::vector<std::shared_ptr<bookmark::AnnotationBase>> // ptrs to annotation
                                                                   // data
        ) {}

        /* Function signature for on screen annotation change - reimplement to
        receive this event */
        virtual void on_screen_media_changed(
            caf::actor,                      // media item actor
            const utility::MediaReference &, // media reference
            const std::string) {}

        /* Function signature for current playhead playing status change - reimplement to
        receive this event */
        virtual void on_playhead_playing_changed(const bool // is playing
        ) {}

        /* Use this function to define the qml code that draws information over the xstudio
        viewport. See basic_viewport_masking and pixel_probe plugin examples. */
        void qml_viewport_overlay_code(const std::string &code);

      private:
        // re-implement to receive callback when the on-screen media changes. To
        void on_screen_media_changed(caf::actor media) override;

        void session_changed(caf::actor session);

        void check_if_onscreen_bookmarks_have_changed(
            const int media_frame, const bool force_update = false);

        void current_viewed_playhead_changed(caf::actor_addr playhead_addr);

        std::vector<std::tuple<utility::Uuid, std::string, int, int>> bookmark_frame_ranges_;
        utility::UuidList onscreen_bookmarks_;
        int playhead_logical_frame_ = {-1};

        caf::actor_addr active_viewport_playhead_;
        caf::actor_addr playhead_media_events_group_;
        caf::actor bookmark_manager_;

        module::QmlCodeAttribute *viewport_overlay_qml_code_ = nullptr;
    };


} // namespace plugin
} // namespace xstudio
