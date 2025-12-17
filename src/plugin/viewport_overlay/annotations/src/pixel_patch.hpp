// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/ui/canvas/canvas.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class PixelPatch {

          public:
            PixelPatch() = default;

            void hide() {
                std::lock_guard l(m_);
                hidden_ = true;
            }

            void update(
                const std::vector<Imath::V4f> &patch_vertex_data,
                const Imath::V2f &position,
                const bool pressed,
                const bool hide_annotations,
                const std::string &viewport_name = std::string()) {
                std::lock_guard l(m_);
                hidden_            = false;
                patch_vertex_data_ = patch_vertex_data;
                position_          = position;
                pressed_           = pressed;
                hide_annotations_  = hide_annotations;
                if (!viewport_name.empty())
                    viewport_name_ = viewport_name;
            }

            [[nodiscard]] bool skip_render_of_drawings(const std::string &viewport_name) const {
                std::lock_guard l(m_);
                return !hidden_ && hide_annotations_ && viewport_name == viewport_name_;
            }

            [[nodiscard]] bool skip_render(const std::string &viewport_name) const {
                std::lock_guard l(m_);
                return hidden_ || patch_vertex_data_.empty() || viewport_name != viewport_name_;
            }

            [[nodiscard]] const std::vector<Imath::V4f> &patch_vertex_data() const {
                return patch_vertex_data_;
            }

            [[nodiscard]] const Imath::V2f &position() const { return position_; }

            [[nodiscard]] bool hidden() const { return hidden_; }

            [[nodiscard]] bool hide_annotations() const { return hide_annotations_; }

            [[nodiscard]] bool pressed() const { return pressed_; }

            std::mutex &mutex() { return m_; }

          private:
            mutable std::mutex m_;
            std::vector<Imath::V4f> patch_vertex_data_;
            Imath::V2f position_;
            bool hidden_           = true;
            bool pressed_          = false;
            bool hide_annotations_ = false;
            std::string viewport_name_;
        };

    } // end namespace viewport
} // end namespace ui
} // end namespace xstudio