// SPDX-License-Identifier: Apache-2.0

#include <caf/io/middleman.hpp>
#include <caf/actor_registry.hpp>

#ifdef __apple__
namespace caf {
namespace detail {

    template <> struct int_types_by_size<16> {
        using unsigned_type = __uint128_t;
        using signed_type   = __int128;
    };

} // namespace detail
} // namespace caf
#endif

#include "xstudio/atoms.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/global/xstudio_actor_system.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/timeline/clip_actor.hpp"
#include "xstudio/timeline/track_actor.hpp"
#include "xstudio/timeline/gap_actor.hpp"
#include "xstudio/timeline/stack_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/conform/conformer.hpp"
#include "xstudio/media_reader/audio_buffer.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

using namespace xstudio;

static std::shared_ptr<CafActorSystem> s_instance_ = nullptr;

struct ExitTimeoutKiller {

    void start() {
#ifdef _WIN32
        spdlog::debug("ExitTimeoutKiller start ignored");
    }
#else


        // lock the mutex ...
        clean_actor_system_exit.lock();

        // .. and start a thread to watch the mutex
        exit_timeout = std::thread([&]() {
            // wait for stop() to be called - 10s
            if (!clean_actor_system_exit.try_lock_for(std::chrono::seconds(20))) {
                // stop() wasn't called! Probably failed to exit actor_system,
                // see main() function. Kill process.
                spdlog::critical("xSTUDIO has not exited cleanly: killing process now");
                kill(0, SIGKILL);
            } else {
                clean_actor_system_exit.unlock();
            }
        });
    }
#endif

    void stop() {
#ifdef _WIN32
        spdlog::debug("ExitTimeoutKiller stop ignored");
    }
#else
        // unlock the  mutex so exit_timeout won't time-out
        clean_actor_system_exit.unlock();
        if (exit_timeout.joinable())
            exit_timeout.join();
    }

    std::timed_mutex clean_actor_system_exit;
    std::thread exit_timeout;
#endif
};


CafActorSystem *CafActorSystem::instance() {
    if (!s_instance_) {
        s_instance_.reset(new CafActorSystem());
    }
    return s_instance_.get();
}

void CafActorSystem::exit() {
    if (s_instance_) {
        s_instance_.reset();
    }
}

CafActorSystem::CafActorSystem() {

    ACTOR_INIT_GLOBAL_META()
    core::init_global_meta_objects();
    io::middleman::init_global_meta_objects();

    // The Buffer class uses a singleton instance of ImageBufferRecyclerCache
    // which is held as a static shared ptr. Buffers access this when they are
    // destroyed. This static shared ptr is part of the media_reader component
    // but buffers might be cleaned up (destroyed) after the media_reader component
    // is cleaned up on exit. So we make a copy here to ensure the ImageBufferRecyclerCache
    // instance outlives any Buffer objects.

    // auto buffer_cache_handle = media_reader::Buffer::s_buf_cache;

    // As far as I can tell caf only allows config to be modified
    // through cli args. We prefer the 'sharing' policy rather than
    // 'stealing'. The latter results in multiple threads spinning
    // aggressively pushing CPU load very high during playback.

    // const char *args[] = {argv[0],
    // "--caf.work-stealing.aggressive-poll-attempts=0","--caf.logger.console.verbosity=trace"};

    // # file {
    // #   # File name template for output log files.
    // #   path = "actor_log_[PID]_[TIMESTAMP]_[NODE].log"
    // #   # Format for rendering individual log file entries.
    // #   format = "%r %c %p %a %t %M %F:%L %m%n"
    // #   # Minimum severity of messages that are written to the log. One of:
    // #   # 'quiet', 'error', 'warning', 'info', 'debug', or 'trace'.
    // #   verbosity = "trace"
    // #   # A list of components to exclude in file output.
    // #   excluded-components = []
    // # }()

    // if debugging actor lifetimes
    // const char *args[] = {
    //     argv[0],
    //     "--caf.scheduler.max-threads=128",
    //     "--caf.scheduler.policy=sharing",
    //     "--caf.logger.console.verbosity=warn",
    //     "--caf.logger.file.verbosity=debug",
    //     "--caf.logger.file.format=%r|%c|%p|%a|%t|%M|%F:%L|%m%n"
    // };

    // caf_config config{6, const_cast<char **>(args)};
    const char *args[] = {
        "xstudio",
        "--caf.scheduler.max-threads=128",
        "--caf.scheduler.policy=sharing",
        "--caf.logger.console.verbosity=warn"};

    config_ = new caf_utility::caf_config{4, const_cast<char **>(args)};

    config_->add_actor_type<
        timeline::GapActor,
        const std::string &,
        const utility::FrameRateDuration &>("Gap");
    config_->add_actor_type<timeline::StackActor, const std::string &>("Stack");
    config_
        ->add_actor_type<timeline::ClipActor, const utility::UuidActor &, const std::string &>(
            "Clip");
    config_->add_actor_type<
        timeline::TrackActor,
        const std::string &,
        const utility::FrameRate &,
        const media::MediaType &>("Track");

    // create the actor system
    the_system_ = new caf::actor_system(*config_);

    // store a reference to the actor system, so we can access it
    // via static method anywhere else we need to (mainly, the python
    // module instanced in the embedded python interpreter)
    utility::ActorSystemSingleton::actor_system_ref(*the_system_);
}

