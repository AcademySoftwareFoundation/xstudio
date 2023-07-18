#include <caf/all.hpp>

#include <chrono>
#include <type_traits>

#include "xstudio/ui/viewport/viewport.hpp"
#include "xstudio/ui/viewport/viewport_frame_queue_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"

#include "fps_monitor.hpp"

using namespace xstudio::utility;
using namespace xstudio::ui::viewport;
using namespace xstudio::ui;
using namespace xstudio;

namespace {
/*
static Imath::M44f matrix_from_square(const float *in) {

    const Imath::V2f * corners = (const Imath::V2f *)in;
    const float xscale = corners[1].x - corners[0].x;
    const float yscale = corners[2].y - corners[1].y;
    const float x_offset = corners[0].x + corners[1].x;
    const float y_offset = corners[2].y + corners[0].y;
    Imath::M44f i;
    i.makeIdentity();
    i.translate(Imath::V3f(x_offset/2.0f, y_offset/2.0f, 0.0f));
    i.scale(Imath::V3f(xscale/2.0f,-yscale/2.0f,1.0f));
    return i;

}
*/
Imath::M44f matrix_from_corners(const float *in) {

    // A simplified version of the cornerpin transform matrix

    auto corners          = (const Imath::V2f *)in;
    const Imath::V2f &cv0 = corners[0];
    const Imath::V2f &cv1 = corners[1];
    const Imath::V2f &cv2 = corners[2];
    const Imath::V2f &cv3 = corners[3];

    const float g = (cv3.x - cv2.x) * (cv1.y - cv0.y) - (cv1.x - cv0.x) * (cv3.y - cv2.y);

    const float h = (cv3.x - cv0.x) * (cv1.y - cv2.y) - (cv1.x - cv2.x) * (cv3.y - cv0.y);

    const float j = (cv1.x - cv2.x) * (cv3.y - cv2.y) - (cv3.x - cv2.x) * (cv1.y - cv2.y);

    const float c = j * cv0.x;

    const float a = (g + j) * cv1.x - c;

    const float b = (h + j) * cv3.x - c;

    const float f = j * cv0.y;

    const float d = (g + j) * cv1.y - f;

    const float e = (h + j) * cv3.y - f;

    Imath::M33f mqt;

    mqt[0][0] = a;
    mqt[0][1] = b;
    mqt[0][2] = c;
    mqt[1][0] = d;
    mqt[1][1] = e;
    mqt[1][2] = f;
    mqt[2][0] = g;
    mqt[2][1] = h;
    mqt[2][2] = j;

    static const Imath::M33f mqs(
        0.125f, 0.0f, 0.125f, 0.0f, -0.125f, 0.125f, 0.0f, 0.0f, 0.25f);

    Imath::M33f mqr = (mqt * mqs).transpose();

    return Imath::M44f(
        mqr[0][0],
        mqr[0][1],
        0.0,
        mqr[0][2],
        mqr[1][0],
        mqr[1][1],
        0.0,
        mqr[1][2],
        0,
        0,
        1,
        0,
        mqr[2][0],
        mqr[2][1],
        0.0,
        mqr[2][2]);
}

} // namespace


