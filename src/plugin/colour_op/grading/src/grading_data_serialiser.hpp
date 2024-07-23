// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "grading_data.h"

namespace xstudio {
namespace ui {
    namespace viewport {

        class GradingDataSerialiser {

          public:
            GradingDataSerialiser() = default;

            static utility::JsonStore serialise(const GradingData *);
            static void deserialise(GradingData *, const utility::JsonStore &);

            virtual void _serialise(const GradingData *, nlohmann::json &) const = 0;
            virtual void _deserialise(GradingData *, const nlohmann::json &)     = 0;

            static void register_serialiser(
                const unsigned char maj_ver,
                const unsigned char minor_ver,
                std::shared_ptr<GradingDataSerialiser> sptr);

          private:
            static std::map<long, std::shared_ptr<GradingDataSerialiser>> serialisers;
        };

#define RegisterGradingDataSerialiser(serialiser_class, v_maj, v_min)                          \
    class serialiser_class##_register_cls {                                                    \
      public:                                                                                  \
        serialiser_class##_register_cls() {                                                    \
            GradingDataSerialiser::register_serialiser(                                        \
                v_maj, v_min, std::shared_ptr<GradingDataSerialiser>(new serialiser_class())); \
        }                                                                                      \
    };                                                                                         \
    serialiser_class##_register_cls serialiser_class##_serialiser_register_rinst;

    } // end namespace viewport
} // end namespace ui
} // end namespace xstudio