caf::actor CafActorSystem::__global_actor(
    const std::string &name, const utility::JsonStore &prefs, const bool embedded_python) {
    // here we create a new global actor or return the existing one if it has
    // already been created
    if (!global_actor_) {
        caf::scoped_actor self{*(the_system_)};
        if (prefs.is_null()) {
            utility::JsonStore basic_prefs;
            if (!global_store::load_preferences(basic_prefs)) {
                return caf::actor();
            }
            global_actor_ = self->spawn<global::GlobalActor>(basic_prefs, embedded_python);
        } else {
            global_actor_ = self->spawn<global::GlobalActor>(prefs, embedded_python);
        }

        // at this stage we ensure that a 'studio' actor (that manages sessions)
        // exists
        utility::request_receive<caf::actor>(
            *self, global_actor_, global::create_studio_atom_v, "XStudio");
    }
    return global_actor_;
}

CafActorSystem::~CafActorSystem() {

    ExitTimeoutKiller exit_timeout_killer;
    exit_timeout_killer.start();

    if (global_actor_) {
        caf::scoped_actor self{*(the_system_)};
        self->send_exit(global_actor_, caf::exit_reason::user_shutdown);
        global_actor_ = caf::actor();
    }

    // Uncomment to help debug case where shutdown is not clean and
    // actors are not exiting
#ifdef NO_OP
// TO DO - Windows build not exiting cleanly. Need to fix.
    while (the_system_->registry().running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        for (auto &i : the_system_->registry().named_actors()) {
            spdlog::warn("{}", i.first);
        }
        spdlog::warn("running in registry {}", the_system_->registry().running());
        spdlog::warn("running actors {}", the_system_->base_metrics().running_actors->value());
        static int ct = 0;
        if (ct > 10) {
            std::exit(EXIT_SUCCESS);
        }
        ct++;
    }
#endif    

    // The caf actors should only clear up when they have completed doing any
    // work still pending. For example, saving a session or saving the current
    // state of the user prefs to the filesystem may still be happnening when
    // we get to this destructor.

    // BUT - if actor cleanup fails with a dangling actor pointer, we will
    // hang indefinitely on the next line. This is a bad situation and should
    // always be addressed by developers if it's happening. However, the
    // ExitTimeoutKiller will ensure that the zombie xstudio process won't hang
    // around if such a bug does occur.

    delete the_system_;
    delete config_;
    exit_timeout_killer.stop();
}