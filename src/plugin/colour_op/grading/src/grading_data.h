// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/ui/canvas/canvas.hpp"

namespace xstudio {
namespace ui {
namespace viewport {

    struct Grade {
        std::array<double, 4> slope  {1.0, 1.0, 1.0, 0.0};
        std::array<double, 4> offset {0.0, 0.0, 0.0, 0.0};
        std::array<double, 4> power  {1.0, 1.0, 1.0, 1.0};
        double sat {1.0};

        bool operator==(const Grade &o) const {
            return (
                slope == o.slope &&
                offset == o.offset &&
                power == o.power &&
                sat == o.sat
            );
        }
    };

    void from_json(const nlohmann::json &j, Grade &g);
    void to_json(nlohmann::json &j, const Grade &g);

    class LayerData {
      public:
        LayerData() = default;

        bool operator==(const LayerData &o) const {
            return (
                grade_ == o.grade_ &&
                mask_active_ == o.mask_active_ &&
                mask_editing_ == o.mask_editing_ &&
                mask_ == o.mask_
            ); 
        }

        bool identity() const;

        Grade & grade() { return grade_; }
        const Grade & grade() const { return grade_; }

        void set_mask_active(bool val) { mask_active_ = val; }
        bool mask_active() const { return mask_active_; }

        void set_mask_editing(bool val) { mask_editing_ = val; }
        bool mask_editing() const { return mask_editing_; }

        canvas::Canvas & mask() { return mask_; }
        const canvas::Canvas & mask() const { return mask_; }

      private:
        friend void from_json(const nlohmann::json &j, LayerData &l);
        friend void to_json(nlohmann::json &j, const LayerData &l);

        Grade grade_;
        bool mask_active_ {false};
        bool mask_editing_ {false};
        canvas::Canvas mask_;
    };

    void from_json(const nlohmann::json &j, LayerData &l);
    void to_json(nlohmann::json &j, const LayerData &l);

    class GradingData : public bookmark::AnnotationBase {
      public:
        GradingData() = default;
        explicit GradingData(const utility::JsonStore &s);

        GradingData & operator=(const GradingData &o) = default;

        bool operator==(const GradingData &o) const {
            return (layers_ == o.layers_);
        }

        [[nodiscard]] utility::JsonStore serialise(utility::Uuid &plugin_uuid) const override;

        bool identity() const;

        size_t size() const { return layers_.size(); }

        std::vector<LayerData>::const_iterator begin() const {
            return layers_.begin(); }
        std::vector<LayerData>::const_iterator end() const {
            return layers_.end(); }

        std::vector<LayerData>& layers() { return layers_; }
        const std::vector<LayerData>& layers() const { return layers_; }

        LayerData* layer(size_t idx);
        void push_layer();
        void pop_layer();

      private:
        friend void from_json(const nlohmann::json &j, GradingData &gd);
        friend void to_json(nlohmann::json &j, const GradingData &gd);

        std::vector<LayerData> layers_;
    };

    void from_json(const nlohmann::json &j, GradingData &gd);
    void to_json(nlohmann::json &j, const GradingData &gd);

    typedef std::shared_ptr<GradingData> GradingDataPtr;

} // end namespace viewport
} // end namespace ui
} // end namespace xstudio