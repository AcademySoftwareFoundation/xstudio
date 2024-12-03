// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <filesystem>

#include <string>
#include <utility>
#include <vector>

#include <caf/actor_system.hpp>
#include <caf/scoped_actor.hpp>
#include <caf/type_id.hpp>

#include "xstudio/json_store/json_store_helper.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/json_store.hpp"

namespace fs = std::filesystem;


namespace xstudio {
namespace global_store {
    // define our hardwired properties..
    // which are loaded into the jsonstore..
    class GlobalStoreDef {
      public:
        GlobalStoreDef() : path_(), value_(), default_value_(), datatype_(), description_() {}
        GlobalStoreDef(
            std::string path,
            const nlohmann::json &value,
            std::string datatype,
            std::string description)
            : path_(std::move(path)),
              value_(value),
              default_value_(value),
              datatype_(std::move(datatype)),
              description_(std::move(description)) {}
        virtual ~GlobalStoreDef() = default;

        std::string path_;
        std::string display_name_;
        std::string category_;
        nlohmann::json value_;
        nlohmann::json default_value_;
        std::string datatype_;
        std::string description_;
        std::string overridden_path_;
        nlohmann::json minimum_value_;
        nlohmann::json maximum_value_;
        nlohmann::json options_;

        [[nodiscard]] std::string path() const { return path_; }
        [[nodiscard]] std::string display_name() const { return display_name_; }
        [[nodiscard]] std::string category() const { return category_; }
        [[nodiscard]] std::string datatype() const { return datatype_; }
        [[nodiscard]] std::string description() const { return description_; }
        [[nodiscard]] std::string overridden_path() const { return overridden_path_; }
        [[nodiscard]] nlohmann::json value() const {
            return (value_.is_null() ? default_value_ : value_);
        }
        [[nodiscard]] nlohmann::json default_value() const { return default_value_; }
        [[nodiscard]] nlohmann::json minimum_value() const { return minimum_value_; }
        [[nodiscard]] nlohmann::json maximum_value() const { return maximum_value_; }
        [[nodiscard]] nlohmann::json options() const { return options_; }

        operator std::string() const { return path_; }
        operator nlohmann::json() const {
            return {
                {"path", path_},
                {"value", value_},
                {"default_value", default_value_},
                {"description", description_},
                {"datatype", datatype_},
                {"category", category_},
                {"minimum", minimum_value_},
                {"maximum", maximum_value_},
                {"display_name", display_name_},
                {"options", options_},
                {"overridden_path", overridden_path_}};
        }
    };

    void to_json(nlohmann::json &j, const GlobalStoreDef &gsd);
    void from_json(const nlohmann::json &j, GlobalStoreDef &gsd);

    static const std::vector<std::string> PreferenceContexts{
        "NEW_SESSION", "APPLICATION", "QML_UI", "PLUGIN"};
    static const GlobalStoreDef gsd_hello{"/hello", "goodbye", "string", "Says goodbye"};
    // static const GlobalStoreDef gsd_beast{"/beast", 666, "Number of the beast"};
    // static const GlobalStoreDef gsd_happy{"/happy", true, "Am I happy"};
    // static const GlobalStoreDef gsd_nested_happy{"/nested/happy", true, "Am I happy"};
    // static const GlobalStoreDef gsd_nested_sad{"/nested/sad", false, "Am I sad"};

    static const std::vector<GlobalStoreDef> gsds{
        gsd_hello,
    };

    template <typename result_type>
    inline result_type preference_property(
        const utility::JsonStore &js, const std::string &path, const std::string &prop) {
        return js.get(path + "/" + prop);
    }

    template <typename result_type>
    inline result_type preference_value(const utility::JsonStore &js, const std::string &path) {
        if (js.get(path + "/value").is_null())
            return preference_property<result_type>(js, path, "default_value");
        return preference_property<result_type>(js, path, "value");
    }

    template <typename result_type>
    inline result_type
    preference_minimum(const utility::JsonStore &js, const std::string &path) {
        return preference_property<result_type>(js, path, "minimum");
    }

    template <typename result_type>
    inline result_type
    preference_maximum(const utility::JsonStore &js, const std::string &path) {
        return preference_property<result_type>(js, path, "maximum");
    }

    template <typename result_type>
    inline result_type
    preference_default_value(const utility::JsonStore &js, const std::string &path) {
        return preference_property<result_type>(js, path, "default_value");
    }

    template <typename result_type>
    inline result_type
    preference_options(const utility::JsonStore &js, const std::string &path) {
        return preference_property<result_type>(js, path, "options");
    }

