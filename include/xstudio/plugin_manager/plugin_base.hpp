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

        void on_exit() override;

        virtual caf::message_handler message_handler_extensions() {
            return caf::message_handler();
        }

        caf::message_handler message_handler_;

        // TODO: deprecate prepare_render_data and use this everywhere
        virtual utility::BlindDataObjectPtr onscreen_render_data(
            const media_reader::ImageBufPtr & /*image*/, const std::string & /*viewport_name*/
        ) const {
            return utility::BlindDataObjectPtr();
        }

        // reimpliment this function to receive the image buffer(s) that are
        // currently being displayed on the given viewport
        virtual void images_going_on_screen(
            const media_reader::ImageBufDisplaySetPtr & /*image_set*/,
            const std::string /*viewport_name*/,
            const bool /*playhead_playing*/
        ) {}

        virtual ViewportOverlayRendererPtr make_overlay_renderer() {
            return ViewportOverlayRendererPtr();
        }

        // Override this and return your own subclass of GPUPreDrawHook to allow
        // arbitrary GPU rendering (e.g. when in the viewport OpenGL context)
        virtual GPUPreDrawHookPtr make_pre_draw_gpu_hook() { return GPUPreDrawHookPtr(); }

        // reimplement this function in an annotations plugin to return your
        // custom annotation class, based on bookmark::AnnotationBase base class.
        virtual bookmark::AnnotationBasePtr
        build_annotation(const utility::JsonStore &anno_data) {
            return bookmark::AnnotationBasePtr();
        }

        /* Function signature for on screen annotation change - reimplement to
        receive this event. Call join_playhead_events() to activate. */
        virtual void on_screen_media_changed(
            caf::actor,                      // media item actor
            const utility::MediaReference &, // media reference
            const utility::JsonStore &       // colour params
        ) {}

        /* Function signature for current playhead playing status change - reimplement to
        receive this event */
        virtual void on_playhead_playing_changed(const bool // is playing
        ) {}

        /* Use this function to define the qml code that draws information over the xstudio
        viewport. See basic_viewport_masking and pixel_probe plugin examples. */
        void qml_viewport_overlay_code(const std::string &code);

        /* Use this function to create a new bookmark on the given frame (as 
        per frame_details). See annotations_tool.cpp for example useage. */
        utility::Uuid create_bookmark_on_frame(
            const media::AVFrameID &frame_details,
            const std::string &bookmark_subject,
            const bookmark::BookmarkDetail &detail,
            const bool bookmark_entire_duration = false);

        /* Use this function to create a new bookmark on the current (on screen) frame
        of for the entire duration for the media currently showing on the given named
        viewport. */
        utility::Uuid create_bookmark_on_current_media(
            const std::string &viewport_name,
            const std::string &bookmark_subject,
            const bookmark::BookmarkDetail &detail,
            const bool bookmark_entire_duratio = false);

        /* Call this function to turn off any other tools that have direct, interactive
        drawing in the viewport. This will allow your drawing plugin to exclusively
        do interactive drawing. */
        void cancel_other_drawing_tools();

        /* Override this function and take necessary action to disable any interactive
        (e.g.) drawing state of your plugin. */
        virtual void turn_off_overlay_interaction() {}

        utility::UuidList get_bookmarks_on_current_media(const std::string &viewport_name);
        bookmark::BookmarkDetail get_bookmark_detail(const utility::Uuid &bookmark_id);
        bookmark::AnnotationBasePtr get_bookmark_annotation(const utility::Uuid &bookmark_id);

        /* Call this function to update the annotation data attached to the
        given bookmark */
        void update_bookmark_annotation(
            const utility::Uuid bookmark_id,
            bookmark::AnnotationBasePtr annotation_data,
            const bool annotation_is_empty);

        void update_bookmark_detail(
            const utility::Uuid bookmark_id, const bookmark::BookmarkDetail &bmd);

        void remove_bookmark(const utility::Uuid &bookmark_id);

        /* set playback state for playhead attached to the named viewport */
        void start_stop_playback(const std::string viewport_name, bool play);

        /* set the cursor (mouse pointer) shape for all viewports. An empty
        string will return to defaul (arrow) pointer. See XsViewport.qml
        for possible cursor names - the are simply stringified versions of the
        Qt cursorshape enumerator. For example "Qt.WaitCursor" would be a
        valid cursor name. If you have an image resource declared in a qrc
        file this can also be used for fully custom cursor. To see an example
        string-search for 'magnifier_cursor' in the xstudio code base.*/
        void set_viewport_cursor(const std::string cusor_name);

        /* Call this function to start listening to events related to the
        current global (active) playhead. This must be called if you want to
        make use of the on_screen_frame_changed, on_screen_media_changed and
        on_playhead_playing_changed callbacks. */
        void listen_to_playhead_events(const bool listen = true);

      private:
        // re-implement to receive callback when the on-screen media changes. To
        void on_screen_media_changed(caf::actor media) override;

        void session_changed(caf::actor session);

        void current_viewed_playhead_changed(caf::actor playhead);

        void join_studio_events();

        void __images_going_on_screen(
            const media_reader::ImageBufDisplaySetPtr & image_set,
            const std::string viewport_name,
            const bool playhead_playing
        );

        caf::actor_addr active_viewport_playhead_;
        caf::actor_addr playhead_media_events_group_;
        caf::actor bookmark_manager_;
        caf::actor playhead_events_actor_;
        bool joined_playhead_events_ = {false};
        std::map<std::string, utility::Uuid> last_source_uuid_;

        module::BooleanAttribute *viewport_overlay_qml_code_ = nullptr;
    };


} // namespace plugin
} // namespace xstudio
