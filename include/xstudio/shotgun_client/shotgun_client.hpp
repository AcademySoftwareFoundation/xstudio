// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <chrono>
#include <string>
#include <variant>
#include <vector>
#include <optional>

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#ifndef CPPHTTPLIB_ZLIB_SUPPORT
#define CPPHTTPLIB_ZLIB_SUPPORT
#endif
#include <cpp-httplib/httplib.h>


#include "xstudio/shotgun_client/enums.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/json_store.hpp"

#include <nlohmann/json.hpp>

namespace xstudio {
namespace shotgun_client {

    struct JSONNull {
        JSONNull()  = default;
        ~JSONNull() = default;
    };

    enum class Period { HOUR = 0, DAY, WEEK, MONTH, YEAR };
    enum class BoolOperator { AND = 0, OR };
    enum class ConditionalOperator {
        NONE = 0,
        IS,
        IS_NOT,
        LESS_THAN,
        GREATER_THAN,
        IN_OPERATOR,
        NOT_IN,
        BETWEEN,
        NOT_BETWEEN,
        CONTAINS,
        NOT_CONTAINS,
        STARTS_WITH,
        ENDS_WITH,
        IN_CALENDAR_DAY,
        IN_CALENDAR_WEEK,
        IN_CALENDAR_MONTH,
        IN_CALENDAR_YEAR,
        IN_LAST,
        NOT_IN_LAST,
        IN_NEXT,
        NOT_IN_NEXT,
        TYPE_IS,
        TYPE_IS_NOT,
        NAME_CONTAINS,
        NAME_NOT_CONTAINS,
        NAME_STARTS_WITH,
        NAME_ENDS_WITH,
        NAME_IS
    };


    static std::map<ConditionalOperator, std::string> ConditionalOperatorMap = {
        {ConditionalOperator::NONE, "none"},
        {ConditionalOperator::IS, "is"},
        {ConditionalOperator::IS_NOT, "is_not"},
        {ConditionalOperator::LESS_THAN, "less_than"},
        {ConditionalOperator::GREATER_THAN, "greater_than"},
        {ConditionalOperator::IN_OPERATOR, "in"},
        {ConditionalOperator::NOT_IN, "not_in"},
        {ConditionalOperator::BETWEEN, "between"},
        {ConditionalOperator::NOT_BETWEEN, "not_between"},
        {ConditionalOperator::CONTAINS, "contains"},
        {ConditionalOperator::NOT_CONTAINS, "not_contains"},
        {ConditionalOperator::STARTS_WITH, "starts_with"},
        {ConditionalOperator::ENDS_WITH, "ends_with"},
        {ConditionalOperator::IN_CALENDAR_DAY, "in_calendar_day"},
        {ConditionalOperator::IN_CALENDAR_WEEK, "in_calendar_week"},
        {ConditionalOperator::IN_CALENDAR_MONTH, "in_calendar_month"},
        {ConditionalOperator::IN_CALENDAR_YEAR, "in_calendar_year"},
        {ConditionalOperator::IN_LAST, "in_last"},
        {ConditionalOperator::NOT_IN_LAST, "not_in_last"},
        {ConditionalOperator::IN_NEXT, "in_next"},
        {ConditionalOperator::NOT_IN_NEXT, "not_in_next"},
        {ConditionalOperator::TYPE_IS, "type_is"},
        {ConditionalOperator::TYPE_IS_NOT, "type_is_not"},
        {ConditionalOperator::NAME_CONTAINS, "name_contains"},
        {ConditionalOperator::NAME_NOT_CONTAINS, "name_not_contains"},
        {ConditionalOperator::NAME_STARTS_WITH, "name_starts_with"},
        {ConditionalOperator::NAME_ENDS_WITH, "name_ends_with"},
        {ConditionalOperator::NAME_IS, "name_is"}};

    inline std::string to_string(const ConditionalOperator &p) {
        const auto i = ConditionalOperatorMap.find(p);
        if (i != ConditionalOperatorMap.end())
            return i->second;

        return "UNKNOWN";
    }

    std::optional<ConditionalOperator>
    conditional_operator_from_string(const std::string &value);

    static std::map<BoolOperator, std::string> BoolOperatorMap = {
        {BoolOperator::AND, "and"}, {BoolOperator::OR, "or"}};

    inline std::string to_string(const BoolOperator &p) {
        const auto i = BoolOperatorMap.find(p);
        if (i != BoolOperatorMap.end())
            return i->second;

        return "UNKNOWN";
    }

    std::optional<BoolOperator> bool_operator_from_string(const std::string &value);

    const static std::map<Period, std::string> PeriodMap = {
        {Period::HOUR, "HOUR"},
        {Period::DAY, "DAY"},
        {Period::WEEK, "WEEK"},
        {Period::MONTH, "MONTH"},
        {Period::YEAR, "YEAR"},
    };

    inline std::string to_string(const Period &p) {
        const auto i = PeriodMap.find(p);
        if (i != PeriodMap.end())
            return i->second;

        return "UNKNOWN";
    }

    std::optional<Period> period_from_string(const std::string &value);

    inline void to_json(nlohmann::json &j, const JSONNull &) { j = nullptr; }
    inline void from_json(const nlohmann::json &, JSONNull &) {}

    class Op {
      public:
        Op(const std::string class_name = "Op") : class_name_(std::move(class_name)) {}
        virtual ~Op() = default;
        virtual operator nlohmann::json() const { return nlohmann::json(); }
        virtual utility::JsonStore serialise() const = 0;
        std::string class_name() const { return class_name_; }

      private:
        std::string class_name_;
    };