Viewport::Viewport(
    const utility::JsonStore &state_data,
    caf::actor parent_actor,
    const int viewport_index,
    ViewportRendererPtr the_renderer)
    : Module(
          viewport_index >= 0 ? fmt::format("viewport{0}", viewport_index)
                              : fmt::format("offscreen_viewport{0}", abs(viewport_index))),
      parent_actor_(std::move(parent_actor)),
      viewport_index_(viewport_index),
      the_renderer_(std::move(the_renderer)) {

    // TODO: set these up via Json prefs coming in from framework
    // so pointer controls are user configurable
    static const Signature zoom_pointer_event_sig{
        Signature::EventType::Drag,
        Signature::Button::Middle,
        Signature::Modifier::ControlModifier};
    static const Signature pan_pointer_event_sig{
        Signature::EventType::Drag, Signature::Button::Middle, Signature::Modifier::NoModifier};
    static const Signature wheel_zoom_pointer_event_sig{
        Signature::EventType::MouseWheel,
        Signature::Button::None,
        Signature::Modifier::NoModifier};
    static const Signature reset_zoom_pointer_event_sig{
        Signature::EventType::DoubleClick,
        Signature::Button::Left,
        Signature::Modifier::ZoomActionModifier};
    static const Signature reset_pan_pointer_event_sig{
        Signature::EventType::DoubleClick,
        Signature::Button::Left,
        Signature::Modifier::PanActionModifier};
    static const Signature force_zoom_pointer_event_sig{
        Signature::EventType::Drag,
        Signature::Button::Left,
        Signature::Modifier::ZoomActionModifier};
    static const Signature force_pan_pointer_event_sig{
        Signature::EventType::Drag,
        Signature::Button::Left,
        Signature::Modifier::PanActionModifier};

    static const xstudio::utility::JsonStore default_settings = nlohmann::json(
        {{"bg_colour", {0.18f, 0.18f, 0.18f}},
         {"pointer_wheel_senistivity", 2.0f},
         {"pointer_zoom_senistivity", 2.0f}});

    auto callback = [this](auto &&PH1) {
        dummy_evt_callback(std::forward<decltype(PH1)>(PH1));
    };
    event_callback_ = callback;

    // TODO: Use the proper settings mechanism here
    try {
        deserialise(state_data);
    } catch (...) {
        utility::JsonStore r;
        r["settings"] = default_settings;
        deserialise(r);
    }

    pointer_event_handlers_[pan_pointer_event_sig] =
        [=](const PointerEvent & /*pointer_event*/) {
            const Imath::V4f delta_trans =
                interact_start_state_.pointer_position_ -
                normalised_pointer_position() * interact_start_projection_matrix_;
            state_.translate_.x = delta_trans.x + interact_start_state_.translate_.x;
            state_.translate_.y = delta_trans.y + interact_start_state_.translate_.y;
            update_matrix();
            return true;
        };

    pointer_event_handlers_[zoom_pointer_event_sig] =
        [=](const PointerEvent & /*pointer_event*/) {
            const Imath::V4f delta_trans =
                interact_start_state_.pointer_position_ -
                normalised_pointer_position() * interact_start_projection_matrix_;
            const float scale_factor = powf(
                2.0,
                -delta_trans.x * state_.size_.x *
                    settings_["pointer_zoom_senistivity"].get<float>() *
                    interact_start_state_.scale_ / 1000.0f);
            state_.scale_ = interact_start_state_.scale_ * scale_factor;
            const Imath::V4f anchor_before =
                interact_start_state_.pointer_position_ * interact_start_inv_projection_matrix_;
            update_matrix();
            const Imath::V4f anchor_after =
                interact_start_state_.pointer_position_ * inv_projection_matrix_;
            state_.translate_.x += (anchor_after.x - anchor_before.x) / state_.scale_;
            state_.translate_.y -= (anchor_after.y - anchor_before.y) * state_.size_.y /
                                   (state_.scale_ * state_.size_.x);
            update_matrix();
            return true;
        };

    pointer_event_handlers_[wheel_zoom_pointer_event_sig] =
        [=](const PointerEvent &pointer_event) {
            if (mouse_wheel_behaviour_->value() != "Zoom Viewer")
                return false;
            const Imath::V4f anchor_before = state_.pointer_position_ * inv_projection_matrix_;
            state_.scale_ =
                state_.scale_ *
                powf(
                    2.0f,
                    float(pointer_event.angle_delta().second) *
                        settings_["pointer_wheel_senistivity"].get<float>() /
                        1000.0f); // basic pointer wheel causes an angle delta of 120 degrees.
            update_matrix();
            const Imath::V4f anchor_after = state_.pointer_position_ * inv_projection_matrix_;
            state_.translate_.x += (anchor_after.x - anchor_before.x) / state_.scale_;
            state_.translate_.y -= (anchor_after.y - anchor_before.y) * state_.size_.y /
                                   (state_.scale_ * state_.size_.x);
            update_matrix();
            return true;
        };

    pointer_event_handlers_[reset_zoom_pointer_event_sig] =
        [=](const PointerEvent & /*pointer_event*/) {
            revert_fit_zoom_to_previous();
            return true;
        };

    pointer_event_handlers_[reset_pan_pointer_event_sig] =
        [=](const PointerEvent & /*pointer_event*/) {
            revert_fit_zoom_to_previous();
            return true;
        };

    pointer_event_handlers_[force_zoom_pointer_event_sig] =
        pointer_event_handlers_[zoom_pointer_event_sig];
    pointer_event_handlers_[force_pan_pointer_event_sig] =
        pointer_event_handlers_[pan_pointer_event_sig];

    zoom_mode_toggle_ = add_boolean_attribute("Zoom (Z)", "Zm", false);

    pan_mode_toggle_ = add_boolean_attribute("Pan (X)", "Pan", false);

    fit_mode_ = add_string_choice_attribute(
        "Fit (F)",
        "Fit",
        "Best",
        {"1:1", "Best", "Width", "Height", "Fill", "Off"},
        {"1:1", "Best", "Width", "Height", "Fill", "Off"});

    mirror_mode_ = add_string_choice_attribute(
        "Mirror",
        "Mirr",
        "Off",
        {"Flip", "Flop", "Both", "Off"},
        {"Flip", "Flop", "Both", "Off"});

    filter_mode_preference_ = add_string_choice_attribute(
        "Viewport Filter Mode", "Vp. Filtering", ViewportRenderer::pixel_filter_mode_names);
    filter_mode_preference_->set_preference_path("/ui/viewport/filter_mode");

    if (viewport_index_ == 0)
        filter_mode_preference_->set_role_data(
            module::Attribute::Groups, nlohmann::json{"viewport_pixel_filter"});

    static const std::vector<std::tuple<int, std::string, std::string, bool>>
        texture_mode_names = {
            {0, "Image Texture", "Image Texture", true}, {1, "SSBO", "SSBO", true}};

    texture_mode_preference_ =
        add_string_choice_attribute("GPU Texture Mode", "Tex. Mode", texture_mode_names);
    texture_mode_preference_->set_preference_path("/ui/viewport/texture_mode");
    if (viewport_index_ == 0)
        texture_mode_preference_->set_role_data(
            module::Attribute::Groups, nlohmann::json{"viewport_texture_mode"});

    mouse_wheel_behaviour_ = add_string_choice_attribute(
        "Mouse Wheel Behaviour",
        "Wheel Behaviour",
        "Scrub Timeline",
        {"Scrub Timeline", "Zoom Viewer"},
        {"Scrub Timeline", "Zoom Viewer"});
    mouse_wheel_behaviour_->set_preference_path("/ui/viewport/viewport_mouse_wheel_behaviour");
    if (viewport_index_ == 0)
        mouse_wheel_behaviour_->set_role_data(
            module::Attribute::Groups, nlohmann::json{"viewport_mouse_wheel_behaviour_attr"});

    // we give a unique 'toolbar_name' per viewport. This is set on the 'Groups'
    // role data of some attributes. We make the toolbar_name_ available in qml
    // as a property of the Viewport item, which can then see those attributes
    // as part of a data model.
    std::string toolbar_name = name() + "_toolbar";

    zoom_mode_toggle_->set_role_data(
        module::Attribute::Groups, nlohmann::json{toolbar_name, "viewport_zoom_and_pan_modes"});

    pan_mode_toggle_->set_role_data(
        module::Attribute::Groups, nlohmann::json{toolbar_name, "viewport_zoom_and_pan_modes"});
    fit_mode_->set_role_data(module::Attribute::Groups, nlohmann::json{toolbar_name});

    mirror_mode_->set_role_data(module::Attribute::Groups, nlohmann::json{toolbar_name});
    mirror_mode_->set_role_data(
        module::Attribute::ToolTip,
        "Set how image is mirrored on screen : flip(on X axis), flop(on Y axis), both, off. "
        "Shift+f to activate / deactivate flop, use button for the other options");

    zoom_mode_toggle_->set_role_data(module::Attribute::Activated, false);
    zoom_mode_toggle_->set_role_data(
        module::Attribute::ToolTip,
        "Toggle on to enter zoom mode: in zoom mode click and drag mouse left/right to zoom "
        "in/out of viewport. Double click to toggle from zoomed view to your latest auto 'fit' "
        "mode.");
    pan_mode_toggle_->set_role_data(
        module::Attribute::ToolTip,
        "Toggle on to enter pan mode: in pan mode click and drag in viewport to translate the "
        "image.");

    fit_mode_->set_role_data(
        module::Attribute::ToolTip,
        "Set how image is fit to window. In free mode: drag middle mouse button to pan, hold "
        "Ctrl key and middle mouse drag to zoom or roll mouse wheel to zoom.");

    if (viewport_index_ >= 0) {
        add_multichoice_attr_to_menu(fit_mode_, name() + "_context_menu_section0", "Fit");
        add_multichoice_attr_to_menu(mirror_mode_, name() + "_context_menu_section0", "Mirror");
    }

    zoom_mode_toggle_->set_role_data(module::Attribute::ToolbarPosition, 5.0f);
    pan_mode_toggle_->set_role_data(module::Attribute::ToolbarPosition, 6.0f);
    fit_mode_->set_role_data(module::Attribute::ToolbarPosition, 7.0f);
    mirror_mode_->set_role_data(module::Attribute::ToolbarPosition, 8.0f);

    frame_error_message_ = add_string_attribute("frame_error", "frame_error", "");
    frame_error_message_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"viewport_frame_error_message"});

    hud_toggle_ = add_boolean_attribute("Hud", "Hud", true);
    hud_toggle_->set_tool_tip("Access HUD controls");
    hud_toggle_->expose_in_ui_attrs_group(name() + "_toolbar");
    hud_toggle_->expose_in_ui_attrs_group("hud_toggle");
    hud_toggle_->set_preference_path("/ui/viewport/enable_hud");
    // here we set custom QML code to implement a custom widget that is inserted
    // into the viewer toolbox.
    hud_toggle_->set_role_data(
        module::Attribute::QmlCode,
        R"(
            import xStudio 1.0
            XsHudToolbarButton {
                id: control
                anchors.fill: parent
            }
        )");
    hud_toggle_->set_role_data(module::Attribute::ToolbarPosition, 0.0f);

    // give the attributes static uuids so that

    bool is_offscreen = false;

    if (parent_actor_) {

        module::Module::set_parent_actor_addr(caf::actor_cast<actor_addr>(parent_actor_));

        auto a = caf::actor_cast<caf::scheduled_actor *>(parent_actor_);
        caf::scoped_actor sys(a->system());
        fps_monitor_ = sys->spawn<ui::fps_monitor::FpsMonitor>();
        is_offscreen = viewport_index_ == -1;
        if (!is_offscreen)
            connect_to_ui();
        instance_overlay_plugins(!is_offscreen);

        get_colour_pipeline();

        if (viewport_index_ == 0) {
            if (!is_offscreen) {
                a->system().registry().put(main_viewport_registry, a);
            }
        } else if (viewport_index_ == 1) {
            // Popout viewer
            other_viewport_ =
                a->system().registry().template get<caf::actor>(main_viewport_registry);
            anon_send(other_viewport_, other_viewport_atom_v, parent_actor_, colour_pipeline_);
        }

        // join the FPS monitor event group
        auto group =
            request_receive<caf::actor>(*sys, fps_monitor_, utility::get_event_group_atom_v);
        utility::request_receive<bool>(
            *sys, group, broadcast::join_broadcast_atom_v, parent_actor_);

        // join the global playhead events group - this tells us when the playhead that should
        // be on screen changes, among other things
        listen_to_playhead_events();
        auto ph_events =
            a->system().registry().template get<caf::actor>(global_playhead_events_actor);
        // get current playhead, if there is one:
        auto playhead =
            request_receive<caf::actor>(*sys, ph_events, viewport::viewport_playhead_atom_v);
        if (playhead)
            set_playhead(playhead);
    }

    set_fit_mode(FitMode::Best);
    // force update our internal filter mode enum
    attribute_changed(
        filter_mode_preference_->get_role_data<utility::Uuid>(module::Attribute::UuidRole),
        module::Attribute::Value);
}

