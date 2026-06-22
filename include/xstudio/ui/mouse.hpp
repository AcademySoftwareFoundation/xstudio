// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/type_id.hpp>
#include <ostream>
#include <utility>
#include <string_view>
#include <Imath/ImathVec.h>

#include "xstudio/utility/chrono.hpp"
#include "xstudio/ui/enums.hpp"

namespace xstudio {
namespace ui {

    struct Signature {

        enum Button {
            None            = 0,
            Left            = 1,
            Right           = 2,
            LeftRight       = 3,
            Middle          = 4,
            LeftMiddle      = 5,
            RightMiddle     = 6,
            LeftMiddleRight = 7
        };

        enum Modifier {
            NoModifier          = 0x0,
            ShiftModifier       = 1 << 0,
            ControlModifier     = 1 << 1,
            AltModifier         = 1 << 2,
            MetaModifier        = 1 << 3,
            KeypadModifier      = 1 << 4,
            GroupSwitchModifier = 1 << 5,
            ZoomActionModifier  = 1 << 6,
            PanActionModifier   = 1 << 7
        };

        enum InputType {
            UnknownInput = 0x0000,
            Mouse        = 0x0001,
            TouchScreen  = 0x0002,
            TouchPad     = 0x0004,
            Puck         = 0x0008, // A device similar to a mouse with a cross-hairs
            Stylus       = 0x0010,
            Airbrush     = 0x0020,
            Keyboard     = 0x1000
        };

        enum PointerType {
            UnknownPointer = 0x0000,
            Generic =
                0x0001, // A mouse or something acting like a mouse (the core pointer on X11)
            Finger = 0x0002,
            Pen    = 0x0004,
            Eraser = 0x0008,
            Cursor = 0x0010
        };

        Signature(
            const EventType type           = EventType::Move,
            const Button buttons           = Button::None,
            const int modifiers            = Modifier::NoModifier,
            const InputType input_type     = InputType::UnknownInput,
            const PointerType pointer_type = PointerType::UnknownPointer)
            : type_(type),
              buttons_(buttons),
              modifiers_(modifiers),
              input_type_(input_type),
              pointer_type_(pointer_type) {}

        Signature(const Signature &o) = default;

        bool operator==(const Signature &o) const {
            return type_ == o.type_ && buttons_ == o.buttons_ && modifiers_ == o.modifiers_ &&
                   input_type_ == o.input_type_ && pointer_type_ == o.pointer_type_;
        }

        bool operator<(const Signature &o) const {
            if (type_ < o.type_)
                return true;
            else if (type_ > o.type_)
                return false;
            if (buttons_ < o.buttons_)
                return true;
            else if (buttons_ > o.buttons_)
                return false;
            if (modifiers_ < o.modifiers_)
                return true;
            else if (modifiers_ > o.modifiers_)
                return false;
            if (input_type_ < o.input_type_)
                return true;
            else if (input_type_ > o.input_type_)
                return false;
            if (pointer_type_ < o.pointer_type_)
                return true;
            return false;
        }

        Signature &operator=(const Signature &o) = default;

        friend std::ostream &operator<<(std::ostream &out, const Signature &o) {

            out << EventType_to_str(o.type_) << " "
                << Signature_Button_to_str(static_cast<Signature::Button>(o.buttons_)) << " ";
            Signature_Modifier_to_str(out, o.modifiers_);
            out << Signature_InputType_to_str(static_cast<Signature::InputType>(o.input_type_))
                << " "
                << Signature_PointerType_to_str(
                       static_cast<Signature::PointerType>(o.pointer_type_));

            return out;
        }

        template <class Inspector> friend bool inspect(Inspector &f, Signature &x) {
            return f.object(x).fields(
                f.field("type", x.type_),
                f.field("but", x.buttons_),
                f.field("mode", x.modifiers_),
                f.field("input type", x.input_type_),
                f.field("pointer type", x.pointer_type_));
        }

        static constexpr const char *Signature_Button_to_str(Signature::Button button) {
            switch (button) {
            case Signature::Button::None:
                return "None";
            case Signature::Button::Left:
                return "Left";
            case Signature::Button::Right:
                return "Right";
            case Signature::Button::LeftRight:
                return "LeftRight";
            case Signature::Button::Middle:
                return "Middle";
            case Signature::Button::LeftMiddle:
                return "LeftMiddle";
            case Signature::Button::RightMiddle:
                return "RightMiddle";
            case Signature::Button::LeftMiddleRight:
                return "LeftMiddleRight";
            }

            return "Undefined";
        }

