// SPDX-License-Identifier: Apache-2.0

#include <caf/actor_registry.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/global_store/global_store_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::json_store;
using namespace caf;

namespace xstudio::global_store {


class GlobalStoreIOActor : public caf::event_based_actor {
  public:
    GlobalStoreIOActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {}
    const char *name() const override { return NAME.c_str(); }

    caf::message_handler message_handler() {
        return caf::message_handler{
            [=](save_atom,
                const JsonStore &data,
                const std::string &path) -> caf::result<bool> {
                try {
                    // check dir exists..
                    std::ofstream o(path + ".tmp", std::ofstream::out | std::ofstream::trunc);
                    try {
                        o.exceptions(std::ios_base::failbit | std::ifstream::badbit);

                        o << std::setw(4) << data.cref() << std::endl;
                        o.close();

                        fs::rename(path + ".tmp", path);

                        spdlog::debug("Saved {}", path);
                    } catch (const std::exception &err) {
                        if (o.is_open()) {
                            o.close();
                            fs::remove(path + ".tmp");
                        }
                        return caf::result<bool>(make_error(
                            xstudio_error::error,
                            fmt::format("Failed to save {} {}", path, err.what())));
                    }
                } catch (const std::exception &err) {
                    return caf::result<bool>(make_error(
                        xstudio_error::error,
                        fmt::format("Failed to save {} {}", path, err.what())));
                }
                return true;
            }};
    }

    caf::behavior make_behavior() override { return message_handler(); }

