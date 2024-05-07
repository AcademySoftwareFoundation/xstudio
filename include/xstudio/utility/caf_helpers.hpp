// SPDX-License-Identifier: Apache-2.0
#pragma once

// required for ACTOR_TEST_SETUP
namespace xstudio {
namespace utility {

    template <class Fun> class scope_guard {
      public:
        scope_guard(Fun f) : fun_(std::move(f)), enabled_(true) {}

        scope_guard(scope_guard &&x) : fun_(std::move(x.fun_)), enabled_(x.enabled_) {
            x.enabled_ = false;
        }

        ~scope_guard() {
            if (enabled_)
                fun_();
        }

      private:
        Fun fun_;
        bool enabled_;
    };

    // Creates a guard that executes `f` as soon as it goes out of scope.
    template <class Fun> scope_guard<Fun> make_scope_guard(Fun f) { return {std::move(f)}; }


    struct absolute_receive_timeout {
      public:
        using ms         = std::chrono::milliseconds;
#ifdef _WIN32
        using clock_type = std::chrono::high_resolution_clock;;
#else
        using clock_type = std::chrono::system_clock;
        // using clock_type = std::chrono::high_resolution_clock;
#endif
        absolute_receive_timeout(int msec) { x_ = clock_type::now() + ms(msec); }

        absolute_receive_timeout()                                 = default;
        absolute_receive_timeout(const absolute_receive_timeout &) = default;
        absolute_receive_timeout &operator=(const absolute_receive_timeout &) = default;

        [[nodiscard]] const clock_type::time_point &value() const { return x_; }

        template <class Inspector>
        friend bool inspect(Inspector &f, absolute_receive_timeout &x) {
            return f.object(x).fields(f.field("x", x.x_));
        }

        // friend void serialize(
        //  serializer& sink, absolute_receive_timeout& x,
        //  const unsigned int
        // ) {
        //  auto tse = x.x_.time_since_epoch();
        //  auto ms_since_epoch = std::chrono::duration_cast<ms>(tse).count();
        //  sink << static_cast<uint64_t>(ms_since_epoch);
        // }

        // friend void serialize(
        //  deserializer& source, absolute_receive_timeout& x,
        //  const unsigned int) {
        //  uint64_t ms_since_epoch;
        //  source >> ms_since_epoch;
        //  clock_type::time_point y;
        //  y += ms(static_cast<ms::rep>(ms_since_epoch));
        //  x.x_ = y;
        // }

      private:
        clock_type::time_point x_;
    };
    inline std::string to_string(const absolute_receive_timeout &) {
        return "absolute_receive_timeout";
    }
} // namespace utility
} // namespace xstudio

#define ACTOR_TEST_SETUP(...)                                                                  \
    struct fixture {                                                                           \
        actor_system_config cfg;                                                               \
        actor_system system;                                                                   \
        scoped_actor self;                                                                     \
        fixture() : system(cfg), self(system) {}                                               \
    };                                                                                         \
                                                                                               \
    int main(int argc, char *argv[]) {                                                         \
        ::testing::InitGoogleTest(&argc, argv);                                                \
        ACTOR_INIT_GLOBAL_META()                                                               \
        core::init_global_meta_objects();                                                      \
                                                                                               \
        return RUN_ALL_TESTS();                                                                \
    }

#define ACTOR_TEST_MINIMAL(...)                                                                \
    struct fixture {                                                                           \
        caf::actor_system_config cfg;                                                          \
        caf::actor_system system;                                                              \
        caf::scoped_actor self;                                                                \
        fixture() : system(cfg), self(system) {}                                               \
    };                                                                                         \
                                                                                               \
    int main(int argc, char *argv[]) {                                                         \
        ::testing::InitGoogleTest(&argc, argv);                                                \
        caf::exec_main_init_meta_objects();                                                    \
        caf::core::init_global_meta_objects();                                                 \
                                                                                               \
        return RUN_ALL_TESTS();                                                                \
    }