Viewport::~Viewport() {
    caf::scoped_actor sys(self()->home_system());
    sys->send_exit(fps_monitor_, caf::exit_reason::user_shutdown);
    sys->send_exit(display_frames_queue_actor_, caf::exit_reason::user_shutdown);
}

void Viewport::register_hotkeys() {

    zoom_hotkey_ = register_hotkey(
        int('Z'),
        NoModifier,
        "Activate viewport zoom mode",
        "Press and hold to turn on viewport zooming - when on, scrub mouse left/right to zoom "
        "into or out of the image");

    pan_hotkey_ = register_hotkey(
        int('X'),
        NoModifier,
        "Activate viewport pan mode",
        "Press and hold to turn on viewport panning - when on, click and drag to pan the image "
        "within the viewport");

    reset_hotkey_ = register_hotkey(
        int('R'),
        ControlModifier,
        "Reset Viewport",
        "Resets the viewport zoom/fit to your last 'Fit' mode setting");

    fit_mode_hotkey_ = register_hotkey(
        int('F'),
        NoModifier,
        "Toggle Fit Mode On/Off",
        "Toggles the viewport zoom/fit between your last Fit Mode setting and whatever "
        "zoom/pan you had previously.");

    mirror_mode_hotkey_ = register_hotkey(
        int('F'),
        ShiftModifier,
        "Activate mirror mode",
        "Toggles the mirror mode from Flip / Flop / Both / Off ");
}


xstudio::utility::JsonStore Viewport::serialise() const {
    utility::JsonStore jsn;
    jsn["settings"]             = settings_;
    jsn["translate"]            = state_.translate_;
    jsn["scale"]                = state_.scale_;
    jsn["size"]                 = state_.size_;
    jsn["raw_pointer_position"] = state_.raw_pointer_position_;
    jsn["pointer_position"]     = state_.pointer_position_;
    return jsn;
}

void Viewport::deserialise(const nlohmann::json &state_data) {
    if (state_data.find("settings") == state_data.end()) {
        throw std::runtime_error("Viewport settings json object is missing 'settings' value.");
    }

    settings_ = state_data["settings"].get<nlohmann::json>();

    // TODO: I presume there is a better way to do this??
    if (state_data.find("translate") != state_data.end()) {
        state_.translate_ = state_data["translate"].get<Imath::V3f>();
    }
    if (state_data.find("scale") != state_data.end()) {
        state_.scale_ = state_data["scale"].get<float>();
    }
    if (state_data.find("size") != state_data.end()) {
        state_.size_ = state_data["size"].get<Imath::V2f>();
    }
    if (state_data.find("raw_pointer_position") != state_data.end()) {
        state_.raw_pointer_position_ = state_data["raw_pointer_position"].get<Imath::V2i>();
    }
    if (state_data.find("pointer_position") != state_data.end()) {
        state_.pointer_position_ = state_data["pointer_position"].get<Imath::V4f>();
    }
}

void Viewport::set_pointer_event_viewport_coords(PointerEvent &pointer_event) {

    // converts the raw (pixels) mouse pointer position into the position in the
    // xstudio viewport coordinate system
    state_.raw_pointer_position_ = Imath::V2i(pointer_event.x(), pointer_event.y());
    state_.pointer_position_     = normalised_pointer_position() * projection_matrix_;

    // TODO: there's a much better way to do this directly from the matrix. I'm
    // computing the viewport coordinate of a pixel at (0,0) and the coordinate
    // of a pixel at (1,0) - the difference is the extent of a pixel in viewport
    // coordinates, which tells us how much the effective viewport zoom is in
    // display pixels. We can use this when detecting if the mouse cursor is within
    // N screen pixels of something in the viewport regardless of the zoom
    Imath::V4f d_zero = Imath::V4f(1.0f / state_.size_.x, 0.0f, 0.0f, 1.0f) *
                        projection_matrix_ * fit_mode_matrix_.inverse();
    Imath::V4f delta =
        Imath::V4f(0.0f, 0.0f, 0.0f, 1.0f) * projection_matrix_ * fit_mode_matrix_.inverse();

    Imath::V4f p = state_.pointer_position_;
    p *= fit_mode_matrix_.inverse();

    pointer_event.set_pos_in_coord_sys(p.x, p.y, (d_zero - delta).x);
}

bool Viewport::process_pointer_event(PointerEvent &pointer_event) {

    set_pointer_event_viewport_coords(pointer_event);

    // return value tells us if the pointer event was consumed by the viewport. If not, it is
    // forwarded to any 'Module' that is interested in pointer events
    bool rt_val = false;

    // if zoom or pan modes are active, add this state to the pointer event 'signature' so
    // that the correct pointer event handler is invoked
    if (zoom_mode_toggle_->value() || pan_mode_toggle_->value()) {
        auto sig = pointer_event.signature();
        sig.modifiers_ |= zoom_mode_toggle_->value() ? Signature::Modifier::ZoomActionModifier
                                                     : Signature::Modifier::PanActionModifier;
        const_cast<PointerEvent &>(pointer_event).override_signature(sig);
        rt_val = true;
    }

    if (pointer_event.type() == Signature::EventType::ButtonDown) // pointer button down
    {
        interact_start_state_                 = state_;
        interact_start_projection_matrix_     = projection_matrix_;
        interact_start_inv_projection_matrix_ = inv_projection_matrix_;
    }

    if (pointer_event_handlers_.find(pointer_event.signature()) !=
        pointer_event_handlers_.end()) {

        const float old_scale          = state_.scale_;
        const Imath::V3f old_translate = state_.translate_;

        if (pointer_event_handlers_[pointer_event.signature()](pointer_event)) {
            // Send message to other_viewport_ and pass zoom/pan
            if (other_viewport_) {
                anon_send(
                    other_viewport_,
                    viewport_pan_atom_v,
                    state_.translate_.x,
                    state_.translate_.y);
                anon_send(other_viewport_, viewport_scale_atom_v, state_.scale_);
            }

            if (state_.translate_.x != 0.0f || state_.translate_.y != 0.0f ||
                state_.scale_ != 1.0f) {
                if (state_.fit_mode_ != Free) {
                    previous_fit_zoom_state_.fit_mode_  = state_.fit_mode_;
                    previous_fit_zoom_state_.translate_ = old_translate;
                    previous_fit_zoom_state_.scale_     = old_scale;
                    state_.fit_mode_                    = Free;
                    fit_mode_->set_value("Off");
                    if (other_viewport_) {
                        anon_send(other_viewport_, fit_mode_atom_v, Free);
                    }
                }
            }

            update_matrix();
            rt_val = true;
        }
    }
    return rt_val;
}

