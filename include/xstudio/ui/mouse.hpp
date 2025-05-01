// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/type_id.hpp>
#include <ostream>
#include <utility>
#include <Imath/ImathVec.h>

#include "xstudio/utility/chrono.hpp"
#include "xstudio/ui/enums.hpp"

namespace xstudio {
namespace ui {

    // enum class EventType { Move, Drag, ButtonDown, ButtonRelease, MouseWheel, DoubleClick };

    struct Signature {


        enum Button {
            None            = 0,
            Left            = 1,
            Middle          = 4,
            Right           = 2,
            LeftRight       = 3,
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

        Signature(
            const EventType type = EventType::Move,
            const int buttons    = Button::None,
            const int modifiers  = Modifier::NoModifier)
            : type_(type), buttons_(buttons), modifiers_(modifiers) {}

        Signature(const Signature &o)

            = default;

        bool operator==(const Signature &o) const {
            return type_ == o.type_ && buttons_ == o.buttons_ && modifiers_ == o.modifiers_;
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
            return false;
        }

        Signature &operator=(const Signature &o) = default;

        friend std::ostream &operator<<(std::ostream &out, const Signature &o) {
            out << (int)o.type_ << " " << (int)o.buttons_ << " " << (int)o.modifiers_;
            return out;
        }

        template <class Inspector> friend bool inspect(Inspector &f, Signature &x) {
            return f.object(x).fields(
                f.field("type", x.type_),
                f.field("but", x.buttons_),
                f.field("mode", x.modifiers_));
        }

        EventType type_;
        int buttons_;
        int modifiers_;
    };

    class PointerEvent {

      public:
        PointerEvent(const PointerEvent &o) = default;

        PointerEvent(
            EventType t                = EventType::Move,
            Signature::Button b        = Signature::Button::None,
            int x                      = 0,
            int y                      = 0,
            int w                      = 0,
            int h                      = 0,
            int m                      = Signature::Modifier::NoModifier,
            std::string ctx            = std::string(),
            std::pair<int, int> adelta = std::make_pair(0, 0),
            std::pair<int, int> pdelta = std::make_pair(0, 0))
            : signature_(t, b, m),
              x_position_(x),
              y_position_(y),
              width_(w),
              height_(h),
              angle_delta_(std::move(adelta)),
              pixel_delta_(std::move(pdelta)),
              context_(std::move(ctx)) {}

        PointerEvent &operator=(const PointerEvent &o) = default;

        bool operator==(const PointerEvent &other) const {
            return (
                signature_ == other.signature_ and x_position_ == other.x_position_ and
                y_position_ == other.y_position_ and width_ == other.width_ and
                height_ == other.height_ and angle_delta_ == other.angle_delta_ and
                pixel_delta_ == other.pixel_delta_);
        }

        bool operator==(const Signature &o) const { return signature_ == o; }

        friend std::ostream &operator<<(std::ostream &out, const PointerEvent &o) {
            out << "PointerEvent " << (int)o.type() << " " << (int)o.buttons() << " "
                << (int)o.modifiers() << " " << o.x_position_ << " " << o.y_position_ << " "
                << o.position_in_viewport_coord_sys_.y << " "
                << o.position_in_viewport_coord_sys_.x << " "
                << " " << o.width_ << " " << o.height_ << " " << o.angle_delta_.first << " "
                << o.angle_delta_.second;
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
                f.field("a", x.angle_delta_),
                f.field("p", x.pixel_delta_));
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
        [[nodiscard]] std::pair<int, int> angle_delta() const { return angle_delta_; }
        [[nodiscard]] std::pair<int, int> pixel_delta() const { return pixel_delta_; }
        [[nodiscard]] EventType type() const { return signature_.type_; }
        [[nodiscard]] int buttons() const { return signature_.buttons_; }
        [[nodiscard]] int modifiers() const { return signature_.modifiers_; }
        [[nodiscard]] const Signature &signature() const { return signature_; }
        [[nodiscard]] const std::string &context() const { return context_; }

        void set_pos_in_coord_sys(const float x, const float y, const float du_dx) {
            position_in_viewport_coord_sys_.x = x;
            position_in_viewport_coord_sys_.y = y;
            viewport_pixel_scale_             = du_dx;
        }

        utility::time_point w_;

      private:
        Signature signature_;
        int x_position_, y_position_;
        int width_, height_;
        std::pair<int, int> angle_delta_;
        std::pair<int, int> pixel_delta_;
        std::string context_;
        Imath::V2f position_in_viewport_coord_sys_ = Imath::V2f{
            std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
        float viewport_pixel_scale_ = {0.01f};
    };
} // namespace ui
} // namespace xstudio
