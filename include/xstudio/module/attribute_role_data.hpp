// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"
#include <map>
#include <memory>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>
#include <any>
#include <typeinfo>
#include <typeindex>
#include <Imath/ImathVec.h>

namespace xstudio {

namespace module {

    template <class T, class F>
    inline std::pair<const std::type_index, std::function<nlohmann::json(std::any const &)>>
    to_any_to_json(F const &f) {
        return {std::type_index(typeid(T)), [g = f](std::any const &a) -> nlohmann::json {
                    return g(std::any_cast<T const &>(a));
                }};
    }

    /*
    Map of functions to convert certain types wrapped by std::any to nlohmann::json
    */

    inline const std::
        unordered_map<std::type_index, std::function<nlohmann::json(std::any const &)>> &
        any_to_json() {

        static std::unordered_map<
            std::type_index,
            std::function<nlohmann::json(std::any const &)>>
            _any_to_json{
                to_any_to_json<nlohmann::json>(
                    [](nlohmann::json x) -> nlohmann::json { return x; }),
                to_any_to_json<int>([](int x) -> nlohmann::json { return nlohmann::json(x); }),
                to_any_to_json<float>(
                    [](float x) -> nlohmann::json { return nlohmann::json(x); }),
                to_any_to_json<double>(
                    [](double x) -> nlohmann::json { return nlohmann::json(x); }),
                to_any_to_json<bool>(
                    [](bool x) -> nlohmann::json { return nlohmann::json(x); }),
                to_any_to_json<utility::Uuid>(
                    [](utility::Uuid x) -> nlohmann::json { return nlohmann::json(x); }),
                to_any_to_json<std::string>(
                    [](std::string x) -> nlohmann::json { return nlohmann::json(x); }),
                to_any_to_json<std::array<float, 4>>(
                    [](std::array<float, 4> x) -> nlohmann::json { return nlohmann::json(x); }),
                to_any_to_json<Imath::V3f>([](Imath::V3f x) -> nlohmann::json {
                    return nlohmann::json{"vec3", 1, x.x, x.y, x.z};
                }),
                to_any_to_json<utility::ColourTriplet>(
                    [](utility::ColourTriplet x) -> nlohmann::json {
                        return nlohmann::json{"colour", 1, x.r, x.g, x.b};
                    }),
                to_any_to_json<std::vector<float>>(
                    [](std::vector<float> x) -> nlohmann::json { return nlohmann::json(x); }),
                to_any_to_json<std::vector<bool>>(
                    [](std::vector<bool> x) -> nlohmann::json { return nlohmann::json(x); }),
                to_any_to_json<std::vector<std::string>>(
                    [](std::vector<std::string> x) -> nlohmann::json {
                        return nlohmann::json(x);
                    }),
            };
        return _any_to_json;
    }
    /*
        Class using 'type erasure' pattern to handle multiple types of data via std::any
    */
    class AttributeData {

      public:
        AttributeData() = default;

        AttributeData(const AttributeData &) = default;

        template <typename T> AttributeData(const T &d) : data_(d) {}

        template <typename T> [[nodiscard]] const T get() const {
            try {
                return std::any_cast<T>(data_);
            } catch ([[maybe_unused]] std::exception &e) {
                spdlog::warn(
                    "{} Attempt to get AttributeData with type {} as type {}",
                    __PRETTY_FUNCTION__,
                    data_.type().name(),
                    typeid(T).name());
                throw;
            }
        }

        template <typename T> bool __set(const T &v) { // NOLINT
            if (data_.has_value() && typeid(v) != data_.type()) {
                spdlog::warn(
                    "{} Attempt to set AttributeData with type {} with data of type {} and "
                    "value {}",
                    __PRETTY_FUNCTION__,
                    data_.type().name(),
                    typeid(v).name(),
                    to_json().dump());
                return false;
            } else if ((data_.has_value() && get<T>() != v) || !data_.has_value()) {
                data_ = v;
                return true;
            }
            return false;
        }

        template <typename T> bool set(const T &v) { return __set(v); }

        bool set(const std::string &data) {
            if (typeid(utility::Uuid) == data_.type()) {
                return set(utility::Uuid(data));
            } else {
                return __set(data);
            }
        }