        static constexpr void Signature_Modifier_to_str(std::ostream &out, int modifier) {
            if (modifier == 0) {
                out << "NoModifier "; // Modifier::NoModifier:
                return;
            }

            for (int mod_mask = Modifier::ShiftModifier;
                 mod_mask != Modifier::PanActionModifier;
                 mod_mask = mod_mask << 1) {
                int res = modifier & mod_mask;

                switch (res) {
                case Modifier::ShiftModifier:
                    out << "ShiftModifier ";
                    break;
                case Modifier::ControlModifier:
                    out << "ControlModifier ";
                    break;
                case Modifier::AltModifier:
                    out << "AltModifier ";
                    break;
                case Modifier::MetaModifier:
                    out << "MetaModifier ";
                    break;
                case Modifier::KeypadModifier:
                    out << "KeypadModifier ";
                    break;
                case Modifier::GroupSwitchModifier:
                    out << "GroupSwitchModifier ";
                    break;
                case Modifier::ZoomActionModifier:
                    out << "ZoomActionModifier ";
                    break;
                case Modifier::PanActionModifier:
                    out << "PanActionModifier ";
                    break;
                }
            }
        }

        static constexpr const char *
        Signature_InputType_to_str(Signature::InputType input_type) {
            switch (input_type) {
            case Signature::InputType::UnknownInput:
                return "UnknownInput";
            case Signature::InputType::Mouse:
                return "Mouse";
            case Signature::InputType::TouchScreen:
                return "TouchScreen";
            case Signature::InputType::TouchPad:
                return "TouchPad";
            case Signature::InputType::Stylus:
                return "Stylus";
            case Signature::InputType::Airbrush:
                return "Airbrush";
            case Signature::InputType::Puck:
                return "Puck";
            case Signature::InputType::Keyboard:
                return "Keyboard";
            }

            return "Undefined";
        }

        static constexpr const char *
        Signature_PointerType_to_str(Signature::PointerType pointer_type) {
            switch (pointer_type) {
            case Signature::PointerType::UnknownPointer:
                return "UnknownPointer";
            case Signature::PointerType::Generic:
                return "Generic";
            case Signature::PointerType::Finger:
                return "Finger";
            case Signature::PointerType::Pen:
                return "Pen";
            case Signature::PointerType::Eraser:
                return "Eraser";
            case Signature::PointerType::Cursor:
                return "Cursor";
            }

            return "Undefined";
        }

        EventType type_;
        int modifiers_;
        int buttons_;
        int input_type_;
        int pointer_type_;
    };


    class PointerEvent {

      public:
        enum WheelDeltaType {
            NotSet = 0,
            Angle  = 1, // On Desktop: the units for the delta is pixels in degree/8.
                        // Since most mouse has a granularity of 15 degress,
                        // the returned values are multiple of 120 (120 units * 1/8 = 15
                        // degrees). On Web: unsupported.
            Pixel = 2,  // On Desktop only on platforms supporting high-resolution pixel-based
                        // delta values, such as macOS. In this case itp =
                        // Signature::InputType::TouchPad. Unreliable on X11, use Angle instead.
                        // On Web: the units for the delta are pixels
            Line = 3,   // On Desktop: unsupported.
                        // On Web: the units for the delta are individual lines of text
            Page = 4    // On Desktop: unsupported.
                        // On Web: the units for the delta are pages,
                        // either defined as a single screen or as a demarcated page
        };

        PointerEvent(const PointerEvent &o) = default;

        PointerEvent(
            EventType t                     = EventType::Move,
            Signature::Button b             = Signature::Button::None,
            int x                           = 0,
            int y                           = 0,
            int w                           = 0,
            int h                           = 0,
            int m                           = Signature::Modifier::NoModifier,
            std::string ctx                 = std::string(),
            std::pair<int, int> wheel_delta = std::make_pair(0, 0),
            WheelDeltaType wheel_delta_unit = WheelDeltaType::NotSet,
            Signature::InputType itp        = Signature::InputType::UnknownInput,
            float pressure                  = 0.0f,
            double timestamp                = 0.0,
            Signature::PointerType ptp      = Signature::PointerType::Generic)
            : signature_(t, b, m, itp, ptp),
              x_position_(x),
              y_position_(y),
              width_(w),
              height_(h),
              wheel_delta_(std::move(wheel_delta)),
              wheel_delta_unit_(wheel_delta_unit),
              context_(std::move(ctx)),
              pressure_(pressure),
              timestamp_(timestamp) {}