    template <typename T> class Field : public Op {
      public:
        typedef std::variant<
            int32_t,
            std::pair<int32_t, Period>,
            T,
            std::vector<T>,
            std::pair<T, T>,
            std::monostate>
            FieldType;

        Field(const std::string class_name, const std::string field_name)
            : Op(std::move(class_name)), field_name_(std::move(field_name)) {}
        Field(const utility::JsonStore &jsn) : Op(jsn.at("class")) {
            enabled_       = jsn.value("enabled", true);
            field_name_    = jsn.at("field");
            auto condition = conditional_operator_from_string(jsn.at("op"));
            if (condition)
                condition_ = *condition;
            null_ = jsn.at("is_null");
            if (not null_) {
                switch (jsn.at("index").get<int>()) {
                case 0:
                    value_ = jsn.at("value").get<int32_t>();
                    break;
                case 1: {
                    auto period = period_from_string(jsn.at("value")[1].get<std::string>());
                    if (period)
                        value_ = std::pair<int32_t, Period>(
                            jsn.at("value")[0].get<int32_t>(), *period);
                } break;
                case 2:
                    value_ = jsn.at("value").get<T>();
                    break;
                case 3:
                    value_ = jsn.at("value").get<std::vector<T>>();
                    break;
                case 4:
                    value_ = jsn.at("value").get<std::pair<T, T>>();
                    break;
                }
            }
        }

        ~Field() override = default;

        operator nlohmann::json() const override {
            return std::visit(
                [field_name = field_name_, condition = condition_, null = null_](
                    auto &&arg) -> nlohmann::json {
                    using TT = std::decay_t<decltype(arg)>;
                    nlohmann::json result;

                    if constexpr (std::is_same_v<std::monostate, TT>) {
                        result =
                            nlohmann::json::array({field_name, to_string(condition), nullptr});
                    } else if constexpr (std::is_same_v<std::pair<int32_t, Period>, TT>) {
                        result = nlohmann::json::array(
                            {field_name,
                             to_string(condition),
                             nlohmann::json::array(
                                 {arg.first, to_string(static_cast<Period>(arg.second))})});
                    } else if (null) {
                        result =
                            nlohmann::json::array({field_name, to_string(condition), nullptr});
                    } else
                        result = nlohmann::json::array({field_name, to_string(condition), arg});

                    return result;
                },
                value_);
        }

        utility::JsonStore serialise() const override {
            utility::JsonStore jsn;
            jsn["class"]   = class_name();
            jsn["field"]   = field_name_;
            jsn["op"]      = to_string(condition_);
            jsn["is_null"] = null_;
            jsn["index"]   = value_.index();
            jsn["enabled"] = enabled_;

            if (value_.index() == 1) {
                auto i       = std::get<std::pair<int32_t, Period>>(value_);
                jsn["value"] = std::pair<int32_t, std::string>(i.first, to_string(i.second));
            } else
                std::visit(
                    [jj = &jsn](auto &&arg) {
                        using TT = std::decay_t<decltype(arg)>;
                        if constexpr (std::is_same_v<std::monostate, TT>) {
                            (*jj)["value"] = nullptr;
                        } else
                            (*jj)["value"] = static_cast<nlohmann::json>(arg);
                    },
                    value_);

            return jsn;
        }


        ConditionalOperator op() const { return condition_; }
        std::string field() const { return field_name_; }
        std::string string_value() const {
            return std::visit(
                [null = null_](auto &&arg) -> std::string {
                    using TT = std::decay_t<decltype(arg)>;
                    std::string result;

                    if constexpr (std::is_same_v<std::monostate, TT>) {
                        result = "null";
                    } else if constexpr (std::is_same_v<std::pair<int32_t, Period>, TT>) {
                        result = nlohmann::json::array(
                                     {arg.first, to_string(static_cast<Period>(arg.second))})
                                     .dump();
                    } else if (null) {
                        result = "null";
                    } else if constexpr (std::is_same_v<std::string, TT>) {
                        result = arg;
                    } else
                        result = nlohmann::json(arg).dump();

                    return result;
                },
                value_);
        }
        std::string json_value() const {
            return std::visit(
                [null = null_](auto &&arg) -> std::string {
                    using TT = std::decay_t<decltype(arg)>;
                    std::string result;

                    if constexpr (std::is_same_v<std::monostate, TT>) {
                        result = "null";
                    } else if (null) {
                        result = "null";
                    } else
                        result = nlohmann::json(arg).dump();

                    return result;
                },
                value_);
        }
        FieldType value() const { return value_; }
        bool null() const { return null_; }
        bool enabled() const { return enabled_; }
        void set_enabled(const bool value) { enabled_ = value; }

      protected:
        Field &is(const T value) {
            condition_ = ConditionalOperator::IS;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &name_is(const T value) {
            condition_ = ConditionalOperator::NAME_IS;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &name_contains(const T value) {
            condition_ = ConditionalOperator::NAME_CONTAINS;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &name_not_contains(const T value) {
            condition_ = ConditionalOperator::NAME_NOT_CONTAINS;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &name_starts_with(const T value) {
            condition_ = ConditionalOperator::NAME_STARTS_WITH;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &name_ends_with(const T value) {
            condition_ = ConditionalOperator::NAME_ENDS_WITH;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &is_not(const T value) {
            condition_ = ConditionalOperator::IS_NOT;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &between(const T &value1, const T &value2) {
            condition_ = ConditionalOperator::BETWEEN;
            value_     = std::move(std::make_pair(value1, value2));
            null_      = false;
            return *this;
        }

        Field &not_between(const T &value1, const T &value2) {
            condition_ = ConditionalOperator::NOT_BETWEEN;
            value_     = std::move(std::make_pair(value1, value2));
            null_      = false;
            return *this;
        }

        Field &contains(const T value) {
            condition_ = ConditionalOperator::CONTAINS;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &not_contains(const T value) {
            condition_ = ConditionalOperator::NOT_CONTAINS;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &starts_with(const T value) {
            condition_ = ConditionalOperator::STARTS_WITH;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &ends_with(const T value) {
            condition_ = ConditionalOperator::ENDS_WITH;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &type_is(const T value) {
            condition_ = ConditionalOperator::TYPE_IS;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &type_is_not(const T value) {
            condition_ = ConditionalOperator::TYPE_IS_NOT;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &is_null() {
            condition_ = ConditionalOperator::IS;
            null_      = true;
            return *this;
        }

        Field &is_not_null() {
            condition_ = ConditionalOperator::IS_NOT;
            null_      = true;
            return *this;
        }

        Field &less_than(const T value) {
            condition_ = ConditionalOperator::LESS_THAN;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &greater_than(const T value) {
            condition_ = ConditionalOperator::GREATER_THAN;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &in(const std::vector<T> value) {
            condition_ = ConditionalOperator::IN_OPERATOR;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &not_in(const std::vector<T> value) {
            condition_ = ConditionalOperator::NOT_IN;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &contains(const std::vector<T> value) {
            condition_ = ConditionalOperator::CONTAINS;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &not_contains(const std::vector<T> value) {
            condition_ = ConditionalOperator::NOT_CONTAINS;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &starts_with(const std::vector<T> value) {
            condition_ = ConditionalOperator::STARTS_WITH;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &ends_with(const std::vector<T> value) {
            condition_ = ConditionalOperator::ENDS_WITH;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &in_calendar_day(const int32_t value) {
            condition_ = ConditionalOperator::IN_CALENDAR_DAY;
            value_     = value;
            null_      = false;
            return *this;
        }
        Field &in_calendar_week(const int32_t value) {
            condition_ = ConditionalOperator::IN_CALENDAR_WEEK;
            value_     = value;
            null_      = false;
            return *this;
        }
        Field &in_calendar_month(const int32_t value) {
            condition_ = ConditionalOperator::IN_CALENDAR_MONTH;
            value_     = value;
            null_      = false;
            return *this;
        }
        Field &in_calendar_year(const int32_t value) {
            condition_ = ConditionalOperator::IN_CALENDAR_YEAR;
            value_     = value;
            null_      = false;
            return *this;
        }

        Field &in_last(const int32_t value, const Period period) {
            condition_ = ConditionalOperator::IN_LAST;
            value_     = std::pair<int32_t, Period>{value, period};
            null_      = false;
            return *this;
        }
        Field &not_in_last(const int32_t value, const Period period) {
            condition_ = ConditionalOperator::NOT_IN_LAST;
            value_     = std::pair<int32_t, Period>{value, period};
            null_      = false;
            return *this;
        }
        Field &in_next(const int32_t value, const Period period) {
            condition_ = ConditionalOperator::IN_NEXT;
            value_     = std::pair<int32_t, Period>{value, period};
            null_      = false;
            return *this;
        }
        Field &not_in_next(const int32_t value, const Period period) {
            condition_ = ConditionalOperator::NOT_IN_NEXT;
            value_     = std::pair<int32_t, Period>{value, period};
            null_      = false;
            return *this;
        }

      protected:
        bool null_    = {false};
        bool enabled_ = {true};
        std::string field_name_;
        ConditionalOperator condition_ = ConditionalOperator::NONE;
        FieldType value_               = {};
    };

    template <> class Field<int32_t> : public Op {
      public:
        typedef std::variant<
            int32_t,
            std::pair<int32_t, Period>,
            std::vector<int32_t>,
            std::pair<int32_t, int32_t>>
            FieldType;

        Field(const std::string class_name, const std::string field_name)
            : Op(std::move(class_name)), field_name_(std::move(field_name)) {}
        Field(const utility::JsonStore &jsn) : Op(jsn.at("class")) {
            enabled_       = jsn.value("enabled", true);
            field_name_    = jsn.at("field");
            auto condition = conditional_operator_from_string(jsn.at("op"));
            if (condition)
                condition_ = *condition;
            null_ = jsn.at("is_null");
            if (not null_) {
                switch (jsn.at("index").get<int>()) {
                case 0:
                    value_ = jsn.at("value").get<int32_t>();
                    break;
                case 1: {
                    auto period = period_from_string(jsn.at("value")[1].get<std::string>());
                    if (period)
                        value_ = std::pair<int32_t, Period>(
                            jsn.at("value")[0].get<int32_t>(), *period);
                } break;
                case 2:
                    value_ = jsn.at("value").get<std::vector<int32_t>>();
                    break;
                case 3:
                    value_ = jsn.at("value").get<std::pair<int32_t, int32_t>>();
                    break;
                }
            }
        }

        ~Field() override = default;

        operator nlohmann::json() const override {
            return std::visit(
                [field_name = field_name_, condition = condition_, null = null_](
                    auto &&arg) -> nlohmann::json {
                    using TT = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<std::monostate, TT>) {
                        return nlohmann::json::array(
                            {field_name, to_string(condition), nullptr});
                    } else if constexpr (std::is_same_v<std::pair<int32_t, Period>, TT>) {
                        return nlohmann::json::array(
                            {field_name,
                             to_string(condition),
                             nlohmann::json::array(
                                 {arg.first, to_string(static_cast<Period>(arg.second))})});
                    } else if (null) {
                        return nlohmann::json::array(
                            {field_name, to_string(condition), nullptr});
                    }

                    return nlohmann::json::array({field_name, to_string(condition), arg});
                },
                value_);
        }

        utility::JsonStore serialise() const override {
            utility::JsonStore jsn;
            jsn["class"]   = class_name();
            jsn["field"]   = field_name_;
            jsn["enabled"] = enabled_;
            jsn["op"]      = to_string(condition_);
            jsn["is_null"] = null_;
            jsn["index"]   = value_.index();
            if (value_.index() == 1) {
                auto i       = std::get<std::pair<int32_t, Period>>(value_);
                jsn["value"] = std::pair<int32_t, std::string>(i.first, to_string(i.second));
            } else
                std::visit(
                    [jj = &jsn](auto &&arg) {
                        (*jj)["value"] = static_cast<nlohmann::json>(arg);
                    },
                    value_);

            return jsn;
        }

        ConditionalOperator op() const { return condition_; }
        std::string field() const { return field_name_; }
        FieldType value() const { return value_; }
        bool null() const { return null_; }
        bool enabled() const { return enabled_; }
        void set_enabled(const bool value) { enabled_ = value; }

        std::string string_value() const {
            return std::visit(
                [null = null_](auto &&arg) -> std::string {
                    using TT = std::decay_t<decltype(arg)>;
                    std::string result;

                    if constexpr (std::is_same_v<std::monostate, TT>) {
                        result = "null";
                    } else if constexpr (std::is_same_v<std::pair<int32_t, Period>, TT>) {
                        result = nlohmann::json::array(
                                     {arg.first, to_string(static_cast<Period>(arg.second))})
                                     .dump();
                    } else if (null) {
                        result = "null";
                    } else
                        result = nlohmann::json(arg).dump();

                    return result;
                },
                value_);
        }
        std::string json_value() const {
            return std::visit(
                [null = null_](auto &&arg) -> std::string {
                    using TT = std::decay_t<decltype(arg)>;
                    std::string result;

                    if constexpr (std::is_same_v<std::monostate, TT>) {
                        result = "null";
                    } else if (null) {
                        result = "null";
                    } else
                        result = nlohmann::json(arg).dump();

                    return result;
                },
                value_);
        }

      protected:
        Field &is(const int32_t value) {
            condition_ = ConditionalOperator::IS;
            value_     = value;
            null_      = false;
            return *this;
        }

        Field &is_not(const int32_t value) {
            condition_ = ConditionalOperator::IS_NOT;
            value_     = value;
            null_      = false;
            return *this;
        }

        Field &is_null() {
            condition_ = ConditionalOperator::IS;
            null_      = true;
            return *this;
        }

        Field &is_not_null() {
            condition_ = ConditionalOperator::IS_NOT;
            null_      = true;
            return *this;
        }

        Field &less_than(const int32_t value) {
            condition_ = ConditionalOperator::LESS_THAN;
            value_     = value;
            null_      = false;
            return *this;
        }

        Field &greater_than(const int32_t value) {
            condition_ = ConditionalOperator::GREATER_THAN;
            value_     = value;
            null_      = false;
            return *this;
        }

        Field &in(const std::vector<int32_t> value) {
            condition_ = ConditionalOperator::IN_OPERATOR;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &not_in(const std::vector<int32_t> value) {
            condition_ = ConditionalOperator::NOT_IN;
            value_     = std::move(value);
            null_      = false;
            return *this;
        }

        Field &between(const int32_t value1, const int32_t value2) {
            condition_ = ConditionalOperator::BETWEEN;
            value_     = std::make_pair(value1, value2);
            null_      = false;
            return *this;
        }

        Field &not_between(const int32_t value1, const int32_t value2) {
            condition_ = ConditionalOperator::NOT_BETWEEN;
            value_     = std::make_pair(value1, value2);
            null_      = false;
            return *this;
        }

      protected:
        bool null_    = {false};
        bool enabled_ = {true};
        std::string field_name_;
        ConditionalOperator condition_ = ConditionalOperator::NONE;
        FieldType value_               = {};
    };

    template <typename T>
    inline std::ostream &operator<<(std::ostream &out, const Field<T> &op) {
        return out << nlohmann::json(op).dump(2);
    }

    template <typename T> class Numeric : public Field<T> {
      public:
        Numeric(const std::string class_name, const std::string field_name)
            : Field<T>(class_name, field_name) {}
        Numeric(const utility::JsonStore &jsn) : Field<T>(jsn) {}
        ~Numeric() override = default;

        Numeric<T> &is(const T value) {
            Field<T>::is(value);
            return *this;
        }

        Numeric<T> &is_not(const T value) {
            Field<T>::is_not(value);
            return *this;
        }

        Numeric<T> &less_than(const T value) {
            Field<T>::less_than(value);
            return *this;
        }

        Numeric<T> &greater_than(const T value) {
            Field<T>::greater_than(value);
            return *this;
        }

        Numeric<T> &is_null() {
            Field<T>::is_null();
            return *this;
        }

        Numeric<T> &is_not_null() {
            Field<T>::is_not_null();
            return *this;
        }

        Numeric<T> &in(const std::vector<T> &value) {
            Field<T>::in(value);
            return *this;
        }

        Numeric<T> &not_in(const std::vector<T> &value) {
            Field<T>::not_in(value);
            return *this;
        }

        Numeric<T> &between(const T value1, const T value2) {
            Field<T>::between(value1, value2);
            return *this;
        }

        Numeric<T> &not_between(const T value1, const T value2) {
            Field<T>::not_between(value1, value2);
            return *this;
        }
    };

    class Number : public Numeric<int32_t> {
      public:
        Number(const std::string field_name) : Numeric<int32_t>("Number", field_name) {}
        Number(const utility::JsonStore &jsn) : Numeric<int32_t>(jsn) {}

        ~Number() override = default;
    };

    class Timecode : public Numeric<int32_t> {
      public:
        Timecode(const std::string field_name) : Numeric<int32_t>("Timecode", field_name) {}
        Timecode(const utility::JsonStore &jsn) : Numeric<int32_t>(jsn) {}
        ~Timecode() override = default;
    };

    class Duration : public Numeric<int32_t> {
      public:
        Duration(const std::string field_name) : Numeric<int32_t>("Duration", field_name) {}
        Duration(const utility::JsonStore &jsn) : Numeric<int32_t>(jsn) {}
        ~Duration() override = default;
    };

    class Percent : public Numeric<float> {
      public:
        Percent(const std::string field_name) : Numeric<float>("Percent", field_name) {}
        Percent(const utility::JsonStore &jsn) : Numeric<float>(jsn) {}
        ~Percent() override = default;
    };

    class Currency : public Numeric<float> {
      public:
        Currency(const std::string field_name) : Numeric<float>("Currency", field_name) {}
        Currency(const utility::JsonStore &jsn) : Numeric<float>(jsn) {}
        ~Currency() override = default;
    };

    class Float : public Numeric<float> {
      public:
        Float(const std::string field_name) : Numeric<float>("Float", field_name) {}
        Float(const utility::JsonStore &jsn) : Numeric<float>(jsn) {}
        ~Float() override = default;
    };

    class Image : public Field<JSONNull> {
      public:
        Image(const std::string field_name) : Field<JSONNull>("Image", field_name) {}
        Image(const utility::JsonStore &jsn) : Field<JSONNull>(jsn) {}
        ~Image() override = default;

        Image &is() { return is_not_null(); }

        Image &is_not() { return is_null(); }

        Image &is_null() {
            Field<JSONNull>::is(JSONNull());
            return *this;
        }

        Image &is_not_null() {
            Field<JSONNull>::is_not(JSONNull());
            return *this;
        }
    };

    class Checkbox : public Field<bool> {
      public:
        Checkbox(const std::string field_name) : Field<bool>("Checkbox", field_name) {}
        Checkbox(const utility::JsonStore &jsn) : Field<bool>(jsn) {}
        ~Checkbox() override = default;

        Checkbox &is_null() {
            Field<bool>::is_null();
            return *this;
        }
        Checkbox &is_not_null() {
            Field<bool>::is_not_null();
            return *this;
        }
        Checkbox &is(const bool value) {
            Field<bool>::is(value);
            return *this;
        }
        Checkbox &is_not(const bool value) {
            Field<bool>::is_not(value);
            return *this;
        }
    };

    class List : public Field<std::string> {
      public:
        List(const std::string field_name, const std::string &class_name = "List")
            : Field<std::string>(class_name, field_name) {}
        List(const utility::JsonStore &jsn) : Field<std::string>(jsn) {}
        ~List() override = default;

        List &is(const std::string &value) {
            Field<std::string>::is(value);
            return *this;
        }

        List &is_not(const std::string &value) {
            Field<std::string>::is_not(value);
            return *this;
        }

        List &in(const std::vector<std::string> &value) {
            Field<std::string>::in(value);
            return *this;
        }

        List &not_in(const std::vector<std::string> &value) {
            Field<std::string>::not_in(value);
            return *this;
        }

        List &is_null() {
            Field<std::string>::is_null();
            return *this;
        }
        List &is_not_null() {
            Field<std::string>::is_not_null();
            return *this;
        }
    };

    class StatusList : public List {
      public:
        StatusList(const std::string field_name) : List(field_name, "StatusList") {}
        StatusList(const utility::JsonStore &jsn) : List(jsn) {}
        ~StatusList() override = default;
    };

    class Text : public Field<std::string> {
      public:
        Text(const std::string field_name) : Field<std::string>("Text", field_name) {}
        Text(const utility::JsonStore &jsn) : Field<std::string>(jsn) {}
        ~Text() override = default;

        Text &is_null() {
            Field<std::string>::is_null();
            return *this;
        }

        Text &is_not_null() {
            Field<std::string>::is_not_null();
            return *this;
        }

        Text &is(const std::string &value) {
            Field<std::string>::is(value);
            return *this;
        }

        Text &is_not(const std::string &value) {
            Field<std::string>::is_not(value);
            return *this;
        }

        Text &contains(const std::string &value) {
            Field<std::string>::contains(value);
            return *this;
        }

        Text &not_contains(const std::string &value) {
            Field<std::string>::not_contains(value);
            return *this;
        }

        Text &starts_with(const std::string &value) {
            Field<std::string>::starts_with(value);
            return *this;
        }

        Text &ends_with(const std::string &value) {
            Field<std::string>::ends_with(value);
            return *this;
        }

        Text &in(const std::vector<std::string> &value) {
            Field<std::string>::in(value);
            return *this;
        }

        Text &not_in(const std::vector<std::string> &value) {
            Field<std::string>::not_in(value);
            return *this;
        }
    };

    class DateTime : public Field<std::string> {
      public:
        DateTime(const std::string field_name, const std::string &class_name = "DateTime")
            : Field<std::string>(class_name, field_name) {}
        DateTime(const utility::JsonStore &jsn) : Field<std::string>(jsn) {}
        ~DateTime() override = default;

        DateTime &is_null() {
            Field<std::string>::is_null();
            return *this;
        }

        DateTime &is_not_null() {
            Field<std::string>::is_not_null();
            return *this;
        }

        DateTime &is(const std::string &value) {
            Field<std::string>::is(value);
            return *this;
        }

        DateTime &is_not(const std::string &value) {
            Field<std::string>::is_not(value);
            return *this;
        }
        DateTime &greater_than(const std::string &value) {
            Field<std::string>::greater_than(value);
            return *this;
        }

        DateTime &less_than(const std::string &value) {
            Field<std::string>::less_than(value);
            return *this;
        }

        DateTime &in(const std::vector<std::string> &value) {
            Field<std::string>::in(value);
            return *this;
        }
        DateTime &not_in(const std::vector<std::string> &value) {
            Field<std::string>::not_in(value);
            return *this;
        }

        DateTime &between(const std::string &value1, const std::string &value2) {
            Field<std::string>::between(value1, value2);
            return *this;
        }
        DateTime &not_between(const std::string &value1, const std::string &value2) {
            Field<std::string>::not_between(value1, value2);
            return *this;
        }

        DateTime &in_calendar_day(const int32_t value) {
            Field<std::string>::in_calendar_day(value);
            return *this;
        }
        DateTime &in_calendar_week(const int32_t value) {
            Field<std::string>::in_calendar_week(value);
            return *this;
        }
        DateTime &in_calendar_month(const int32_t value) {
            Field<std::string>::in_calendar_month(value);
            return *this;
        }
        DateTime &in_calendar_year(const int32_t value) {
            Field<std::string>::in_calendar_year(value);
            return *this;
        }

        DateTime &in_last(const int32_t value, const Period period) {
            Field<std::string>::in_last(value, period);
            return *this;
        }
        DateTime &not_in_last(const int32_t value, const Period period) {
            Field<std::string>::not_in_last(value, period);
            return *this;
        }
        DateTime &in_next(const int32_t value, const Period period) {
            Field<std::string>::in_next(value, period);
            return *this;
        }
        DateTime &not_in_next(const int32_t value, const Period period) {
            Field<std::string>::not_in_next(value, period);
            return *this;
        }
    };

    class Date : public DateTime {
      public:
        Date(const std::string field_name) : DateTime(field_name, "Date") {}
        Date(const utility::JsonStore &jsn) : DateTime(jsn) {}
        ~Date() override = default;
    };


    class Entity : public Field<std::string> {
      public:
        Entity(const std::string field_name, const std::string &class_name = "Entity")
            : Field<std::string>(class_name, field_name) {}
        Entity(const utility::JsonStore &jsn) : Field<std::string>(jsn) {}
        ~Entity() override = default;

        Entity &is(const std::string &value) {
            Field<std::string>::is(value);
            return *this;
        }

        Entity &is_not(const std::string &value) {
            Field<std::string>::is_not(value);
            return *this;
        }

        Entity &is_null() {
            Field<std::string>::is_null();
            return *this;
        }

        Entity &is_not_null() {
            Field<std::string>::is_not_null();
            return *this;
        }

        Entity &type_is(const std::string &value) {
            Field<std::string>::type_is(value);
            return *this;
        }

        Entity &type_is_not(const std::string &value) {
            Field<std::string>::type_is_not(value);
            return *this;
        }

        Entity &name_contains(const std::string &value) {
            Field<std::string>::name_contains(value);
            return *this;
        }

        Entity &name_not_contains(const std::string &value) {
            Field<std::string>::name_not_contains(value);
            return *this;
        }

        Entity &name_starts_with(const std::string &value) {
            Field<std::string>::name_starts_with(value);
            return *this;
        }

        Entity &name_ends_with(const std::string &value) {
            Field<std::string>::name_ends_with(value);
            return *this;
        }

        Entity &name_is(const std::string &value) {
            Field<std::string>::name_is(value);
            return *this;
        }


        Entity &in(const std::vector<std::string> &value) {
            Field<std::string>::in(value);
            return *this;
        }

        Entity &not_in(const std::vector<std::string> &value) {
            Field<std::string>::not_in(value);
            return *this;
        }
    };

    class MultiEntity : public Entity {
      public:
        MultiEntity(const std::string field_name) : Entity(field_name, "MultiEntity") {}
        MultiEntity(const utility::JsonStore &jsn) : Entity(jsn) {}
        ~MultiEntity() override = default;
    };

    class Addressing : public Entity {
      public:
        Addressing(const std::string field_name) : Entity(field_name, "Addressing") {}
        Addressing(const utility::JsonStore &jsn) : Entity(jsn) {}
        ~Addressing() override = default;
    };

    class RelationType : public Field<utility::JsonStore> {
      public:
        RelationType(
            const std::string field_name, const std::string &class_name = "RelationType")
            : Field<utility::JsonStore>(class_name, field_name) {}
        RelationType(const utility::JsonStore &jsn) : Field<utility::JsonStore>(jsn) {}
        ~RelationType() override = default;

        RelationType &is(const utility::JsonStore &value) {
            Field<utility::JsonStore>::is(value);
            return *this;
        }
        RelationType &is_not(const utility::JsonStore &value) {
            Field<utility::JsonStore>::is_not(value);
            return *this;
        }
        RelationType &name_is(const std::string &value) {
            nlohmann::json jvalue;
            jvalue = value;
            Field<utility::JsonStore>::name_is(utility::JsonStore(jvalue));
            return *this;
        }
        RelationType &name_contains(const std::string &value) {
            nlohmann::json jvalue;
            jvalue = value;
            Field<utility::JsonStore>::name_contains(utility::JsonStore(jvalue));
            return *this;
        }
        RelationType &name_not_contains(const std::string &value) {
            nlohmann::json jvalue;
            jvalue = value;
            Field<utility::JsonStore>::name_not_contains(utility::JsonStore(jvalue));
            return *this;
        }
        RelationType &in(const std::vector<utility::JsonStore> &value) {
            Field<utility::JsonStore>::in(value);
            return *this;
        }
        RelationType &not_in(const std::vector<utility::JsonStore> &value) {
            Field<utility::JsonStore>::not_in(value);
            return *this;
        }
        RelationType &is_null() {
            Field<utility::JsonStore>::is_null();
            return *this;
        }
        RelationType &is_not_null() {
            Field<utility::JsonStore>::is_not_null();
            return *this;
        }
    };

    class FilterBy;


    typedef std::variant<
        FilterBy,
        Checkbox,
        Number,
        Numeric<int32_t>,
        Numeric<float>,
        Float,
        Timecode,
        Duration,
        Percent,
        Currency,
        Text,
        Image,
        List,
        StatusList,
        DateTime,
        Date,
        RelationType,
        Entity>
        FilterType;

    class FilterBy : public Op, private std::list<FilterType> {
      public:
        typedef std::list<FilterType> StdList;

        using StdList::assign;
        using StdList::back;
        using StdList::begin;
        using StdList::cbegin;
        using StdList::cend;
        using StdList::clear;
        using StdList::const_iterator;
        using StdList::crbegin;
        using StdList::crend;
        using StdList::emplace_back;
        using StdList::empty;
        using StdList::end;
        using StdList::erase;
        using StdList::front;
        using StdList::insert;
        using StdList::iterator;
        using StdList::pop_back;
        using StdList::push_back;
        using StdList::size;

        FilterBy(const utility::JsonStore &jsn);
        FilterBy(const BoolOperator op = BoolOperator::AND)
            : Op("FilterBy"), op_(std::move(op)) {}
        ~FilterBy() override = default;

        [[nodiscard]] bool enabled() const { return true; }

        BoolOperator op() const { return op_; }
        void set_op(const BoolOperator op) { op_ = op; }

        void set_and_op() { op_ = BoolOperator::AND; }
        void set_or_op() { op_ = BoolOperator::OR; }

        template <typename T> FilterBy &And(const T &that) { // NOLINT
            return logical_op(BoolOperator::AND, that);
        }

        template <typename T> FilterBy &Or(const T &that) { // NOLINT
            return logical_op(BoolOperator::OR, that);
        }

        template <typename... T> FilterBy &And(T &&...that) { // NOLINT
            return logical_op(BoolOperator::AND, std::forward<T>(that)...);
        }

        template <typename... T> FilterBy &Or(T &&...that) { // NOLINT
            return logical_op(BoolOperator::OR, std::forward<T>(that)...);
        }

        operator nlohmann::json() const override {
            auto j                = R"({"conditions":[]})"_json;
            j["logical_operator"] = to_string(op_);
            for (const auto &i : *this)
                std::visit(
                    [jj = &j["conditions"]](auto &&arg) {
                        if (arg.enabled())
                            jj->push_back(static_cast<nlohmann::json>(arg));
                    },
                    i);
            return j;
        }

        utility::JsonStore serialise() const override {
            utility::JsonStore jsn;
            jsn["class"] = class_name();
            jsn["op"]    = to_string(op_);
            jsn["array"] = nlohmann::json::array();
            for (const auto &i : *this)
                std::visit(
                    [jj = &jsn["array"]](auto &&arg) { jj->push_back(arg.serialise()); },
                    i); // 2
            return jsn;
        }

      protected:
        template <typename T>
        FilterBy &logical_op(const BoolOperator logic_operator, const T &that) {
            push_back(that);
            op_ = std::move(logic_operator);
            return *this;
        }

        template <typename T> void add_term(T &&arg) { push_back(arg); }

        template <typename T, typename... TT> void add_term(T &&arg, TT &&...args) {
            push_back(arg);
            add_term(std::forward<TT>(args)...);
        }

        template <typename... T>
        FilterBy &logical_op(const BoolOperator logic_operator, T &&...args) {
            op_ = std::move(logic_operator);
            add_term(std::forward<T>(args)...);
            // insert(end(), that.begin(), that.end());
            return *this;
        }

        BoolOperator op_;
    };

    inline std::ostream &operator<<(std::ostream &out, const FilterBy &op) {
        return out << nlohmann::json(op).dump(2);
    }

    class EntityType : public Op, private std::map<std::string, FilterBy> {
      public:
        typedef std::map<std::string, FilterBy> EntityMap;

        EntityType(const utility::JsonStore &jsn);
        EntityType() : Op("EntityType") {}
        ~EntityType() override = default;

        operator nlohmann::json() const override {
            if (empty())
                return R"({})"_json;

            auto j = R"({})"_json;
            for (const auto &i : *this)
                j[i.first] = static_cast<nlohmann::json>(i.second);

            return j;
        }

        EntityType &With(const std::string &entity, const FilterBy &filter) { // NOLINT
            (*this)[entity] = filter;
            return *this;
        }

        utility::JsonStore serialise() const override {
            utility::JsonStore jsn;
            jsn["class"] = class_name();

            jsn["map"] = nlohmann::json::object();
            for (const auto &i : *this)
                jsn["map"][i.first].push_back(i.second.serialise());

            return jsn;
        }

        using EntityMap::at;
        using EntityMap::begin;
        using EntityMap::cbegin;
        using EntityMap::cend;
        using EntityMap::clear;
        using EntityMap::const_iterator;
        using EntityMap::crbegin;
        using EntityMap::crend;
        using EntityMap::empty;
        using EntityMap::end;
        using EntityMap::find;
        using EntityMap::iterator;
        using EntityMap::size;
        using EntityMap::operator[];
        using EntityMap::erase;
        using EntityMap::insert;
    };

    inline std::ostream &operator<<(std::ostream &out, const EntityType &op) {
        return out << nlohmann::json(op).dump(2);
    }

    inline std::string filter_type_type_to_string(const FilterType &ft) {
        return std::visit(
            [](auto &&arg) -> std::string {
                using TT           = std::decay_t<decltype(arg)>;
                std::string result = "UNKNOWN";

                if constexpr (std::is_same_v<FilterBy, TT>) {
                    result = "FilterBy";
                } else if constexpr (std::is_same_v<Checkbox, TT>) {
                    result = "Checkbox";
                } else if constexpr (std::is_same_v<Number, TT>) {
                    result = "Number";
                } else if constexpr (std::is_same_v<Float, TT>) {
                    result = "Float";
                } else if constexpr (std::is_same_v<Numeric<int32_t>, TT>) {
                    result = "Numeric<int32_t>";
                } else if constexpr (std::is_same_v<Numeric<float>, TT>) {
                    result = "Numeric<float>";
                } else if constexpr (std::is_same_v<Timecode, TT>) {
                    result = "Timecode";
                } else if constexpr (std::is_same_v<Duration, TT>) {
                    result = "Duration";
                } else if constexpr (std::is_same_v<Percent, TT>) {
                    result = "Percent";
                } else if constexpr (std::is_same_v<Currency, TT>) {
                    result = "Currency";
                } else if constexpr (std::is_same_v<Text, TT>) {
                    result = "Text";
                } else if constexpr (std::is_same_v<Image, TT>) {
                    result = "Image";
                } else if constexpr (std::is_same_v<List, TT>) {
                    result = "List";
                } else if constexpr (std::is_same_v<StatusList, TT>) {
                    result = "StatusList";
                } else if constexpr (std::is_same_v<DateTime, TT>) {
                    result = "DateTime";
                } else if constexpr (std::is_same_v<Date, TT>) {
                    result = "Date";
                }
                return result;
            },
            ft);
    }

    inline std::string filter_type_field(const FilterType &ft) {
        return std::visit(
            [](auto &&arg) -> std::string {
                using TT           = std::decay_t<decltype(arg)>;
                std::string result = "UNKNOWN";

                if constexpr (std::is_same_v<FilterBy, TT>)
                    result = "N/A";
                else
                    result = arg.field();
                return result;
            },
            ft);
    }

    inline ConditionalOperator filter_type_op(const FilterType &ft) {
        return std::visit(
            [](auto &&arg) -> ConditionalOperator {
                using TT                   = std::decay_t<decltype(arg)>;
                ConditionalOperator result = ConditionalOperator::NONE;

                if constexpr (std::is_same_v<FilterBy, TT>)
                    result = ConditionalOperator::NONE;
                else
                    result = arg.op();
                return result;
            },
            ft);
    }

    inline bool filter_type_null(const FilterType &ft) {
        return std::visit(
            [](auto &&arg) -> bool {
                using TT    = std::decay_t<decltype(arg)>;
                bool result = false;

                if constexpr (std::is_same_v<FilterBy, TT>)
                    result = false;
                else
                    result = arg.null();
                return result;
            },
            ft);
    }

    inline bool filter_type_enabled(const FilterType &ft) {
        return std::visit(
            [](auto &&arg) -> bool {
                using TT = std::decay_t<decltype(arg)>;
                return arg.enabled();
            },
            ft);
    }

    inline std::string filter_type_value_to_string(const FilterType &ft) {
        return std::visit(
            [](auto &&arg) -> std::string {
                using TT           = std::decay_t<decltype(arg)>;
                std::string result = "UNKNOWN";

                if constexpr (std::is_same_v<FilterBy, TT>)
                    result = "N/A";
                else
                    result = arg.string_value();
                return result;
            },
            ft);
    }

    inline std::string filter_type_value_to_json(const FilterType &ft) {
        return std::visit(
            [](auto &&arg) -> std::string {
                using TT           = std::decay_t<decltype(arg)>;
                std::string result = "UNKNOWN";

                if constexpr (std::is_same_v<FilterBy, TT>)
                    result = "N/A";
                else
                    result = arg.json_value();
                return result;
            },
            ft);
    }

    // inline std::string filter_type_value(const FilterType &ft) {
    //     return std::visit(
    //         [](auto &&arg) -> std::string {
    //             using TT = std::decay_t<decltype(arg)>;
    //             std::string result = "UNKNOWN";

    //             if constexpr (std::is_same_v<FilterBy, TT>)
    //                 result = "N/A";
    //              else
    //                 result = arg.string_value();
    //             return result;
    //         },
    //         ft);
    // }

    class Filter : public FilterBy {
      public:
        Filter(
            const std::string name    = "",
            const std::string entity  = "",
            const std::string project = "",
            const int project_id      = -1,
            const BoolOperator op     = BoolOperator::AND)
            : FilterBy(op),
              name_(std::move(name)),
              entity_(std::move(entity)),
              project_(std::move(project)),
              project_id_(project_id) {}
        Filter(const utility::JsonStore &jsn)
            : FilterBy(utility::JsonStore(jsn["filterby"])),
              name_(jsn["name"]),
              entity_(jsn["entity"]),
              project_(jsn["project"]),
              project_id_(jsn["project_id"]) {}

        ~Filter() override = default;

        void set_name(const std::string &value) { name_ = value; }
        void set_entity(const std::string &value) { entity_ = value; }
        void set_project(const std::string &value) { project_ = value; }
        void set_project_id(const int value) { project_id_ = value; }

        auto name() const { return name_; }
        auto entity() const { return entity_; }
        auto project() const { return project_; }
        auto project_id() const { return project_id_; }

        utility::JsonStore serialise() const override {
            utility::JsonStore jsn;
            jsn["name"]       = name_;
            jsn["entity"]     = entity_;
            jsn["project"]    = project_;
            jsn["project_id"] = project_id_;
            jsn["filterby"]   = FilterBy::serialise();
            return jsn;
        }

      private:
        std::string name_;
        std::string entity_;
        std::string project_;
        int project_id_;
    };

    class AuthenticateShotgun;

    class ShotgunClient {
      public:
        ShotgunClient()          = default;
        virtual ~ShotgunClient() = default;

        [[nodiscard]] std::string scheme_host_port() const { return scheme_host_port_; }
        [[nodiscard]] std::string token() const { return access_token_; }
        [[nodiscard]] std::string refresh_token() const { return refresh_token_; }

        [[nodiscard]] std::string content_type_json() const { return "application/json"; }

        [[nodiscard]] std::string content_type_hash() const {
            return "application/vnd+shotgun.api3_hash+json";
        }
        [[nodiscard]] std::string content_type_array() const {
            return "application/vnd+shotgun.api3_array+json";
        }
        [[nodiscard]] utility::clock::duration token_lifetime() const {
            return expires_ - utility::clock::now();
        }
        [[nodiscard]] bool token_expired() const { return expires_ < utility::clock::now(); }

        void set_scheme_host_port(const std::string &scheme_host_port) {
            scheme_host_port_ = scheme_host_port;
        }

        void set_credentials_method(const AuthenticateShotgun &auth);

        void expire_refresh_token() {
            expires_       = utility::clock::now();
            refresh_token_ = "";
        }

        bool failed_authentication() const { return failed_authentication_; }
        void set_failed_authentication(const bool value = true) {
            failed_authentication_ = value;
            refresh_token_         = "";
            expires_               = utility::clock::now();
        }

        void set_token(
            const std::string &token_type,
            const std::string &access_token,
            const std::string &refresh_token,
            const int expires_in);

        void set_refresh_token(const std::string &refresh_token, const int expires_in);

        [[nodiscard]] httplib::Headers get_headers() const;
        [[nodiscard]] httplib::Headers
        get_auth_headers(const std::string &content_type = "") const;
        [[nodiscard]] httplib::Params
        get_auth_request_params(const std::string &auth_token = "") const;
        [[nodiscard]] httplib::Params get_auth_refresh_params() const;

        [[nodiscard]] bool authenticated() const { return authenticated_; }
        void set_authenticated(const bool value) { authenticated_ = value; }

      private:
        bool authenticated_{false};
        bool failed_authentication_{false};
        std::string scheme_host_port_;

        // authentication
        AUTHENTICATION_METHOD authentication_method_ = {AM_UNDEFINED};
        std::string id_;
        std::string secret_;
        std::string session_uuid_;

        std::string token_type_;
        std::string access_token_;
        std::string refresh_token_;
        utility::time_point expires_;
    };

    class AuthenticateShotgun {
      public:
        AuthenticateShotgun() = default;
        AuthenticateShotgun(const utility::JsonStore &jsn) {
            if (jsn.count("authentication_method") &&
                not jsn["authentication_method"].is_null())
                authentication_method_ = jsn["authentication_method"];
            if (jsn.count("client_id") && not jsn["client_id"].is_null())
                client_id_ = jsn["client_id"];
            if (jsn.count("client_secret") && not jsn["client_secret"].is_null())
                client_secret_ = jsn["client_secret"];
            if (jsn.count("username") && not jsn["username"].is_null())
                username_ = jsn["username"];
            if (jsn.count("password") && not jsn["password"].is_null())
                password_ = jsn["password"];
            if (jsn.count("session_token") && not jsn["session_token"].is_null())
                session_token_ = jsn["session_token"];
            if (jsn.count("session_uuid") && not jsn["session_uuid"].is_null())
                session_uuid_ = jsn["session_uuid"];
            if (jsn.count("refresh_token") && not jsn["refresh_token"].is_null())
                refresh_token_ = jsn["refresh_token"];
        }

        virtual ~AuthenticateShotgun() = default;

        utility::JsonStore serialise() const {
            utility::JsonStore jsn;

            jsn["authentication_method"] = authentication_method_
                                               ? nlohmann::json(*authentication_method_)
                                               : nlohmann::json(nullptr);
            jsn["client_id"]             = client_id_ ? *client_id_ : nullptr;
            jsn["client_secret"]         = client_secret_ ? *client_secret_ : nullptr;
            jsn["username"]              = username_ ? *username_ : nullptr;
            jsn["password"]              = password_ ? *password_ : nullptr;
            jsn["session_token"]         = session_token_ ? *session_token_ : nullptr;
            jsn["session_uuid"]          = session_uuid_ ? *session_uuid_ : nullptr;
            jsn["refresh_token"]         = refresh_token_ ? *refresh_token_ : nullptr;

            return jsn;
        }

        void set_authentication_method(const std::string &value) {
            authentication_method_ = authentication_method_from_string(value);
        }

        void set_authentication_method(const AUTHENTICATION_METHOD value) {
            authentication_method_ = value;
        }
        void set_client_id(const std::string &value) { client_id_ = value; }
        void set_client_secret(const std::string &value) { client_secret_ = value; }
        void set_username(const std::string &value) { username_ = value; }
        void set_password(const std::string &value) { password_ = value; }
        void set_session_token(const std::string &value) { session_token_ = value; }
        void set_session_uuid(const std::string &value) { session_uuid_ = value; }
        void set_refresh_token(const std::string &value) { refresh_token_ = value; }

        auto authentication_method() const { return authentication_method_; }
        auto client_id() const { return client_id_; }
        auto client_secret() const { return client_secret_; }
        auto username() const { return username_; }
        auto password() const { return password_; }
        auto session_token() const { return session_token_; }
        auto session_uuid() const { return session_uuid_; }
        auto refresh_token() const { return refresh_token_; }

        template <class Inspector> friend bool inspect(Inspector &f, AuthenticateShotgun &x) {
            return f.object(x).fields(
                f.field("mth", x.authentication_method_),
                f.field("cli", x.client_id_),
                f.field("cls", x.client_secret_),
                f.field("unm", x.username_),
                f.field("pas", x.password_),
                f.field("suu", x.session_uuid_),
                f.field("stk", x.session_token_),
                f.field("rtk", x.refresh_token_));
        }

      private:
        std::optional<AUTHENTICATION_METHOD> authentication_method_;
        std::optional<std::string> client_id_;
        std::optional<std::string> client_secret_;
        std::optional<std::string> username_;
        std::optional<std::string> password_;
        std::optional<std::string> session_uuid_;
        std::optional<std::string> session_token_;
        std::optional<std::string> refresh_token_;
    };

} // namespace shotgun_client
} // namespace xstudio
