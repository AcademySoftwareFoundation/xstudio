// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/type_id.hpp>
#include <nlohmann/json.hpp>
#include <utility>
#include <filesystem>

#include <fstream>

#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/types.hpp"

#include <Imath/ImathBox.h>
#include <Imath/ImathMatrix.h>
#include <Imath/ImathVec.h>

// allow nlohmann serialisation of Imath types
namespace nlohmann {

template <> struct adl_serializer<xstudio::utility::ColourTriplet> {
    static void to_json(json &j, const xstudio::utility::ColourTriplet &c) {
        j = json{"colour", 1, c.r, c.g, c.b};
    }

    static void from_json(const json &j, xstudio::utility::ColourTriplet &c) {
        auto vv = j.begin();
        vv++; // skip param type
        vv++; // skip count
        c.r = (vv++).value().get<float>();
        c.g = (vv++).value().get<float>();
        c.b = (vv++).value().get<float>();
    }
};

template <typename T> struct adl_serializer<Imath::Matrix44<T>> {
    static void to_json(json &j, const Imath::Matrix44<T> &p) {
        j = json{
            "mat4",
            1,
            p[0][0],
            p[1][0],
            p[2][0],
            p[3][0],
            p[0][1],
            p[1][1],
            p[2][1],
            p[3][1],
            p[0][2],
            p[1][2],
            p[2][2],
            p[3][2],
            p[0][3],
            p[1][3],
            p[2][3],
            p[3][3]};
    }

    static void from_json(const json &j, Imath::Matrix44<T> &p) {
        auto vv = j.begin();
        vv++; // skip param type
        vv++; // skip count
        for (int i = 0; i < 4; ++i)
            for (int k = 0; k < 4; ++k)
                p[k][i] = (vv++).value().get<T>();
    }
};

template <typename T> struct adl_serializer<Imath::Matrix33<T>> {
    static void to_json(json &j, const Imath::Matrix33<T> &p) {
        j = json{
            "mat3",
            1,
            p[0][0],
            p[1][0],
            p[2][0],
            p[0][1],
            p[1][1],
            p[2][1],
            p[0][2],
            p[1][2],
            p[2][2]};
    }

    static void from_json(const json &j, Imath::Matrix33<T> &p) {
        auto vv = j.begin();
        vv++; // skip param type
        vv++; // skip count
        for (int i = 0; i < 3; ++i)
            for (int k = 0; k < 3; ++k)
                p[k][i] = (vv++).value().get<T>();
    }
};

template <> struct adl_serializer<Imath::Vec2<int>> {
    static void to_json(json &j, const Imath::Vec2<int> &p) { j = json{"ivec2", 1, p.x, p.y}; }

    static void from_json(const json &j, Imath::Vec2<int> &p) {
        auto vv = j.begin();
        vv++; // skip param type
        vv++; // skip count
        p.x = (vv++).value().get<int>();
        p.y = (vv++).value().get<int>();
    }
};

template <typename T> struct adl_serializer<Imath::Vec2<T>> {
    static void to_json(json &j, const Imath::Vec2<T> &p) { j = json{"vec2", 1, p.x, p.y}; }

    static void from_json(const json &j, Imath::Vec2<T> &p) {
        auto vv = j.begin();
        vv++; // skip param type
        vv++; // skip count
        p.x = (vv++).value().get<T>();
        p.y = (vv++).value().get<T>();
    }
};

template <typename T> struct adl_serializer<Imath::Vec3<T>> {
    static void to_json(json &j, const Imath::Vec3<T> &p) {
        j = json{"vec3", 1, p.x, p.y, p.z};
    }

    static void from_json(const json &j, Imath::Vec3<T> &p) {
        auto vv = j.begin();
        vv++; // skip param type
        vv++; // skip count
        p.x = (vv++).value().get<T>();
        p.y = (vv++).value().get<T>();
        p.z = (vv++).value().get<T>();
    }
};

template <typename T> struct adl_serializer<Imath::Vec4<T>> {
    static void to_json(json &j, const Imath::Vec4<T> &p) {
        j = json{"vec4", 1, p.x, p.y, p.z, p.w};
    }

    static void from_json(const json &j, Imath::Vec4<T> &p) {
        auto vv = j.begin();
        vv++; // skip param type
        vv++; // skip count
        p.x = (vv++).value().get<T>();
        p.y = (vv++).value().get<T>();
        p.z = (vv++).value().get<T>();
        p.w = (vv++).value().get<T>();
    }
};

} // namespace nlohmann

// hack to stop GTest exploding
namespace nlohmann {
inline void PrintTo(json const &json, std::ostream *os) { *os << json.dump(); }
} // namespace nlohmann

