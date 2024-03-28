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

    class GPUPreDrawHook {

      public:
        /* Plugins can provide this class to allow a way to execute any GPU
        draw/compute functions *before* the viewport is drawn to the screen.
        Note that 'image' is a non-const reference and as-such the colour
        pipeline data object ptr that is a member of ImageBufPtr can be
        overwritten with new data that the plugin (if it's a ColourOP) can
        access at draw time (like LUTS & texture). Similiarly ViewportOverlay
        plugins could use this to do pixel analysis and put the result into
        texture data. This could be useful for doing waveform overlays, for
        example.

        Note that plugins can add their own data to media via the bookmarks
        system which will then be available here at draw time as metadata on
        the ImageBufPtr we receive here. */

        virtual void pre_viewport_draw_gpu_hook(
            const Imath::M44f &transform_window_to_viewport_space,
            const Imath::M44f &transform_viewport_to_image_space,
            const float viewport_du_dpixel,
            xstudio::media_reader::ImageBufPtr &image) = 0;
    };

    typedef std::shared_ptr<GPUPreDrawHook> GPUPreDrawHookPtr;

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

        virtual utility::BlindDataObjectPtr prepare_overlay_data(
            const media_reader::ImageBufPtr & /*image*/, const bool /*offscreen*/
        ) const {
            return utility::BlindDataObjectPtr();
        }

        // TODO: deprecate prepare_render_data and use this everywhere
        virtual utility::BlindDataObjectPtr onscreen_render_data(
            const media_reader::ImageBufPtr & /*image*/, const std::string & /*viewport_name*/
        ) const {
            return utility::BlindDataObjectPtr();
        }

        // reimpliment this function to receive the image buffer(s) that are
        // currently being displayed on the given viewport
        virtual void images_going_on_screen(
            const std::vector<media_reader::ImageBufPtr> & /*images*/,
            const std::string /*viewport_name*/,
            const bool /*playhead_playing*/
        ) {}

        virtual ViewportOverlayRendererPtr make_overlay_renderer(const int /*viewer_index*/) {
            return ViewportOverlayRendererPtr();
        }

        // Override this and return your own subclass of GPUPreDrawHook to allow
        // arbitrary GPU rendering (e.g. when in the viewport OpenGL context)
        virtual GPUPreDrawHookPtr make_pre_draw_gpu_hook(const int /*viewer_index*/) {
            return GPUPreDrawHookPtr();
        }

        // reimplement this function in an annotations plugin to return your
        // custom annotation class, based on bookmark::AnnotationBase base class.
        virtual bookmark::AnnotationBasePtr
        build_annotation(const utility::JsonStore &anno_data) {
            return bookmark::AnnotationBasePtr();
        }

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

        /* Use this function to create a new bookmark on the current (on screen) frame
        of for the entire duration for the media currently showing on the given named
        viewport. */
        utility::Uuid create_bookmark_on_current_media(
            const std::string &viewport_name,
            const std::string &bookmark_subject,
            const bookmark::BookmarkDetail &detail,
            const bool bookmark_entire_duratio = false);


        /* Call this function to update the annotation data attached to the
        given bookmark */
        void update_bookmark_annotation(
            const utility::Uuid bookmark_id,
            std::shared_ptr<bookmark::AnnotationBase> annotation_data,
            const bool annotation_is_empty);

        void update_bookmark_detail(
            const utility::Uuid bookmark_id, const bookmark::BookmarkDetail &bmd);


      private:
        // re-implement to receive callback when the on-screen media changes. To
        void on_screen_media_changed(caf::actor media) override;

        void session_changed(caf::actor session);

        void current_viewed_playhead_changed(caf::actor_addr playhead_addr);

        void join_studio_events();

        int playhead_logical_frame_ = {-1};

        caf::actor_addr active_viewport_playhead_;
        caf::actor_addr playhead_media_events_group_;
        caf::actor bookmark_manager_;

        module::QmlCodeAttribute *viewport_overlay_qml_code_ = nullptr;
    };


} // namespace plugin
} // namespace xstudio
