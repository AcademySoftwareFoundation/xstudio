// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "annotation.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class AnnotationSerialiser {

          public:
            AnnotationSerialiser() = default;

            static utility::JsonStore serialise(const Annotation *);
            static void deserialise(Annotation *anno, const utility::JsonStore &data);

            virtual void _serialise(const Annotation *, nlohmann::json &) const = 0;
            virtual void _deserialise(Annotation *, const nlohmann::json &data) = 0;

            static void register_serialiser(
                const unsigned char maj_ver,
                const unsigned char minor_ver,
                std::shared_ptr<AnnotationSerialiser> sptr);

          private:
            static std::map<long, std::shared_ptr<AnnotationSerialiser>> serialisers;
        };

#define RegisterAnnotationSerialiser(serialiser_class, v_maj, v_min)                           \
    class serialiser_class##_register_cls {                                                    \
      public:                                                                                  \
        serialiser_class##_register_cls() {                                                    \
            AnnotationSerialiser::register_serialiser(                                         \
                v_maj, v_min, std::shared_ptr<AnnotationSerialiser>(new serialiser_class()));  \
        }                                                                                      \
    };                                                                                         \
    serialiser_class##_register_cls serialiser_class##_serialiser_register_rinst;

    } // end namespace viewport
} // end namespace ui
} // end namespace xstudio