bool Viewport::set_scene_coordinates(
    const Imath::V2f topleft,
    const Imath::V2f topright,
    const Imath::V2f bottomright,
    const Imath::V2f bottomleft,
    const Imath::V2i scene_size) {

    // These coordinates describe the quad into which the viewport
    // will be rendered in the coordinate system of the parent 'canvas'.
    // We need this so that we can render the viewport correctly into the
    // QQuickWindow into which it is embedded as a QQuickItem within some
    // qml scene.

    std::vector<float> render_rect = {
        topleft.x * 2.0f / float(scene_size.x) - 1.0f,
        -(topleft.y * 2.0f / float(scene_size.y) - 1.0f),
        topright.x * 2.0f / float(scene_size.x) - 1.0f,
        -(topright.y * 2.0f / float(scene_size.y) - 1.0f),
        bottomright.x * 2.0f / float(scene_size.x) - 1.0f,
        -(bottomright.y * 2.0f / float(scene_size.y) - 1.0f),
        bottomleft.x * 2.0f / float(scene_size.x) - 1.0f,
        -(bottomleft.y * 2.0f / float(scene_size.y) - 1.0f)};

    // now we've got the quad we convert to a transform matrix to be used at
    // render time to plot the viewport correctly into its window within the
    // fuill glviewport (which corresponds to the QQuickWindow)
    Imath::M44f vp2c = matrix_from_corners(render_rect.data());

    if (vp2c != viewport_to_canvas_ || (bottomright - bottomleft).length() != state_.size_.x ||
        (bottomleft - topleft).length() != state_.size_.y) {
        viewport_to_canvas_ = vp2c;
        set_size((bottomright - bottomleft).length(), (bottomleft - topleft).length());
        return true;
    }
    return false;
}

void Viewport::update_fit_mode_matrix(
    const int image_width, const int image_height, const float pixel_aspect) {
    // update current image dims if known
    if (image_height) {
        state_.image_aspect_ = float(image_width * pixel_aspect) / float(image_height);
        state_.image_size_   = Imath::V2i(image_width, image_height);
    }

    // exit if window size is as-yet unknown
    if (!size().x)
        return;

    fit_mode_matrix_.makeIdentity();
    const float a_ratio = state_.image_aspect_ * size().y / (size().x);

    float tx = 0.0f;
    float ty = 0.0f;

    if (fit_mode() == Width) {

        state_.fit_mode_zoom_ = 1.0f;

    } else if (fit_mode() == Height) {

        state_.fit_mode_zoom_ = a_ratio;

    } else if (fit_mode() == Best) {

        if (a_ratio < 1.0f) {
            state_.fit_mode_zoom_ = a_ratio;
        } else {
            state_.fit_mode_zoom_ = 1.0f;
        }

    } else if (fit_mode() == Fill) {

        if (a_ratio > 1.0f) {
            state_.fit_mode_zoom_ = a_ratio;
        } else {
            state_.fit_mode_zoom_ = 1.0f;
        }

    } else if (fit_mode() == One2One && state_.image_size_.x) {

        state_.fit_mode_zoom_ = float(state_.image_size_.x) / size().x;

        // in 1:1 fit mode, if the image has an odd number of pixels and the
        // viewport an even number of pixels (or vice versa) in either axis it causes a problem:
        // the pixel boundaries are exactly on the texture sample points for
        // screen pixels. Floating point errors result in samples jumping to
        // the 'wrong' pixel and thus we get a nasty aliasing pattern arising in the plot. To
        // overcome this I add a half pixel shift in the image position
        if ((state_.image_size_.x & 1) != (int(round(state_.size_.x)) & 1)) {
            tx = 0.5f / state_.size_.x;
        }
        if ((state_.image_size_.y & 1) != (int(round(state_.size_.y)) & 1)) {
            ty = 0.5f / state_.size_.y;
        }
    }

    fit_mode_matrix_.makeIdentity();
    fit_mode_matrix_.scale(Imath::V3f(state_.fit_mode_zoom_, state_.fit_mode_zoom_, 1.0f));
    fit_mode_matrix_.translate(Imath::V3f(tx, ty, 0.0f));
}

float Viewport::pixel_zoom() const {
    return state_.fit_mode_zoom_ * state_.scale_ * float(state_.size_.x) / state_.image_size_.x;
}

void Viewport::set_scale(const float scale) {
    state_.scale_ = scale;
    update_matrix();
}

void Viewport::set_size(const float w, const float h) {
    state_.size_ = Imath::V2f(w, h);
    update_matrix();
}

void Viewport::set_pan(const float x_pan, const float y_pan) {
    state_.translate_.x = x_pan;
    state_.translate_.y = y_pan;
    update_matrix();
}

void Viewport::set_fit_mode(const FitMode md) {

    if (state_.fit_mode_ != md) {
        previous_fit_zoom_state_.fit_mode_  = state_.fit_mode_;
        previous_fit_zoom_state_.translate_ = state_.translate_;
        previous_fit_zoom_state_.scale_     = state_.scale_;
    }

    if (md == Free && state_.fit_mode_ != Free) {

        state_.scale_         = state_.fit_mode_zoom_;
        state_.fit_mode_zoom_ = 1.0f;

    } else if (md != Free) {

        // With active fit mode translate is held at 0.0,0.0 and scale
        // is held at 1.0 .. the additional matrix to apply the fit mode
        // is computed on demand
        state_.translate_ = Imath::V3f(0.0f, 0.0f, 0.0f);
        state_.scale_     = 1.0f;
    }

    state_.fit_mode_ = md;

    if (state_.fit_mode_ == FitMode::One2One)
        fit_mode_->set_value("1:1");
    else if (state_.fit_mode_ == FitMode::Best)
        fit_mode_->set_value("Best");
    else if (state_.fit_mode_ == FitMode::Width)
        fit_mode_->set_value("Width");
    else if (state_.fit_mode_ == FitMode::Height)
        fit_mode_->set_value("Height");
    else if (state_.fit_mode_ == FitMode::Fill)
        fit_mode_->set_value("Fill");
    else if (state_.fit_mode_ == FitMode::Free)
        fit_mode_->set_value("Off");

    update_matrix();

    event_callback_(FitModeChanged);
    event_callback_(ZoomChanged);
    event_callback_(Redraw);
}

void Viewport::set_mirror_mode(const MirrorMode md) {
    state_.mirror_mode_ = md;
    update_matrix();
    event_callback_(MirrorModeChanged);
    event_callback_(Redraw);
}

void Viewport::set_pixel_zoom(const float zoom) {
    if (state_.size_.x && state_.fit_mode_zoom_) {
        const float old_scale          = state_.scale_;
        const Imath::V3f old_translate = state_.translate_;

        state_.scale_ =
            state_.image_size_.x * zoom / (state_.fit_mode_zoom_ * float(state_.size_.x));
        if (state_.translate_.x != 0.0f || state_.translate_.y != 0.0f ||
            state_.scale_ != 1.0f) {
            if (state_.fit_mode_ != Free) {
                previous_fit_zoom_state_.fit_mode_  = state_.fit_mode_;
                previous_fit_zoom_state_.translate_ = old_translate;
                previous_fit_zoom_state_.scale_     = old_scale;
            }
            state_.fit_mode_ = Free;
            fit_mode_->set_value("Off");
        }
        update_matrix();
    }
}