        bool set(const utility::JsonStore &data) {
            return set(static_cast<const nlohmann::json &>(data));
        }

        bool set(const nlohmann::json &data) {

            bool rt = false;
            if (data.is_string()) {

                if (typeid(utility::Uuid) == data_.type()) {
                    rt = set(utility::Uuid(data.get<std::string>()));
                } else {
                    rt = set(data.get<std::string>());
                }

            } else if (
                data.is_array() && data.size() == 5 && data.begin().value().is_string() &&
                data.begin().value().get<std::string>() == "vec3") {

                rt = set(data.get<Imath::V3f>());

            } else if (
                data.is_array() && data.size() == 5 && data.begin().value().is_string() &&
                data.begin().value().get<std::string>() == "colour") {

                rt = set(data.get<utility::ColourTriplet>());

            } else if (data.is_array() && data.size() && data.begin().value().is_string()) {

                std::vector<std::string> v;
                for (auto p = data.begin(); p != data.end(); p++) {
                    if (p.value().is_string()) {
                        v.push_back(p.value().get<std::string>());
                    }
                }
                rt = set(v);

            } else if (data.is_array() && data.size() && data.begin().value().is_boolean()) {

                std::vector<bool> v;
                for (auto p = data.begin(); p != data.end(); p++) {
                    if (p.value().is_boolean()) {
                        v.push_back(p.value().get<bool>());
                    }
                }
                rt = set(v);

            } else if (
                data.is_array() && data.size() && data.begin().value().is_number_float()) {

                std::vector<float> v;
                for (auto p = data.begin(); p != data.end(); p++) {
                    if (p.value().is_number_float()) {
                        v.push_back(p.value().get<float>());
                    }
                }
                rt = set(v);

            } else if (data.is_boolean()) {
                rt = set(data.get<bool>());
            } else if (data.is_number_integer()) {
                rt = set(data.get<int>());
            } else if (data.is_number_float()) {
                rt = set(data.get<float>());
            } else if (
                data_.type() == typeid(nlohmann::json) && get<nlohmann::json>() != data) {
                data_ = data;
                rt    = true;
            } else if (!data_.has_value()) {
                data_ = data;
                rt    = true;
            } else if (data.is_null()) {
                rt = true;
            }
            return rt;
        }

        // not pretty, must be a better way of letting an int set a float and vice versa
        // without violating type checking
        bool set(const float &v) {
            if (data_.has_value()) {
                if (data_.type() == typeid(float) && get<float>() != v) {
                    data_ = v;
                    return true;
                } else if (data_.type() == typeid(int) && get<int>() != (int)v) {
                    data_ = (int)v;
                    return true;
                } else if (!(data_.type() == typeid(int) || data_.type() == typeid(float))) {
                    spdlog::warn(
                        "{} Attempt to set AttributeData with type {} with data of type {} "
                        "and "
                        "value",
                        __PRETTY_FUNCTION__,
                        data_.type().name(),
                        typeid(v).name());
                }
                return false;
            }
            data_ = v;
            return true;
        }

        bool set(const int &v) {
            if (data_.has_value()) {
                if (data_.type() == typeid(float) && get<float>() != (float)v) {
                    data_ = (float)v;
                    return true;
                } else if (data_.type() == typeid(int) && get<int>() != v) {
                    data_ = v;
                    return true;
                } else if (!(data_.type() == typeid(int) || data_.type() == typeid(float))) {
                    spdlog::warn(
                        "{} Attempt to set AttributeData with type {} with data of type {} "
                        "and "
                        "value",
                        __PRETTY_FUNCTION__,
                        data_.type().name(),
                        typeid(v).name());
                }
                return false;
            }
            data_ = v;
            return true;
        }

        template <typename T> AttributeData &operator=(const T &v) {
            set(v);
            return *this;
        }

        [[nodiscard]] inline nlohmann::json to_json() const {
            if (data_.has_value()) {
                auto atj = any_to_json();
                if (const auto it = atj.find(std::type_index(data_.type())); it != atj.cend()) {
                    return it->second(data_);
                } else {
                    spdlog::warn(
                        "{} Unregistered type {}", __PRETTY_FUNCTION__, data_.type().name());
                }
            }

            return nlohmann::json();
        }

      private:
        std::any data_;
    };

} // namespace module

} // namespace xstudio