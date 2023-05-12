// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/module/attribute.hpp"

namespace xstudio {

namespace module {

    template <typename T> class TypeAttribute : public Attribute {
      protected:
        TypeAttribute(
            const std::string &title,
            const std::string &abbr_title,
            const T &value,
            const std::string &type_name)
            : Attribute(title, abbr_title, type_name) {
            role_data_[Value] = value;
        }

      public:
        [[nodiscard]] T value() const { return get_role_data<T>(Value); }
        void set_value(const T &v, const bool notify = true) {
            set_role_data(Value, v, notify);
        }
    };

    class ActionAttribute : public Attribute {
      public:
        ActionAttribute(const std::string &title, const std::string &abbr_title)
            : Attribute(title, abbr_title, type_name(Attribute::ActionAttribute)) {}
    };

    class FloatAttribute : public TypeAttribute<float> {
      public:
        FloatAttribute(
            const std::string &title,
            const std::string &abbr_title,
            const float value,
            const float float_min,
            const float float_max,
            const float float_step,
            const int float_display_decimals,
            const float fscrub_sensitivity);
    };

    class StringAttribute : public TypeAttribute<std::string> {
      public:
        StringAttribute(
            const std::string &title, const std::string &abbr_title, const std::string &value);
    };

    class StringChoiceAttribute : public TypeAttribute<std::string> {
      public:
        StringChoiceAttribute(
            const std::string &title,
            const std::string &abbr_title                = "",
            const std::string &value                     = "",
            const std::vector<std::string> &options      = std::vector<std::string>(),
            const std::vector<std::string> &abbr_options = std::vector<std::string>());

        [[nodiscard]] std::vector<std::string> options() const {
            return get_role_data<std::vector<std::string>>(StringChoices);
        }
    };

    class BooleanAttribute : public TypeAttribute<bool> {
      public:
        BooleanAttribute(
            const std::string &title, const std::string &abbr_title, const bool value);
    };

    class QmlCodeAttribute : public TypeAttribute<std::string> {
      public:
        QmlCodeAttribute(const std::string &title, const std::string &code);
    };

    class IntegerAttribute : public TypeAttribute<int> {
      public:
        IntegerAttribute(
            const std::string &title,
            const std::string &abbr_title,
            const int value,
            const int int_min = std::numeric_limits<int>::lowest(),
            const int int_max = std::numeric_limits<int>::max());
    };

    class ColourAttribute : public TypeAttribute<utility::ColourTriplet> {
      public:
        ColourAttribute(
            const std::string &title,
            const std::string &abbr_title,
            const utility::ColourTriplet &value);
    };

    class JsonAttribute : public TypeAttribute<nlohmann::json> {
      public:
        JsonAttribute(
            const std::string &title,
            const std::string &abbr_title,
            const nlohmann::json &value);
    };

} // namespace module
} // namespace xstudio