void Viewport::revert_fit_zoom_to_previous() {
    if (previous_fit_zoom_state_.scale_ == 0.0f)
        return; // previous state not set
    std::swap(state_.fit_mode_, previous_fit_zoom_state_.fit_mode_);
    std::swap(state_.translate_, previous_fit_zoom_state_.translate_);
    std::swap(state_.scale_, previous_fit_zoom_state_.scale_);

    if (state_.fit_mode_ == FitMode::One2One)
        fit_mode_->set_value("1:1");
    else if (state_.fit_mode_ == FitMode::Best)
        fit_mode_->set_value("Best");
    else if (state_.fit_mode_ == FitMode::Width)
        fit_mode_->set_value("Width");
    else if (state_.fit_mode_ == FitMode::Height)
        fit_mode_->set_value("Height");
    else if (state_.fit_mode_ == FitMode::Fill)
        fit_mode_->set_value("Fill");
    else if (state_.fit_mode_ == FitMode::Free)
        fit_mode_->set_value("Off");

    update_matrix();
    event_callback_(FitModeChanged);
    event_callback_(ZoomChanged);
    event_callback_(Redraw);
}

void Viewport::switch_mirror_mode() {
    if (mirror_mode_->value() != "Flop")
        mirror_mode_->set_value("Flop");
    else
        mirror_mode_->set_value("Off");
}

Imath::V4f Viewport::normalised_pointer_position() const {

    return Imath::V4f(
        (float(state_.raw_pointer_position_.x) / state_.size_.x) * 2.0f - 1.0f,
        -((float(state_.raw_pointer_position_.y) / state_.size_.y) * 2.0f - 1.0f),
        0.0f,
        1.0f);
}

Imath::V2f Viewport::image_coordinate_to_viewport_coordinate(const int x, const int y) const {

    const float aspect = float(state_.image_size_.y) / float(state_.image_size_.x);

    float norm_x = float(x) / state_.image_size_.x;
    float norm_y = float(y) / state_.image_size_.y;

    Imath::V4f a(norm_x * 2.0f - 1.0f, (norm_y * 2.0f - 1.0f) * aspect, 0.0f, 1.0f);

    a *= fit_mode_matrix() * inv_projection_matrix();

    return Imath::V2f((a.x / a.w + 1.0f) / 2.0f, (-a.y / a.w + 1.0f) / 2.0f);
}

Imath::V2f Viewport::pointer_position() const {
    return Imath::V2f(state_.pointer_position_.x, state_.pointer_position_.y);
}

Imath::V2i Viewport::raw_pointer_position() const { return state_.raw_pointer_position_; }

void Viewport::update_matrix() {

    inv_projection_matrix_.makeIdentity();
    inv_projection_matrix_.scale(Imath::V3f(1.0f, -1.0f, 1.0f));
    inv_projection_matrix_.scale(Imath::V3f(1.0f, state_.size_.x / state_.size_.y, 1.0f));
    inv_projection_matrix_.scale(Imath::V3f(state_.scale_, state_.scale_, state_.scale_));
    inv_projection_matrix_.translate(
        Imath::V3f(-state_.translate_.x, -state_.translate_.y, 0.0f));

    projection_matrix_.makeIdentity();
    projection_matrix_.translate(Imath::V3f(state_.translate_.x, state_.translate_.y, 0.0f));
    projection_matrix_.scale(
        Imath::V3f(1.0f / state_.scale_, 1.0f / state_.scale_, 1.0f / state_.scale_));

    float scale_factor_x = 1.0f;
    if (state_.mirror_mode_ == MirrorMode::Flop || state_.mirror_mode_ == MirrorMode::Both)
        scale_factor_x = -scale_factor_x;

    float scale_factor_y = state_.size_.y / state_.size_.x;
    if (state_.mirror_mode_ == MirrorMode::Flip || state_.mirror_mode_ == MirrorMode::Both)
        scale_factor_y = -scale_factor_y;

    projection_matrix_.scale(Imath::V3f(scale_factor_x, scale_factor_y, 1.0f));
    projection_matrix_.scale(Imath::V3f(1.0f, -1.0f, 1.0f));

    update_fit_mode_matrix();
}

Imath::Box2f Viewport::image_bounds_in_viewport_pixels() const {
    const Imath::M44f m = fit_mode_matrix() * inv_projection_matrix();

    const float aspect = float(state_.image_size_.y) / float(state_.image_size_.x);

    Imath::Vec4<float> a(-1.0f, -aspect, 0.0f, 1.0f);
    a *= m;

    Imath::Vec4<float> b(1.0f, aspect, 0.0f, 1.0f);
    b *= m;

    Imath::V2f topLeft((a.x / a.w + 1.0f) / 2.0f, (-a.y / a.w + 1.0f) / 2.0f);
    Imath::V2f bottomRight((b.x / b.w + 1.0f) / 2.0f, (-b.y / b.w + 1.0f) / 2.0f);

    return Imath::Box2f(topLeft, bottomRight);
}

Imath::V2i Viewport::image_resolution() const {
    Imath::V2i resolution(state_.image_size_.x, state_.image_size_.y);
    return resolution;
}

std::list<std::pair<FitMode, std::string>> Viewport::fit_modes() {
    return std::list<std::pair<FitMode, std::string>>({
        std::make_pair(FitMode::One2One, "1:1"),
        std::make_pair(FitMode::Best, "Best"),
        std::make_pair(FitMode::Width, "Width"),
        std::make_pair(FitMode::Height, "Height"),
        std::make_pair(FitMode::Fill, "Fill"),
        std::make_pair(FitMode::Free, "Off"),
    });
}

