// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <future>
#include <mutex>
#include <regex>
#include <reproc++/drain.hpp>
#include <reproc++/reproc.hpp>

#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/plugin_manager/plugin_utility.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::plugin_manager;

class DisableScreensaver : public Utility {
  public:
    DisableScreensaver(const utility::JsonStore &jsn = utility::JsonStore())
        : Utility("DisableScreensaver", jsn) {
        reproc::stop_actions stop = {
            {reproc::stop::terminate, reproc::milliseconds(5000)},
            {reproc::stop::kill, reproc::milliseconds(2000)},
            {}};

        options_.stop            = stop;
        options_.redirect.parent = false;
    }
    ~DisableScreensaver() override { stop_screensaver(); }

    void preference_update(const utility::JsonStore &jsn) override {
        try {
            enabled_ = global_store::preference_value<bool>(
                jsn, "/plugin/utility/DisableScreensaver/enabled");
            interval_seconds_ = global_store::preference_value<int>(
                jsn, "/plugin/utility/DisableScreensaver/interval");
            enabled_when_playing_ = global_store::preference_value<int>(
                jsn, "/plugin/utility/DisableScreensaver/enabled_when_playing");
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }

    [[nodiscard]] bool enabled() const { return enabled_; }
    [[nodiscard]] bool running() const { return running_; }
    [[nodiscard]] bool enabled_when_playing() const { return enabled_when_playing_; }
    [[nodiscard]] int interval_seconds() const { return interval_seconds_; }

    int start_screensaver() {
        if (running_)
            return 0;

        process_           = reproc::process();
        std::error_code ec = process_.start(
            std::vector<std::string>{
                "watch",
                "-t",
                "-n",
                std::to_string(interval_seconds_),
                "-x",
                "xscreensaver-command",
                "-deactivate"},
            options_);
        if (ec == std::errc::no_such_file_or_directory) {
            spdlog::warn(
                "{} Program not found. Make sure it's available from the PATH.",
                __PRETTY_FUNCTION__);
            return ec.value();
        } else if (ec) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, ec.message());
            return ec.value();
        }

        drain_async_ = std::async(std::launch::async, [this]() {
            // `sink::thread_safe::string` locks a given mutex before appending to the
            // given string, allowing working with the string across multiple threads if
            // the mutex is locked in the other threads as well.
            reproc::sink::thread_safe::string sink(output_, mutex_);
            return reproc::drain(process_, sink, sink);
        });

        spdlog::debug("Starting screensaver block");
        running_ = true;

        return 0;
    }

    int stop_screensaver() {
        // spdlog::warn("{}",__PRETTY_FUNCTION__);
        int status = 0;
        if (running_) {
            std::error_code ec;
            // Check if any errors occurred in the background thread.
            // drain..
            report_screensaver();

            spdlog::debug("Stopping screensaver block");

            std::tie(status, ec) = process_.stop(options_.stop);
            drain_async_.get();
            running_ = false;

            if (ec) {
                // spdlog::warn("{} {}", __PRETTY_FUNCTION__, ec.message());
                return ec.value();
            }
        }

        return status;
    }

    std::string report_screensaver() {
        if (running_) {
            if (drain_async_.wait_for(std::chrono::milliseconds(100)) !=
                std::future_status::ready) {
                std::lock_guard<std::mutex> lock(mutex_);
                auto result = output_;
                output_.clear();
                return result;
            }
        }
        return "";
    }

  private:
    bool enabled_{false};
    bool enabled_when_playing_{false};
    int interval_seconds_{60};
    reproc::process process_;
    reproc::options options_;
    bool running_{false};
    std::string output_;
    std::mutex mutex_;
    std::future<std::error_code> drain_async_;
};

template <typename T> class DisableScreensaverPluginActor : public caf::event_based_actor {

  public:
    DisableScreensaverPluginActor(
        caf::actor_config &cfg, const utility::JsonStore &jsn = utility::JsonStore())
        : caf::event_based_actor(cfg), utility_(jsn) {

        spdlog::debug("Created DisableScreensaverPluginActor");

        // watch for updates to global preferences
        try {
            auto prefs = global_store::GlobalStoreHelper(system());
            utility::JsonStore j;
            join_broadcast(this, prefs.get_group(j));
            utility_.preference_update(j);
        } catch (...) {
        }

        // trigger loop..
        delayed_anon_send(this, std::chrono::seconds(5), utility::event_atom_v);
        join_event_group(this, system().registry().template get<caf::actor>(global_registry));

        behavior_.assign(
            [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](global::api_exit_atom) {},
            [=](global::exit_atom) {},
            [=](utility::event_atom, global::status_atom, const global::StatusType status) {
                playing_ = status & global::StatusType::ST_BUSY;

                if (playing_ and not utility_.running() and utility_.enabled() and
                    utility_.enabled_when_playing()) {
                    utility_.start_screensaver();
                } else if (
                    not playing_ and utility_.running() and utility_.enabled() and
                    utility_.enabled_when_playing()) {
                    utility_.stop_screensaver();
                }
            },
            [=](utility::name_atom) -> std::string { return utility_.name(); },

            [=](utility::event_atom) {
                if (utility_.running()) {
                    // capture output and dump it.
                    auto output = utility_.report_screensaver();

                    // check we've not been disabled.
                    if (not utility_.enabled()) {
                        utility_.stop_screensaver();
                    }
                } else {
                    if (utility_.enabled() and not utility_.enabled_when_playing()) {
                        utility_.start_screensaver();
                    }
                }

                delayed_anon_send(
                    this,
                    std::chrono::seconds(utility_.interval_seconds()),
                    utility::event_atom_v);
            },

            [=](json_store::update_atom,
                const utility::JsonStore & /*change*/,
                const std::string & /*path*/,
                const utility::JsonStore &full) {
                delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
            },

            [=](json_store::update_atom, const utility::JsonStore &js) {
                utility_.preference_update(js);
            });
    }

    caf::behavior make_behavior() override { return behavior_; }

  private:
    caf::behavior behavior_;
    T utility_;
    bool playing_{false};
};

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<UtilityPlugin<DisableScreensaverPluginActor<DisableScreensaver>>>(
                Uuid("2cd6db34-c696-421a-8564-90fa65d2a59a"),
                "DisableScreensaver",
                "xStudio",
                "Disable screensaver when playing",
                semver::version("1.0.0"))}));
}
}