  private:
    inline static const std::string NAME = "GlobalStoreIOActor";
};


GlobalStoreActor::GlobalStoreActor(
    caf::actor_config &cfg, const JsonStore &jsn, const bool read_only, std::string reg_value)
    : caf::event_based_actor(cfg),
      reg_value_(std::move(reg_value)),
      base_(static_cast<JsonStore>(jsn["base"])),
      read_only_(read_only) {
    init();
}

GlobalStoreActor::GlobalStoreActor(
    caf::actor_config &cfg,
    const std::string &name,
    const JsonStore &jsn,
    const bool read_only,
    std::string reg_value)
    : caf::event_based_actor(cfg),
      reg_value_(std::move(reg_value)),
      base_(name),
      read_only_(read_only) {
    base_.preferences_ = jsn;
    init();
}

caf::message_handler GlobalStoreActor::message_handler() {
    return caf::message_handler{
        [=](autosave_atom) -> bool { return base_.autosave_; },

        [=](autosave_atom, const bool enable) {
            if (enable != base_.autosave_)
                mail(utility::event_atom_v, autosave_atom_v, enable).send(base_.event_group());
            base_.autosave_ = enable;
            if (base_.autosave_)
                anon_mail(do_autosave_atom_v)
                    .delay(std::chrono::seconds(base_.autosave_interval_))
                    .send(actor_cast<caf::actor>(this), weak_ref);
        },

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](read_only_atom) -> bool { return read_only_; },

        [=](do_autosave_atom) {
            if (base_.autosave_)
                anon_mail(do_autosave_atom_v)
                    .delay(std::chrono::seconds(base_.autosave_interval_))
                    .send(actor_cast<caf::actor>(this), weak_ref);

            anon_mail(save_atom_v).send(this);
        },

        [=](get_json_atom atom) { return mail(atom).delegate(jsonactor_); },

        [=](get_json_atom atom, const std::string &path) {
            return mail(atom, path).delegate(jsonactor_);
        },

        [=](json_store::update_atom atom,
            const JsonStore &change,
            const std::string &path,
            const JsonStore &full) {
            mail(utility::event_atom_v, atom, change, path).send(base_.event_group());
            return mail(atom, full).delegate(actor_cast<caf::actor>(this));
        },

        [=](json_store::update_atom, const JsonStore &json) {
            base_.preferences_.set(json);
            try {
                if (auto tmp = preference_value<int>(
                        base_.preferences_, "/core/global_store/autosave_interval");
                    tmp != base_.autosave_interval_) {
                    base_.autosave_interval_ = tmp;
                    spdlog::debug("autosave updated {}", base_.autosave_interval_);
                }

                if (auto tmp = preference_value<bool>(
                        base_.preferences_, "/core/global_store/autosave_enable");
                    tmp != base_.autosave_) {
                    base_.autosave_ = tmp;
                    spdlog::debug("autosave enable updated {}", base_.autosave_);
                }

            } catch (...) {
            }
        },

        [=](save_atom) -> caf::result<bool> {
            auto rp     = make_response_promise<bool>();
            auto result = std::make_shared<bool>(true);
            auto count  = std::make_shared<size_t>(PreferenceContexts.size());

            for (const auto &context : PreferenceContexts)
                mail(save_atom_v, context)
                    .request(actor_cast<caf::actor>(this), infinite)
                    .then(
                        [=](const bool success) mutable {
                            if (not success)
                                (*result) = false;
                            (*count)--;
                            if (not(*count))
                                rp.deliver(*result);
                        },
                        [=](const error &err) mutable {
                            (*result) = false;
                            (*count)--;
                            if (not(*count))
                                rp.deliver(*result);
                        });

            return rp;
        },

        [=](save_atom, const std::string &context) -> caf::result<bool> {
            // collect items for context
            // make sure we're uptodate with jsonstore..
            auto rp                = make_response_promise<bool>();
            const std::string path = preference_path_context(context);

            if (not path.empty()) {
                if (not check_preference_path()) {
                    rp.deliver(make_error(
                        xstudio_error::error, fmt::format("Failed to save {}", context)));
                } else {
                    caf::scoped_actor sys(system());
                    // update our copy
                    base_.preferences_.set(request_receive<JsonStore>(
                        *sys, jsonactor_, json_store::get_json_atom_v));

                    // get things to store..
                    JsonStore prefs = get_preference_values(
                        base_.preferences_, std::set<std::string>{context}, true, path);

                    if (not read_only_)
                        rp.delegate(ioactor_, save_atom_v, prefs, path);
                    else
                        rp.deliver(true);
                }
            } else {
                rp.deliver(make_error(
                    xstudio_error::error, fmt::format("Invalid context {}", context)));
            }

            return rp;
        },

        [=](utility::serialise_atom, const std::string &context) -> JsonStore {
            caf::scoped_actor sys(system());
            base_.preferences_.set(
                request_receive<JsonStore>(*sys, jsonactor_, json_store::get_json_atom_v));
            JsonStore result;

            try {
                if (context.empty()) {
                    result = base_.serialise();
                } else {
                    // extract context..
                    const std::string path = preference_path_context(context);
                    result                 = get_preference_values(
                        base_.preferences_, std::set<std::string>{context}, true, path);
                }
            } catch (...) {
            }
            return result;
        },

        [=](set_json_atom atom, const JsonStore &json) {
            return mail(atom, json).delegate(jsonactor_);
        },

        [=](set_json_atom atom, const JsonStore &json, const std::string &path) {
            return mail(atom, json, path).delegate(jsonactor_);
        },

        [=](set_json_atom atom,
            const JsonStore &json,
            const std::string &path,
            const bool broadcast) {
            return mail(atom, json, path, false, broadcast).delegate(jsonactor_);
        },

        [=](utility::get_group_atom atom) { return mail(atom).delegate(jsonactor_); }};
}


void GlobalStoreActor::init() {
    // only parial..
    spdlog::debug("Created GlobalStoreActor {}", base_.name());
    print_on_exit(this, "GlobalStoreActor");

    jsonactor_ =
        spawn<JsonStoreActor>(Uuid(), base_.preferences_, std::chrono::milliseconds(50));
    link_to(jsonactor_);

    // link to store, so we can get our own settings.
    try {
        caf::scoped_actor sys(system());
        auto result = request_receive<std::tuple<caf::actor, caf::actor, JsonStore>>(
            *sys, jsonactor_, utility::get_group_atom_v);

        request_receive<bool>(
            *sys, std::get<1>(result), broadcast::join_broadcast_atom_v, this);

        base_.preferences_.set(std::get<2>(result));
        base_.autosave_interval_ =
            preference_value<int>(base_.preferences_, "/core/global_store/autosave_interval");
        base_.autosave_ =
            preference_value<bool>(base_.preferences_, "/core/global_store/autosave_enable");

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    ioactor_ = spawn<GlobalStoreIOActor>();
    link_to(ioactor_);

    if (base_.autosave_)
        anon_mail(do_autosave_atom_v)
            .delay(std::chrono::seconds(base_.autosave_interval_))
            .send(actor_cast<caf::actor>(this), weak_ref);

    system().registry().put(reg_value_, this);
}

void GlobalStoreActor::on_exit() { system().registry().erase(reg_value_); }
} // namespace xstudio::global_store