caf::message_handler Viewport::message_handler() {

    auto a = caf::actor_cast<caf::scheduled_actor *>(parent_actor_);
    if (a) {
        keyboard_events_actor_ = a->system().registry().get<caf::actor>(keyboard_events);
    }

    return caf::message_handler(
               {[=](::viewport_set_scene_coordinates_atom,
                    const Imath::V2f &topleft,
                    const Imath::V2f &topright,
                    const Imath::V2f &bottomright,
                    const Imath::V2f &bottomleft,
                    const Imath::V2i &scene_size) {
                    float zoom = pixel_zoom();
                    if (set_scene_coordinates(
                            topleft, topright, bottomright, bottomleft, scene_size)) {
                        if (zoom != pixel_zoom()) {
                            event_callback_(ZoomChanged);
                        }
                        event_callback_(Redraw);
                    }
                },
                [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

                [=](fit_mode_atom) -> FitMode { return fit_mode(); },

                [=](fit_mode_atom, const FitMode mode) { set_fit_mode(mode); },

                [=](other_viewport_atom,
                    caf::actor other_view,
                    caf::actor other_colour_pipeline) {
                    other_viewport_ = other_view;

                    // here we link up the colour pipelines of the two viewports
                    anon_send(
                        colour_pipeline_,
                        module::link_module_atom_v,
                        other_colour_pipeline,
                        false, // link all attrs
                        true,  // two way link (change in one is synced to other, both ways)
                        viewport_index_ == 0 // push sync (if we are main viewport, sync the
                                             // attrs on the other colour pipelin to ourselves)
                    );
                },

                [=](colour_pipeline::colour_pipeline_atom) -> caf::actor {
                    return colour_pipeline_;
                },

                [=](show_buffer_atom, bool playing) {
                    if (!playing || (playing != playing_)) {
                        playing_ = playing;
                    }
                    event_callback_(Redraw);
                },

                [=](utility::event_atom,
                    ui::fps_monitor::fps_meter_update_atom,
                    const std::string &fps_expr) {
                    frame_rate_expr_ = fps_expr;
                    event_callback_(FrameRateChanged);
                },

                [=](utility::serialise_atom) -> utility::JsonStore {
                    utility::JsonStore jsn;
                    jsn["base"] = serialise();
                    return jsn;
                },

                [=](viewport_pan_atom) -> Imath::V2f { return pan(); },

                [=](viewport_pan_atom, const float xpan, const float ypan) {
                    // To use
                    set_pan(xpan, ypan);
                    event_callback_(TranslationChanged);
                    event_callback_(Redraw);
                },

                [=](viewport_playhead_atom, caf::actor playhead) -> bool {
                    set_playhead(playhead);
                    return true;
                },

                [=](utility::event_atom, viewport_playhead_atom, caf::actor playhead) {
                    set_playhead(playhead);
                },

                [=](viewport_playhead_atom) -> caf::actor_addr { return playhead_addr_; },

                [=](viewport_pixel_zoom_atom, const float zoom) {
                    const FitMode fm = fit_mode();
                    set_pixel_zoom(zoom);
                    event_callback_(ZoomChanged);
                    if (fm != fit_mode()) {
                        event_callback_(FitModeChanged);
                    }
                },

                [=](viewport_scale_atom) -> float { return pixel_zoom(); },

                [=](viewport_scale_atom, const float scale) {
                    // To use
                    set_scale(scale);
                    event_callback_(ZoomChanged);
                    event_callback_(ScaleChanged);
                    event_callback_(Redraw);
                },

                [=](const error &err) mutable {}

               })
        .or_else(module::Module::message_handler());
}

void Viewport::set_playhead(caf::actor playhead, const bool wait_for_refresh) {

    spdlog::debug("Viewport::set_playhead {0}", to_string(playhead));

    // if null playhead stop here.
    if (!parent_actor_) {
        // set_new_playhead(utility::Uuid());
        return;
    }

    auto a = caf::actor_cast<caf::event_based_actor *>(parent_actor_);
    caf::scoped_actor sys(a->system());

    try {

        // leave previous playhead's broacast events group
        if (playhead_viewport_events_group_) {
            try {
                utility::request_receive<bool>(
                    *sys,
                    playhead_viewport_events_group_,
                    broadcast::leave_broadcast_atom_v,
                    a);
            } catch (...) {
            }
        }

        if (!playhead) {
            playhead_addr_ = caf::actor_addr();
            return;
        }

        // and join the new playhead's broacast events group that concern the
        // viewport
        playhead_viewport_events_group_ = utility::request_receive<caf::actor>(
            *sys, playhead, playhead::viewport_events_group_atom_v);

        if (playhead_viewport_events_group_)
            utility::request_receive<bool>(
                *sys, playhead_viewport_events_group_, broadcast::join_broadcast_atom_v, a);

        // Get the 'key' child playhead UUID
        auto curr_playhead_uuid = utility::request_receive<utility::Uuid>(
            *sys, playhead, playhead::key_child_playhead_atom_v);

        // Let the fps monitor join the new playhead too
        sys->anon_send(fps_monitor_, fps_monitor::connect_to_playhead_atom_v, playhead);

        // Let the fps monitor join the new playhead too
        sys->anon_send(colour_pipeline_, viewport_playhead_atom_v, playhead);

        // for off screen rendering, we need to make sure that we've fetched images
        // from the playhead ...
        if (wait_for_refresh) {

            // here we tell any viewport plugins that the playhead has changed
            for (auto p : overlay_plugin_instances_) {
                utility::request_receive<bool>(
                    *sys,
                    p.second,
                    module::current_viewport_playhead_atom_v,
                    caf::actor_cast<caf::actor_addr>(playhead));
            }

            // pass the new playhead to the actor that manages the queue of images
            // to be display now and in the near future
            utility::request_receive<bool>(
                *sys, display_frames_queue_actor_, viewport_playhead_atom_v, playhead, true);


        } else {

            anon_send(display_frames_queue_actor_, viewport_playhead_atom_v, playhead, false);

            // here we tell any viewport plugins that the playhead has changed
            for (auto p : overlay_plugin_instances_) {
                anon_send(
                    p.second,
                    module::current_viewport_playhead_atom_v,
                    caf::actor_cast<caf::actor_addr>(playhead));
            }
        }

        auto ph_events =
            a->system().registry().template get<caf::actor>(global_playhead_events_actor);
        // tell the playhead events actor that the on-screen playhead has changed
        // (in case the viewport playhead was set directly rather than from
        // the playhead events actor itself)
        anon_send(ph_events, viewport::viewport_playhead_atom_v, playhead);

    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }


    // sending a jump message forces the playhead to re-broadcast the image buffers
    // and in turn causes a redraw/update on this viewport
    if (playhead)
        sys->anon_send(playhead, playhead::jump_atom_v);
    playhead_addr_ = caf::actor_cast<caf::actor_addr>(playhead);

    // trigger stuff in the UI layer if necessary, like connecting the playheadUI
    // to the new viewport playhead
    event_callback_(PlayheadChanged);
}

void Viewport::attribute_changed(const utility::Uuid &attr_uuid, const int role) {
    if (attr_uuid == fit_mode_->uuid()) {
        const std::string mode = fit_mode_->value();
        if (mode == "1:1")
            set_fit_mode(FitMode::One2One);
        else if (mode == "Best")
            set_fit_mode(FitMode::Best);
        else if (mode == "Width")
            set_fit_mode(FitMode::Width);
        else if (mode == "Height")
            set_fit_mode(FitMode::Height);
        else if (mode == "Fill")
            set_fit_mode(FitMode::Fill);
        else
            set_fit_mode(FitMode::Free);

        // bind fit mode between the two viewports (main and popout)
        // don't do that!
        /*if(other_viewport_){
            anon_send(other_viewport_,
                xstudio::module::change_attribute_value_atom_v,
                fit_mode_->get_role_data<std::string>(module::Attribute::Title),
                utility::JsonStore(fit_mode_->value()),
                true);
        }*/

    } else if (attr_uuid == zoom_mode_toggle_->uuid() && role == module::Attribute::Value) {

        if (zoom_mode_toggle_->value()) {
            pan_mode_toggle_->set_value(false);
            if (other_viewport_) {
                anon_send(
                    other_viewport_,
                    xstudio::module::change_attribute_value_atom_v,
                    pan_mode_toggle_->get_role_data<std::string>(module::Attribute::Title),
                    utility::JsonStore(false),
                    false);
            }
        }

        if (other_viewport_) {
            anon_send(
                other_viewport_,
                xstudio::module::change_attribute_value_atom_v,
                zoom_mode_toggle_->get_role_data<std::string>(module::Attribute::Title),
                utility::JsonStore(zoom_mode_toggle_->value()),
                false);
        }

    } else if (attr_uuid == pan_mode_toggle_->uuid() && role == module::Attribute::Value) {

        if (pan_mode_toggle_->value()) {
            zoom_mode_toggle_->set_value(false);
            if (other_viewport_) {
                anon_send(
                    other_viewport_,
                    xstudio::module::change_attribute_value_atom_v,
                    zoom_mode_toggle_->get_role_data<std::string>(module::Attribute::Title),
                    utility::JsonStore(false),
                    false);
            }
        }

        if (other_viewport_) {
            anon_send(
                other_viewport_,
                xstudio::module::change_attribute_value_atom_v,
                pan_mode_toggle_->get_role_data<std::string>(module::Attribute::Title),
                utility::JsonStore(pan_mode_toggle_->value()),
                false);
        }
    } else if (attr_uuid == filter_mode_preference_->uuid()) {

        const std::string filter_mode_pref = filter_mode_preference_->value();
        for (auto opt : ViewportRenderer::pixel_filter_mode_names) {
            if (filter_mode_pref == std::get<1>(opt)) {
                the_renderer_->set_render_hints(std::get<0>(opt));
            }
        }
        event_callback_(Redraw);

        if (other_viewport_) {
            anon_send(
                other_viewport_,
                xstudio::module::change_attribute_value_atom_v,
                filter_mode_preference_->get_role_data<std::string>(module::Attribute::Title),
                utility::JsonStore(filter_mode_preference_->value()),
                true);
        }


    } else if (attr_uuid == texture_mode_preference_->uuid()) {
        if (other_viewport_) {
            anon_send(
                other_viewport_,
                xstudio::module::change_attribute_value_atom_v,
                texture_mode_preference_->get_role_data<std::string>(module::Attribute::Title),
                utility::JsonStore(texture_mode_preference_->value()),
                true);
        }
    } else if (attr_uuid == mouse_wheel_behaviour_->uuid()) {
        if (other_viewport_) {
            anon_send(
                other_viewport_,
                xstudio::module::change_attribute_value_atom_v,
                mouse_wheel_behaviour_->get_role_data<std::string>(module::Attribute::Title),
                utility::JsonStore(mouse_wheel_behaviour_->value()),
                true);
        }
    } else if (attr_uuid == hud_toggle_->uuid()) {
        if (other_viewport_) {
            anon_send(
                other_viewport_,
                xstudio::module::change_attribute_value_atom_v,
                hud_toggle_->get_role_data<std::string>(module::Attribute::Title),
                utility::JsonStore(hud_toggle_->value()),
                true);
        }
        for (auto &p : hud_plugin_instances_) {
            anon_send(p.second, enable_hud_atom_v, hud_toggle_->value());
        }
    } else if (attr_uuid == mirror_mode_->uuid()) {
        const std::string mode = mirror_mode_->value();
        if (mode == "Flip")
            set_mirror_mode(MirrorMode::Flip);
        else if (mode == "Flop")
            set_mirror_mode(MirrorMode::Flop);
        else if (mode == "Both")
            set_mirror_mode(MirrorMode::Both);
        else
            set_mirror_mode(MirrorMode::Off);
    }
}

void Viewport::update_attrs_from_preferences(const utility::JsonStore &j) {

    Module::update_attrs_from_preferences(j);
    // TODO: proper preferences handling for the viewport renderer class
    utility::JsonStore p;
    p["texture_mode"] = texture_mode_preference_->value();
    the_renderer_->set_prefs(p);
}

void Viewport::hotkey_pressed(
    const utility::Uuid &hotkey_uuid, const std::string & /*context*/) {
    if (hotkey_uuid == zoom_hotkey_) {
        zoom_mode_toggle_->set_role_data(module::Attribute::Activated, true);
        zoom_mode_toggle_->set_value(true);
        pan_mode_toggle_->set_role_data(module::Attribute::Activated, false);
        pan_mode_toggle_->set_value(false);
    } else if (hotkey_uuid == pan_hotkey_) {
        pan_mode_toggle_->set_role_data(module::Attribute::Activated, true);
        pan_mode_toggle_->set_value(true);
        zoom_mode_toggle_->set_role_data(module::Attribute::Activated, false);
        zoom_mode_toggle_->set_value(false);
    } else if (hotkey_uuid == reset_hotkey_) {
        if (fit_mode() == FitMode::Free)
            revert_fit_zoom_to_previous();
    } else if (hotkey_uuid == fit_mode_hotkey_) {
        revert_fit_zoom_to_previous();
    } else if (hotkey_uuid == mirror_mode_hotkey_) {
        switch_mirror_mode();
    }
}

void Viewport::hotkey_released(
    const utility::Uuid &hotkey_uuid, const std::string & /*context*/) {
    if (hotkey_uuid == zoom_hotkey_) {
        zoom_mode_toggle_->set_role_data(module::Attribute::Activated, false);
        zoom_mode_toggle_->set_value(false);
    } else if (hotkey_uuid == pan_hotkey_) {
        pan_mode_toggle_->set_role_data(module::Attribute::Activated, false);
        pan_mode_toggle_->set_value(false);
    }
}

void Viewport::update_onscreen_frame_info(const media_reader::ImageBufPtr &frame) {

    // this should be called by the subclass of this Viewport class just
    // before or after the viewport is drawn or redrawn with the given frame

    if (!frame) {
        on_screen_frame_buffer_.reset();
        about_to_go_on_screen_frame_buffer_.reset();
        on_screen_frame_ = 0;
        event_callback_(OnScreenFrameChanged);
        return;
    }

    if (about_to_go_on_screen_frame_buffer_ != frame) {
        about_to_go_on_screen_frame_buffer_ = frame;
    }

    // check if the frame buffer has some error message attached
    if (frame->error_state()) {
        frame_error_message_->set_value(frame->error_message());
    } else {
        frame_error_message_->set_value("");
    }

    // update the 'on_screen_frame_' attr, which is propagated to the QML
    // layer
    if (frame->params().find("playhead_frame") != frame->params().end()) {
        on_screen_frame_ = frame->params()["playhead_frame"].get<int>();
        event_callback_(OnScreenFrameChanged);
    }

    //
    if (frame->params().find("HELD_FRAME") != frame->params().end()) {
        if (frame_out_of_range_ != frame->params()["HELD_FRAME"]) {
            frame_out_of_range_ = frame->params()["HELD_FRAME"];
            event_callback_(OutOfRangeChanged);
        }
    } else if (frame_out_of_range_) {
        frame_out_of_range_ = false;
        event_callback_(OutOfRangeChanged);
    }

    if (frame->has_alpha() == no_alpha_channel_) {

        no_alpha_channel_ = !frame->has_alpha();
        event_callback_(NoAlphaChannelChanged);
    }
}

void Viewport::framebuffer_swapped() {

    anon_send(
        display_frames_queue_actor_,
        ui::fps_monitor::framebuffer_swapped_atom_v,
        utility::clock::now(),
        screen_refresh_period_,
        viewport_index_);

    if (about_to_go_on_screen_frame_buffer_ != on_screen_frame_buffer_) {

        on_screen_frame_buffer_ = about_to_go_on_screen_frame_buffer_;
        int f                   = 0;
        if (on_screen_frame_buffer_ &&
            on_screen_frame_buffer_->params().find("playhead_frame") !=
                on_screen_frame_buffer_->params().end()) {
            f = on_screen_frame_buffer_->params()["playhead_frame"].get<int>();
        }
        anon_send(
            fps_monitor(),
            ui::fps_monitor::framebuffer_swapped_atom_v,
            utility::clock::now(),
            f);
    }
}

void Viewport::get_frames_for_display(std::vector<media_reader::ImageBufPtr> &next_images) {

    if (!parent_actor_)
        return;
    auto a = caf::actor_cast<caf::scheduled_actor *>(parent_actor_);
    caf::scoped_actor sys(a->system());

    try {

        auto t0 = utility::clock::now();

        next_images = request_receive_wait<std::vector<media_reader::ImageBufPtr>>(
            *sys,
            display_frames_queue_actor_,
            std::chrono::milliseconds(1000),
            viewport_get_next_frames_for_display_atom_v);

        for (auto &image : next_images) {

            image.colour_pipe_data_ =
                request_receive_wait<colour_pipeline::ColourPipelineDataPtr>(
                    *sys,
                    colour_pipeline_,
                    std::chrono::milliseconds(1000),
                    colour_pipeline::get_colour_pipe_data_atom_v,
                    image.frame_id());

            image.colour_pipe_uniforms_ = request_receive_wait<utility::JsonStore>(
                *sys,
                colour_pipeline_,
                std::chrono::milliseconds(1000),
                colour_pipeline::colour_operation_uniforms_atom_v,
                image.frame_id(),
                image.colour_pipe_data_);
        }

        if (next_images.size()) {

            auto image = next_images.front();
            // for active 'fit modes' to work we need to tell the viewport the dimensions of the
            // image that is about to be drawn.
            auto image_dims = image->image_size_in_pixels();
            update_fit_mode_matrix(image_dims.x, image_dims.y, image->pixel_aspect());
        }


    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    t1_ = utility::clock::now();
}

void Viewport::instance_overlay_plugins(const bool share_plugin_instances) {

    if (!parent_actor_)
        return;
    auto a = caf::actor_cast<caf::scheduled_actor *>(parent_actor_);
    caf::scoped_actor sys(a->system());

    try {

        // get the OCIO colour pipeline plugin (the only one implemented right now)
        auto pm = a->system().registry().template get<caf::actor>(plugin_manager_registry);
        auto overlay_plugin_details =
            request_receive<std::vector<plugin_manager::PluginDetail>>(
                *sys,
                pm,
                utility::detail_atom_v,
                plugin_manager::PluginType::PT_VIEWPORT_OVERLAY);

        for (const auto &pd : overlay_plugin_details) {
            if (true) { // pd.enabled_) {
                // Note the use of the singleton flag on spawning - if this
                // plugin has already been spawned we want to use the existing
                // instance - hence the pop-out viewport will share the plugin
                // with the main viewport
                overlay_actor_ = request_receive<caf::actor>(
                    *sys,
                    pm,
                    plugin_manager::spawn_plugin_atom_v,
                    pd.uuid_,
                    utility::JsonStore(),
                    share_plugin_instances // this is the 'singleton' flag
                );

                // Note on that 'singleton' flag. We want the pop-out viewer and
                // main viewport to share viewport plugin instances because they
                // should both show the same overlay graphics. Other viewport
                // instances (e.g. offscreen viewport for rendering images) needs
                // it's own overlay plugins because it won't necessarily be
                // rendering the same content as what's on screen in the GUI.

                if (share_plugin_instances)
                    anon_send(overlay_actor_, module::connect_to_ui_atom_v);

                auto funkydunc = request_receive<plugin::ViewportOverlayRendererPtr>(
                    *sys, overlay_actor_, overlay_render_function_atom_v, viewport_index_);

                if (funkydunc) {
                    the_renderer_->add_overlay_renderer(pd.uuid_, funkydunc);
                }

                overlay_plugin_instances_[pd.uuid_] = overlay_actor_;
            }
        }

        /* HUD plugins are more-or-less the same as viewport overlay plugins, except
        that they are activated through a single HUD pop-up in the toolbar and the
        are 'aware' of the screenspace that other HUDs have already occupied */
        auto hud_plugin_details = request_receive<std::vector<plugin_manager::PluginDetail>>(
            *sys, pm, utility::detail_atom_v, plugin_manager::PluginType::PT_HEAD_UP_DISPLAY);

        for (const auto &pd : hud_plugin_details) {
            if (true) { // pd.enabled_) {
                overlay_actor_ = request_receive<caf::actor>(
                    *sys,
                    pm,
                    plugin_manager::spawn_plugin_atom_v,
                    pd.uuid_,
                    utility::JsonStore(),
                    share_plugin_instances // this is the 'singleton' flag
                );

                if (share_plugin_instances)
                    anon_send(overlay_actor_, module::connect_to_ui_atom_v);

                auto funkydunc = request_receive<plugin::ViewportOverlayRendererPtr>(
                    *sys, overlay_actor_, overlay_render_function_atom_v, viewport_index_);

                if (funkydunc) {
                    the_renderer_->add_overlay_renderer(pd.uuid_, funkydunc);
                }

                overlay_plugin_instances_[pd.uuid_] = overlay_actor_;
                hud_plugin_instances_[pd.uuid_]     = overlay_actor_;
                anon_send(overlay_actor_, enable_hud_atom_v, hud_toggle_->value());
            }
        }

        display_frames_queue_actor_ =
            sys->spawn<ViewportFrameQueueActor>(overlay_plugin_instances_, viewport_index_);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

media_reader::ImageBufPtr Viewport::get_image_from_playhead(caf::actor playhead) {

    caf::scoped_actor sys(self()->home_system());

    // set the playhead on overlay actors that do annotations, masks for example
    for (auto p : overlay_plugin_instances_) {

        utility::request_receive<bool>(
            *sys,
            p.second,
            module::current_viewport_playhead_atom_v,
            caf::actor_cast<caf::actor_addr>(playhead));
    }

    // get the current image for the playhead
    auto image = utility::request_receive<media_reader::ImageBufPtr>(
        *sys, playhead, playhead::buffer_atom_v);

    if (image && image->error_state() == xstudio::media_reader::HAS_ERROR) {
        throw std::runtime_error(image->error_message());
    } else if (!image) {
        throw std::runtime_error(
            "OffscreenViewport::renderToThumbnail - Playhead returned a null image.");
    }

    image.colour_pipe_data_ = request_receive_wait<colour_pipeline::ColourPipelineDataPtr>(
        *sys,
        colour_pipeline_,
        std::chrono::milliseconds(1000),
        colour_pipeline::get_colour_pipe_data_atom_v,
        image.frame_id());

    image.colour_pipe_uniforms_ = request_receive_wait<utility::JsonStore>(
        *sys,
        colour_pipeline_,
        std::chrono::milliseconds(1000),
        colour_pipeline::colour_operation_uniforms_atom_v,
        image.frame_id(),
        image.colour_pipe_data_);

    // get the overlay plugins to generate their data for onscreen rendering
    // (e.g. annotations strokes) and add to the image
    for (auto p : overlay_plugin_instances_) {

        utility::Uuid overlay_actor_uuid = p.first;
        caf::actor overlay_actor         = p.second;

        auto bdata = request_receive<utility::BlindDataObjectPtr>(
            *sys, overlay_actor, prepare_overlay_render_data_atom_v, image, true);

        image.add_plugin_blind_data(overlay_actor_uuid, bdata);
    }

    return image;
}

void Viewport::get_colour_pipeline() {
    try {
        if (!parent_actor_)
            return;

        auto a = caf::actor_cast<caf::scheduled_actor *>(parent_actor_);
        caf::scoped_actor sys(a->system());

        auto colour_pipe_manager =
            a->system().registry().get<caf::actor>(colour_pipeline_registry);
        auto colour_pipe = request_receive<caf::actor>(
            *sys,
            colour_pipe_manager,
            xstudio::colour_pipeline::colour_pipeline_atom_v,
            name());

        if (colour_pipeline_ != colour_pipe) {
            colour_pipeline_ = colour_pipe;
        }

        if (viewport_index_ >= 0) {
            // negative index is offscreen
            anon_send(colour_pipeline_, module::connect_to_ui_atom_v);
            anon_send(
                colour_pipeline_,
                colour_pipeline::connect_to_viewport_atom_v,
                self(),
                name(),
                viewport_index_);
        }

        anon_send(
            display_frames_queue_actor_,
            colour_pipeline::colour_pipeline_atom_v,
            colour_pipeline_);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void Viewport::set_screen_infos(
    const std::string &name,
    const std::string &model,
    const std::string &manufacturer,
    const std::string &serialNumber,
    const double refresh_rate) {
    anon_send(
        colour_pipeline_,
        /*utility::event_atom_v,*/ xstudio::ui::viewport::screen_info_atom_v,
        name,
        model,
        manufacturer,
        serialNumber);
    if (refresh_rate)
        screen_refresh_period_ = timebase::to_flicks(1.0 / refresh_rate);
}