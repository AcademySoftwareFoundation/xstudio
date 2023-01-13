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
            return std::any_cast<T>(data_);
        }

        template <typename T> bool set(const T &v) {
            if (data_.has_value() && typeid(v) != data_.type()) {
                spdlog::warn(
                    "{} Attempt to set AttributeData with type {} with data of type {} and "
                    "value",
                    __PRETTY_FUNCTION__,
                    data_.type().name(),
                    typeid(v).name());
                return false;
            } else if ((data_.has_value() && get<T>() != v) || !data_.has_value()) {
                data_ = v;
                return true;
            }
            return false;
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

        bool set(const nlohmann::json &data);

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