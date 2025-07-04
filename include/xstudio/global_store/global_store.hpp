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

    // note: extra_prefs_paths allows us to add preference files (or folders) that
    // are loaded BEFORE the user's own preference files are loaded. The
    // override_prefs_paths will be loaded AFTER the user's own prefs, providing
    // a way to override user settings if required (e.g. on special machines like
    // in a playback suite we may want to force certain xstudio settings to suit
    // the machine and ignore the user's own settings)
    bool load_preferences(
        utility::JsonStore &js,
        const bool load_user_prefs                           = true,
        const std::vector<std::string> &extra_prefs_paths    = std::vector<std::string>(),
        const std::vector<std::string> &override_prefs_paths = std::vector<std::string>());

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

    static const std::vector<std::string> PreferenceContexts{
        "NEW_SESSION", "APPLICATION", "QML_UI", "PLUGIN"};

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

        template <typename value_type>
        inline void set_overridden_path(
            const value_type &value,
            const std::string &path,
            const bool async           = true,
            const bool broacast_change = true) {
            JsonStoreHelper::set(value, path + "/overridden_path", async, broacast_change);
        }

        /*If a preference is found at path return the value. Otherwise build
        a preference at path and return default.*/
        utility::JsonStore get_existing_or_create_new_preference(
            const std::string &path,
            const utility::JsonStore &default_,
            const bool async           = true,
            const bool broacast_change = true,
            const std::string &context = "APPLICATION");

        bool save(const std::string &context);
        [[nodiscard]] bool read_only() const;
    };
} // namespace global_store
} // namespace xstudio
