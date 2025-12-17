// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/ui/canvas/canvas.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class Annotation : public bookmark::AnnotationBase {

          public:
            explicit Annotation();
            explicit Annotation(const utility::JsonStore &s);
            explicit Annotation(const Annotation &o) = default;

            bool operator==(const Annotation &o) const {
                return canvas_ == o.canvas_ && is_laser_annotation_ == o.is_laser_annotation_;
            }

            [[nodiscard]] utility::JsonStore
            serialise(utility::Uuid &plugin_uuid) const override;

            size_t hash() const override { return canvas_.hash(); }

            xstudio::ui::canvas::Canvas &canvas() { return canvas_; }
            const xstudio::ui::canvas::Canvas &canvas() const { return canvas_; }

            // this allows other parts of the app (notably the Sync plugin) to
            // access the canvas from the AnnotationBase, because 'Annotation' 
            // is only defined here in the plugin, not in xSTUDIO's general API
            const void * user_data() const override { return reinterpret_cast<const void*>(&canvas_); }

          private:
            bool is_laser_annotation_{false};
            xstudio::ui::canvas::Canvas canvas_;
        };

        typedef std::shared_ptr<Annotation> AnnotationPtr;

    } // end namespace viewport
} // end namespace ui
} // end namespace xstudio