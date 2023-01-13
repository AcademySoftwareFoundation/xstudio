// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <caf/io/middleman.hpp>

namespace xstudio::caf_utility {
class caf_config;
}

namespace xstudio {

class CafSys {

  public:
    static CafSys *instance();
    ~CafSys();

    caf::actor_system &system() { return *the_system_; }
    caf::actor &global_actor() { return global_actor_; }


  private:
    CafSys();
    caf::actor_system *the_system_;
    caf_utility::caf_config *conf;
    caf::actor global_actor_;
};

} // namespace xstudio