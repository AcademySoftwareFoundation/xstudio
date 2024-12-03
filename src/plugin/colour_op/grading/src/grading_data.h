// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <vector>

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/ui/canvas/canvas.hpp"

namespace xstudio {
namespace ui {
namespace viewport {

    struct Grade {
        std::array<double, 4> slope  {1.0, 1.0, 1.0, 1.0};
        std::array<double, 4> offset {0.0, 0.0, 0.0, 0.0};
        std::array<double, 4> power  {1.0, 1.0, 1.0, 1.0};
        double sat {1.0};
        double exposure {0.0};
        double contrast {1.0};

        bool operator==(const Grade &o) const {
            return (
                slope == o.slope &&
                offset == o.offset &&
                power == o.power &&
                sat == o.sat &&
                exposure == o.exposure &&
                contrast == o.contrast
            );
        }
    };

    void from_json(const nlohmann::json &j, Grade &g);
    void to_json(nlohmann::json &j, const Grade &g);

    class GradingData : public bookmark::AnnotationBase {
      public:
        GradingData() = default;
        explicit GradingData(const utility::JsonStore &s);

        bool operator==(const GradingData &o) const {
            return (
                colour_space_ == o.colour_space_ &&
                grade_ == o.grade_ &&
                mask_active_ == o.mask_active_ &&
                mask_editing_ == o.mask_editing_ &&
                mask_ == o.mask_
            ); 
        }

        [[nodiscard]] utility::JsonStore serialise(utility::Uuid &plugin_uuid) const override;

        bool identity() const;

        void set_colour_space(const std::string &val) { colour_space_ = val; }
        std::string colour_space() const { return colour_space_; }

        Grade & grade() { return grade_; }
        const Grade & grade() const { return grade_; }

        void set_mask_active(bool val) { mask_active_ = val; }
        bool mask_active() const { return mask_active_; }

        void set_mask_editing(bool val) { mask_editing_ = val; }
        bool mask_editing() const { return mask_editing_; }

        canvas::Canvas & mask() { return mask_; }
        const canvas::Canvas & mask() const { return mask_; }

      private:
        friend void from_json(const nlohmann::json &j, GradingData &gd);
        friend void to_json(nlohmann::json &j, const GradingData &gd);

        std::string colour_space_ {"scene_linear"};
        Grade grade_;
        bool mask_active_ {false};
        bool mask_editing_ {false};
        canvas::Canvas mask_;
    };

    void from_json(const nlohmann::json &j, GradingData &gd);
    void to_json(nlohmann::json &j, const GradingData &gd);

    typedef std::shared_ptr<GradingData> GradingDataPtr;

    // This struct is a convenience wrapper to bundle GradingData
    // with other metadatas living in the bookmark user_data field.
    struct GradingInfo {
        const GradingData* data;
        bool isActive;
    };

} // end namespace viewport
} // end namespace ui
} // end namespace xstudio