    inline std::string
    preference_category(const utility::JsonStore &js, const std::string &path) {
        return preference_property<std::string>(js, path, "category");
    }

    inline std::string
    preference_description(const utility::JsonStore &js, const std::string &path) {
        return preference_property<std::string>(js, path, "description");
    }

    inline std::string
    preference_display_name(const utility::JsonStore &js, const std::string &path) {
        return preference_property<std::string>(js, path, "display_name");
    }

    inline std::string
    preference_overridden_path(const utility::JsonStore &js, const std::string &path) {
        return preference_property<std::string>(js, path, "overridden_path");
    }

    template <typename result_type>
    inline result_type
    preference_overridden_value(const utility::JsonStore &js, const std::string &path) {
        return preference_property<result_type>(js, path, "overridden_value");
    }

    inline std::string
    preference_datatype(const utility::JsonStore &js, const std::string &path) {
        return preference_property<std::string>(js, path, "datatype");
    }

    template <typename value_type>
    inline void set_preference_property(
        utility::JsonStore &js,
        const value_type &value,
        const std::string &path,
        const std::string &prop) {
        js.set(value, path + "/" + prop);
    }

    template <typename value_type>
    inline void set_preference_value(
        utility::JsonStore &js, const value_type &value, const std::string &path) {
        set_preference_property(js, value, path, "value");
    }

    inline void set_preference_overridden_path(
        utility::JsonStore &js, const std::string &value, const std::string &path) {
        set_preference_property(js, value, path, "overridden_path");
    }

    template <typename value_type>
    inline void set_preference_overridden_value(
        utility::JsonStore &js, const value_type &value, const std::string &path) {
        set_preference_property(js, value, path, "overridden_value");
    }

    std::set<std::string> get_preferences(
        const utility::JsonStore &js,
        const std::set<std::string> &context = std::set<std::string>());
    utility::JsonStore get_preference_values(
        const utility::JsonStore &js,
        const std::set<std::string> &context = std::set<std::string>(),
        const bool only_changed              = false,
        const std::string &override_path     = "");
    void set_global_store_def(utility::JsonStore &js, const GlobalStoreDef &gsd);

    bool preference_load_defaults(utility::JsonStore &js, const std::string &path);
    void
    preference_load_overrides(utility::JsonStore &js, const std::vector<std::string> &paths);

    class GlobalStore : public utility::Container {
      public:
        GlobalStore(const utility::JsonStore &jsn);
        GlobalStore(const std::string &name = "");

        ~GlobalStore() override = default;
        [[nodiscard]] utility::JsonStore serialise() const override;

      public:
        utility::JsonStore preferences_;
        bool autosave_{false};
        int autosave_interval_{600};
        std::string user_path_;
    };

    utility::JsonStore global_store_builder(
        const std::vector<std::string> &paths,
        const utility::JsonStore &json = utility::JsonStore());

    bool check_preference_path();

    // attaches to global datastore actor.
    class GlobalStoreHelper : public json_store::JsonStoreHelper {
      public:
        GlobalStoreHelper(
            caf::actor_system &, const std::string &reg_name = global_store_registry);
        ~GlobalStoreHelper() override = default;

        template <typename result_type>
        [[nodiscard]] inline result_type value(const std::string &path) const {
            return JsonStoreHelper::get(path + "/value");
        }

        template <typename result_type>
        [[nodiscard]] inline result_type default_value(const std::string &path) const {
            return JsonStoreHelper::get(path + "/default_value");
        }

        [[nodiscard]] inline std::string description(const std::string &path) const {
            return JsonStoreHelper::get(path + "/description");
        }

        [[nodiscard]] inline std::string overridden(const std::string &path) const {
            return JsonStoreHelper::get(path + "/overridden");
        }

        template <typename value_type>
        inline void set_value(
            const value_type &value,
            const std::string &path,
            const bool async           = true,
            const bool broacast_change = true) {
            JsonStoreHelper::set(value, path + "/value", async, broacast_change);
        }

        /*If a preference is found at path return the value. Otherwise build
        a preference at path and return default.*/
        utility::JsonStore get_existing_or_create_new_preference(
            const std::string &path,
            const utility::JsonStore &default_,
            const bool async           = true,
            const bool broacast_change = true,
            const std::string &context = "APPLICATION");

        void set(const GlobalStoreDef &gsd, const bool async = true);
        bool save(const std::string &context);
    };
} // namespace global_store
} // namespace xstudio