namespace xstudio {
/** @brief Contains Json store classes */
namespace utility {
    /**
     *  @brief JSON container class.
     *
     *  @details
     *   Handles JSON operations, wraps nlohmann::json class.
     */
    using namespace caf;

    class JsonStore : public nlohmann::json {
      public:
        /**
         *  @brief Init from json.
         *  @param json  Init with json.
         */
        JsonStore(nlohmann::json json = nlohmann::json());
        /**
         *  @brief Init from other JsonStore.
         *  @param json  copy from other.
         */
        // JsonStore(const JsonStore &other);

        virtual ~JsonStore() = default;

        /**
         *  @brief Get json at JSON pointer path.
         *  @param path  JSON pointer path.
         *  @result JSON or exception.
         */
        [[nodiscard]] nlohmann::json get(const std::string &path) const;

        /**
         *  @brief Get type at JSON pointer path.
         *  @param path  JSON pointer path.
         *  @result type or exception.
         */
        template <typename result_type>
        [[nodiscard]] inline result_type get(const std::string &path = "") const {
            return get(path);
        }

        /**
         *  @brief Get type at JSON pointer path.
         *  @param path  JSON pointer path.
         *  @param default_val  Value returned if path invalid.
         *  @result type or exception.
         */
        template <typename result_type>
        [[nodiscard]] inline result_type
        get_or(const std::string &path, const result_type &default_val) const {
            return !is_null() ? value(path, default_val) : default_val;
        }

        /**
         *  @brief Set json at JSON pointer path.
         *  @param path  JSON pointer path.
         *  @param _json  JSON to insert.
         */
        void set(const nlohmann::json &json, const std::string &path = "");
        // void set(const JsonStore &json, const std::string &path = "");

        /**
         *  @brief Remove json at JSON pointer path.
         *  @param path  JSON pointer path.
         */
        bool remove(const std::string &path);

        /**
         *  @brief Merge json.
         *  @param json Json to merge.
         *  @param path JSON pointer path.
         */
        // void merge(const JsonStore &json, const std::string &path = "");
        void merge(const nlohmann::json &json, const std::string &path = "");

        nlohmann::json &ref() { return *this; }

        // nlohmann::json &operator[](const std::string &key) { return json_[key]; }

        // const nlohmann::json &operator[](const std::string &key) const { return
        // json_[key]; }

        [[nodiscard]] nlohmann::json cref() const { return *this; }

        [[nodiscard]] const nlohmann::json &ref() const { return *this; }

        // [[nodiscard]] bool empty() const { return json_.empty(); }
        // void clear() { json_.clear(); }
        [[nodiscard]] std::string dump(int pad = 0) const {
            return nlohmann::json::dump(
                pad, ' ', false, nlohmann::detail::error_handler_t::replace);
        }
        // [[nodiscard]] bool is_null() const { return json_.is_null(); }

        // bool operator==(const JsonStore &other) const { return json_ == other.json_; }

        // operator nlohmann::json() const { return json_; }

      private:
        // nlohmann::json json_;
    };


    template <class Inspector> bool inspect(Inspector &f, JsonStore &x) {
        auto get_jsn = [&x] { return x.dump(-1); };
        auto set_jsn = [&x](const std::string &val) {
            try {
                x.set(nlohmann::json::parse(val));
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return false;
            }

            return true;
        };
        return f.object(x).fields(f.field("jsn", get_jsn, set_jsn));
    }

    void to_json(nlohmann::json &j, const JsonStore &c);
    void from_json(const nlohmann::json &j, JsonStore &c);

    /**
     *  @brief Serialize JSON.
     *  @param json  JSON.
     *  @result Serialized JSON.
     */
    extern std::string to_string(const JsonStore &json);

    namespace fs = std::filesystem;

    JsonStore open_session(const caf::uri &path);
    JsonStore open_session(const std::string &path);

    nlohmann::json sort_by(const nlohmann::json &jsn, const nlohmann::json::json_pointer &ptr);

    inline JsonStore
    merge_json_from_path(const std::string &path, JsonStore merged = JsonStore()) {
        try {
            for (const auto &entry : fs::directory_iterator(path)) {
                if (not fs::is_regular_file(entry.status()) or
                    not(entry.path().extension() == ".json")) {
                    continue;
                }

                std::ifstream i(entry.path());
                utility::JsonStore j;
                i >> j;
                merged.merge(j);
            }
        } catch ([[maybe_unused]] const std::exception &e) {
            spdlog::warn("Preference path does not exist {}. ({})", path);
        }
        return merged;
    }
} // namespace utility
} // namespace xstudio