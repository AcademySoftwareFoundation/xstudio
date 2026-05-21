// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/uuid.hpp"
#include <caf/all.hpp>

namespace xstudio::ui::viewport {

/*Simple struct for defining Viewport masks - i.e. black bars, safe areas, etc.

Masks are rendered over the image by the MaskRenderer plugin, which receives details
of masks to render from other plugins (e.g. the basic_viewport_mask plugin) via
the messaging system. See BasicViewportMasking for an example of how
to control mask rendering from another plugin.
*/
struct Mask {

    Mask() = default;

    Mask(const Mask &o) = default;

    Mask(
        float l,
        float r,
        float t,
        float b,
        float lt,
        float lo,
        float mo,
        const std::string lbl,
        float ls,
        bool af)
        : left_(l),
          right_(r),
          top_(t),
          bottom_(b),
          line_thickness_(lt),
          line_opacity_(lo),
          mask_opacity_(mo),
          label_(std::move(lbl)),
          label_size_(ls),
          auto_fit_(af) {
        make_hash();
    }

    [[nodiscard]] float left() const { return left_; }
    [[nodiscard]] float right() const { return right_; }
    [[nodiscard]] float top() const { return top_; }
    [[nodiscard]] float bottom() const { return bottom_; }
    [[nodiscard]] float line_thickness() const { return line_thickness_; }
    [[nodiscard]] float line_opacity() const { return line_opacity_; }
    [[nodiscard]] float mask_opacity() const { return mask_opacity_; }
    [[nodiscard]] std::string label() const { return label_; }
    [[nodiscard]] float label_size() const { return label_size_; }
    [[nodiscard]] bool auto_fit() const { return auto_fit_; }
    [[nodiscard]] size_t hash() const { return hash_value_; }
    [[nodiscard]] bool is_null() const {
        return left_ == 0.0f && right_ == 0.0f && top_ == 0.0f && bottom_ == 0.0f;
    }

  private:
    float left_           = 0.0f;
    float right_          = 0.0f;
    float top_            = 0.0f;
    float bottom_         = 0.0f;
    float line_thickness_ = 0.0f;
    float line_opacity_   = 0.0f;
    float mask_opacity_   = 0.0f;
    std::string label_;
    float label_size_  = 0.0f;
    bool auto_fit_     = false;
    size_t hash_value_ = 0;

    void make_hash() {
        hash_value_ = 0x9e3779b9;
        hash_combine(hash_value_, left_);
        hash_combine(hash_value_, right_);
        hash_combine(hash_value_, top_);
        hash_combine(hash_value_, bottom_);
        hash_combine(hash_value_, line_thickness_);
        hash_combine(hash_value_, line_opacity_);
        hash_combine(hash_value_, mask_opacity_);
        hash_combine(hash_value_, label_);
        hash_combine(hash_value_, label_size_);
        hash_combine(hash_value_, auto_fit_);
    }

    template <typename T> static void hash_combine(size_t &seed, const T &v) {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template <class Inspector> friend bool inspect(Inspector &f, Mask &x) {
        return f.object(x).fields(
            f.field("l", x.left_),
            f.field("r", x.right_),
            f.field("t", x.top_),
            f.field("b", x.bottom_),
            f.field("lt", x.line_thickness_),
            f.field("lo", x.line_opacity_),
            f.field("mo", x.mask_opacity_),
            f.field("lbl", x.label_),
            f.field("ls", x.label_size_),
            f.field("af", x.auto_fit_));
    }
};

inline std::string to_string(const Mask &m) {
    return "Mask(left=" + std::to_string(m.left()) + ", right=" + std::to_string(m.right()) +
           ", top=" + std::to_string(m.top()) + ", bottom=" + std::to_string(m.bottom()) +
           ", line_thickness=" + std::to_string(m.line_thickness()) +
           ", line_opacity=" + std::to_string(m.line_opacity()) +
           ", mask_opacity=" + std::to_string(m.mask_opacity()) + ", label=" + m.label() +
           ", label_size=" + std::to_string(m.label_size()) + ")";
}

} // namespace xstudio::ui::viewport