        PointerEvent &operator=(const PointerEvent &o) = default;

        bool operator==(const PointerEvent &other) const {
            return (
                signature_ == other.signature_ && x_position_ == other.x_position_ &&
                y_position_ == other.y_position_ && width_ == other.width_ &&
                height_ == other.height_ && wheel_delta_ == other.wheel_delta_ &&
                wheel_delta_unit_ == other.wheel_delta_unit_ && pressure_ == other.pressure_ &&
                timestamp_ == other.timestamp_);
        }

        // bool operator==(const Signature &o) const { return signature_ == o; }

        friend std::ostream &operator<<(std::ostream &out, const PointerEvent &o) {
            out << "PointerEvent " << (int)o.type() << " " << (int)o.buttons() << " "
                << (int)o.modifiers() << " " << o.x_position_ << " " << o.y_position_ << " "
                << o.position_in_viewport_coord_sys_.y << " "
                << o.position_in_viewport_coord_sys_.x << " " << o.width_ << " " << o.height_
                << " " << o.wheel_delta_.first << " " << o.wheel_delta_.second << " "
                << o.wheel_delta_unit_ << "" << o.pressure_ << " " << o.timestamp_ << " "
                << (int)o.input_type() << " " << (int)o.pointer_type();
            return out;
        }

        template <class Inspector> friend bool inspect(Inspector &f, PointerEvent &x) {
            return f.object(x).fields(
                f.field("sig", x.signature_),
                f.field("x", x.x_position_),
                f.field("y", x.y_position_),
                f.field("pos_in_cs", x.position_in_viewport_coord_sys_),
                f.field("vp_du_dx", x.viewport_pixel_scale_),
                f.field("w", x.width_),
                f.field("h", x.height_),
                f.field("wd", x.wheel_delta_),
                f.field("wd_unit", x.wheel_delta_unit_),
                f.field("ts", x.timestamp_));
        }

        void override_signature(const Signature &sig) { signature_ = sig; }

        [[nodiscard]] int x() const { return x_position_; }
        [[nodiscard]] int y() const { return y_position_; }
        [[nodiscard]] Imath::V2f position_in_viewport_coord_sys() const {
            return position_in_viewport_coord_sys_;
        }
        [[nodiscard]] float viewport_pixel_scale() const { return viewport_pixel_scale_; }
        [[nodiscard]] int width() const { return width_; }
        [[nodiscard]] int height() const { return height_; }
        [[nodiscard]] std::pair<int, int> wheel_delta() const { return wheel_delta_; }
        [[nodiscard]] int wheel_delta_unit() const { return wheel_delta_unit_; }
        [[nodiscard]] EventType type() const { return signature_.type_; }
        [[nodiscard]] int buttons() const { return signature_.buttons_; }
        [[nodiscard]] int modifiers() const { return signature_.modifiers_; }
        [[nodiscard]] const Signature &signature() const { return signature_; }
        [[nodiscard]] const std::string &context() const { return context_; }
        [[nodiscard]] float pressure() const { return pressure_; }
        [[nodiscard]] double timestamp() const { return timestamp_; }
        [[nodiscard]] int input_type() const { return signature_.input_type_; }
        [[nodiscard]] int pointer_type() const { return signature_.pointer_type_; }

        void set_pos_in_coord_sys(const float x, const float y, const float du_dx) {
            position_in_viewport_coord_sys_.x = x;
            position_in_viewport_coord_sys_.y = y;
            viewport_pixel_scale_             = du_dx;
        }

        // utility::time_point w_;

      private:
        Signature signature_;
        int x_position_, y_position_;
        int width_, height_;
        std::pair<int, int> wheel_delta_;
        int wheel_delta_unit_;
        std::string context_;
        float pressure_;
        double timestamp_;
        Imath::V2f position_in_viewport_coord_sys_ = Imath::V2f{
            std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
        float viewport_pixel_scale_ = {0.01f};
    };

} // namespace ui
} // namespace xstudio
