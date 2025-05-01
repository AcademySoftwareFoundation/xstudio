// SPDX-License-Identifier: Apache-2.0
#include <caf/actor.hpp>
#include <caf/actor_system.hpp>
#include "xstudio/utility/json_store.hpp"

namespace xstudio::caf_utility {
class caf_config;
}

namespace xstudio {

// description: Helper class to set-up and manage an instance of a caf actor
// system and the xstudio 'global' actor that manages the instances of the
// various core components of xSTUDIO.
//
// Note that any application embedding xSTUDIO (and the main xSTUDIO applicaiton
// itself) will need to call the statuc 'instance' method of this class to
// intialise the actor system and create the global actor. Other set-up like
// loading preferences, starting the logging etc. is handled by this class.
class CafActorSystem {

  public:
    static void exit();
    static CafActorSystem *instance();
    static caf::actor_system &system() { return instance()->__system(); }
    static caf::actor global_actor(
        const bool embedded_python,
        const std::string &name         = "XStudio",
        const utility::JsonStore &prefs = utility::JsonStore()) {
        return instance()->__global_actor(name, prefs, embedded_python);
    }
    ~CafActorSystem();

  private:
    caf::actor_system &__system() { return *the_system_; }
    caf::actor __global_actor(
        const std::string &name, const utility::JsonStore &prefs, const bool embedded_python);

    CafActorSystem();
    caf::actor_system *the_system_;
    caf_utility::caf_config *config_;
    caf::actor global_actor_;
};

} // namespace xstudio