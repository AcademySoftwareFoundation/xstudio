// SPDX-License-Identifier: Apache-2.0
#pragma GCC diagnostic ignored "-Wattributes"

#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/global/global_actor.hpp"

#include "py_opaque.hpp"
#include "py_config.hpp"
#include "py_context.hpp"

namespace caf::python {

/* This actor receives event messages from actors (xstudio components) that
Python plugins want to get messages from. It then sends the message back
to the py_context object which has registered the python callback functions
from the Python plugins, and executes the relevant callback function*/
class EventToPythonThreadLockerActor : public caf::event_based_actor {
  public:
    EventToPythonThreadLockerActor(caf::actor_config &cfg, py_context *context)
        : caf::event_based_actor(cfg), context_(context) {

        behavior_.assign(
            [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {
                // TODO: self clean up
            },
            [=](const xstudio::utility::Uuid &callback_id) {
                // stop watching

                // find actor that has its events forwarded to the callback with
                // the given ID
                auto p = actor_to_callback_uuid_.begin();
                while (p != actor_to_callback_uuid_.end()) {

                    auto q = p->second.begin();
                    while (q != p->second.end()) {
                        if (*q == callback_id) {
                            q = p->second.erase(q);
                        } else {
                            q++;
                        }
                    }

                    if (p->second.empty()) {
                        // don't need to get events from this actor any more
                        auto events_source = caf::actor_cast<caf::actor>(p->first);
                        if (events_source) {
                            mail(
                                xstudio::broadcast::leave_broadcast_atom_v,
                                caf::actor_cast<caf::actor>(this))
                                .send(events_source);
                        }
                        p = actor_to_callback_uuid_.erase(p);
                    } else {
                        p++;
                    }
                }
            },
            [=](caf::actor events_source, const xstudio::utility::Uuid &callback_id) {
                actor_to_callback_uuid_[caf::actor_cast<caf::actor_addr>(events_source)]
                    .push_back(callback_id);
                // join the events broadcast
                mail(
                    xstudio::broadcast::join_broadcast_atom_v,
                    caf::actor_cast<caf::actor>(this))
                    .send(events_source);
            },
            [=](bool) {},
            [=](message &_msg) {
                auto src = caf::actor_cast<caf::actor_addr>(current_sender());
                auto p   = actor_to_callback_uuid_.find(src);
                if (p == actor_to_callback_uuid_.end()) {
                    spdlog::debug(
                        "Python event group callback - getting messages from unknown source.");
                } else {
                    for (const auto &id : p->second) {
                        context_->execute_event_callback(_msg, id);
                    }
                }
            });
    }

    ~EventToPythonThreadLockerActor() = default;

    caf::behavior behavior_;
    py_context *context_;
    std::map<caf::actor_addr, std::vector<xstudio::utility::Uuid>> actor_to_callback_uuid_;

    caf::behavior make_behavior() override { return behavior_; }
};

inline void set_py_exception_fill(std::ostream &) {
    // end of recursion
}

template <class T, class... Ts>
void set_py_exception_fill(std::ostream &oss, T &&x, Ts &&...xs) {
    set_py_exception_fill(oss << std::forward<T>(x), std::forward<Ts>(xs)...);
}

template <class... Ts> void set_py_exception(Ts &&...xs) {
    std::ostringstream oss;
    set_py_exception_fill(oss, std::forward<Ts>(xs)...);
    throw std::runtime_error(oss.str().c_str());
}

py_context::py_context(int argc, char **argv)
    : py_config(argc, argv),
      py_local_system_(*this),
      system_(xstudio::utility::ActorSystemSingleton::actor_system_ref(py_local_system_)),
      self_(system_),
      remote_() {}

py_context::~py_context() {
    // shutdown system
    disconnect();
}

std::optional<message> py_context::py_build_message(const py::args &xs) {
    if (xs.size() < 2) {
        set_py_exception("Too few arguments to call build_message");
        return {};
    }
    auto i = xs.begin();
    ++i;
    message_builder mb;
    for (; i != xs.end(); ++i) {
        std::string type_name = PyEval_GetFuncName((*i).ptr());
        auto kvp              = bindings().find(type_name);
        if (kvp == bindings().end()) {
            set_py_exception(
                R"(Unable to add element of type A ")",
                type_name,
                R"(" to message: type is unknown to CAF)");
            return {};
        }
        kvp->second->append(mb, *i);
    }
    return mb.move_to_message();
}

void py_context::py_send(const py::args &xs) {
    if (xs.size() < 2) {
        set_py_exception("Too few arguments to call CAF.send");
        return;
    }
    auto i    = xs.begin();
    auto dest = (*i).cast<actor>();
    auto msg  = py_build_message(xs);
    if (msg)
        self_->mail(*msg).send(dest);
}

caf::actor py_context::py_spawn(const py::args &xs) {
    if (xs.size() < 1) {
        set_py_exception("Too few arguments to call py_spawn");
        return caf::actor();
    }
    auto i      = xs.begin();
    auto name   = (*i).cast<std::string>();
    auto msg    = py_build_message(xs);
    auto remote = system_.spawn<caf::actor>(name, *msg);
    if (remote)
        return *remote;
    set_py_exception("Failed to spawn actor.");
    return caf::actor();
}

caf::actor py_context::py_remote_spawn(const py::args &xs) {
    if (xs.size() < 1) {
        set_py_exception("Too few arguments to call py_remote_spawn");
        return caf::actor();
    }
    auto i      = xs.begin();
    auto name   = (*i).cast<std::string>();
    auto msg    = py_build_message(xs);
    auto remote = system_.middleman().remote_spawn<caf::actor>(
        remote_.node(), name, *msg, std::chrono::seconds(30));
    if (remote)
        return *remote;
    set_py_exception("Failed to spawn remote actor.");
    return caf::actor();
}

// void py_context::py_join(const py::args &xs) {
//     if (xs.size() < 1) {
//         set_py_exception("Too few arguments to call CAF.join");
//         return;
//     }
//     auto i   = xs.begin();
//     auto grp = (*i).cast<group>();

//     if (grp) {
//         self_->join(grp);
//         // spdlog::warn("{}", self_->joined_groups().size());
//     }
// }

// void py_context::py_leave(const py::args &xs) {
//     if (xs.size() < 1) {
//         set_py_exception("Too few arguments to call CAF.leave");
//         return;
//     }
//     auto i   = xs.begin();
//     auto grp = (*i).cast<group>();
//     if (grp)
//         self_->leave(grp);
// }

void py_context::erase_func(py::function &callback_func) {

    try {

        // acquire the GIL!
        py::gil_scoped_acquire gil;

        // erase
        callback_func = py::function();

    } catch (std::exception &e) {

        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void py_context::execute_event_callback(
    const caf::message &msg, const xstudio::utility::Uuid &callback_id) {

    auto p = message_callback_funcs_.find(callback_id);
    if (p == message_callback_funcs_.end())
        return;
    auto &callback_func = p->second;

    try {

        // acquire the GIL!
        py::gil_scoped_acquire gil;

        // build our CAF message data into a python tuple
        py::tuple result(msg.size());
        for (size_t i = 0; i < msg.size(); ++i) {
            auto tid = msg.type_at(i);
            if (auto meta_obj = detail::global_meta_object(tid);
                not meta_obj.type_name.empty()) {
                auto kvp = portable_bindings().find(std::string(meta_obj.type_name));
                if (kvp == portable_bindings().end()) {
                    set_py_exception(
                        R"(Unable to add element of type B ")",
                        std::string(meta_obj.type_name),
                        R"(" to message: type is unknown to CAF)");
                    return;
                }
                auto obj = kvp->second->to_object(msg, i);
                PyTuple_SetItem(result.ptr(), static_cast<int>(i), obj.release().ptr());
            } else {
                set_py_exception(
                    "Unable to extract element #",
                    i,
                    " from message: ",
                    "could not get portable name of ",
                    tid);
                return;
            }
        }

        // run the callback
        callback_func(result);

    } catch (std::exception &e) {

        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

uint64_t py_context::py_request(const py::args &xs) {
    if (xs.size() < 2) {
        set_py_exception("Too few arguments to call CAF.request");
        return 0;
    }
    auto i    = xs.begin();
    auto dest = (*i).cast<actor>();
    auto msg  = py_build_message(xs);
    if (msg) {
        auto reqhan = self_->request(dest, caf::infinite, *msg);
        return reqhan.id().request_id().integer_value();
    }
    return 0;
}

void py_context::py_send_exit(const py::args &xs) {
    if (xs.size() < 1) {
        set_py_exception("Too few arguments to call CAF.send_exit");
        return;
    }
    auto i    = xs.begin();
    auto dest = (*i).cast<actor>();
    ++i;
    self_->send_exit(dest, caf::exit_reason::user_shutdown);
}

py::tuple py_context::tuple_from_message(
    const message_id mid, const strong_actor_ptr sender, const message &msg) {
    py::tuple result(msg.size() + 2);
    // spdlog::warn("is_async {}, is_request {}, is_response {} msg size {}",
    // mid.is_async(), mid.is_request(), mid.is_response(), msg.size());
    // spdlog::warn("Request id {}", std::to_string(mid.response_id().integer_value()));
    // spdlog::warn("Response answered {} id
    // {}", mid.is_answered(), std::to_string(mid.request_id().integer_value()));

    for (size_t i = 0; i < msg.size(); ++i) {
        auto tid = msg.type_at(i);
        if (auto meta_obj = detail::global_meta_object(tid); not meta_obj.type_name.empty()) {
            auto kvp = portable_bindings().find(std::string(meta_obj.type_name));
            if (kvp == portable_bindings().end()) {
                set_py_exception(
                    R"(Unable to add element of type C ")",
                    std::string(meta_obj.type_name),
                    R"(" to message: type is unknown to CAF)");
                return py::tuple{};
            }
            auto obj = kvp->second->to_object(msg, i);
            PyTuple_SetItem(result.ptr(), static_cast<int>(i), obj.release().ptr());
        } else {
            set_py_exception(
                "Unable to extract element #",
                i,
                " from message: ",
                "could not get portable name of ",
                tid);
            return py::tuple{};
        }
    }
    if (mid.is_response())
        PyTuple_SetItem(
            result.ptr(), msg.size(), PyLong_FromSize_t(mid.request_id().integer_value()));
    else if (mid.is_async())
        PyTuple_SetItem(
            result.ptr(), msg.size(), PyLong_FromSize_t(mid.response_id().integer_value()));
    else
        PyTuple_SetItem(result.ptr(), msg.size(), PyLong_FromSize_t(0));

    PyTuple_SetItem(
        result.ptr(),
        msg.size() + 1,
        py::cast(caf::actor_cast<caf::actor>(sender)).release().ptr());

    return result;
}

py::tuple py_context::py_dequeue() {
    mailbox_element_ptr ptr = nullptr;

    if (self_->has_next_message())
        ptr = self_->next_message();

    while (!ptr) {
        self_->await_data();
        ptr = self_->next_message();
    }
    return tuple_from_message(ptr->mid, ptr->sender, std::move(ptr->content()));
}

py::tuple
py_context::py_dequeue_with_timeout(xstudio::utility::absolute_receive_timeout timeout) {
    mailbox_element_ptr ptr = nullptr;

    if (self_->has_next_message()) {
        // std::cout << "has message already" << std::endl;
        ptr = self_->next_message();
    }

    while (!ptr) {
        if (!self_->await_data(timeout.value())) {
            set_py_exception(R"(Dequeue timeout)");
            return py::tuple{};
        }
        // std::cout << "got message" << std::endl;
        ptr = self_->next_message();
    }
    return tuple_from_message(ptr->mid, ptr->sender, std::move(ptr->content()));
}

xstudio::utility::Uuid py_context::py_add_message_callback(const py::args &xs) {

    // this is called from Python plugins so that event messages from some
    // actor in xstudio's system can be watched via a python callback function
    // in the plugin.

    xstudio::utility::Uuid uuid = xstudio::utility::Uuid::generate();

    if (xs.size() == 2) {

        auto i = xs.begin();
        // this is the xstudio actor that
        auto remote_actor = (*i).cast<caf::actor>();
        i++;
        auto callback_func = (*i).cast<py::function>();
        auto addr          = caf::actor_cast<caf::actor_addr>(remote_actor);

        // spawn a listener actor to receive the event messages. THis will
        // run the Python callback (after acquiring the GIL)
        if (!message_callback_handler_actor_) {
            message_callback_handler_actor_ =
                self_->spawn<EventToPythonThreadLockerActor>(this);
        }

        // here we store the python function object - the callback in the Python
        // code
        message_callback_funcs_[uuid] = callback_func;

        // here we message our 'watcher'. It will join the event group of
        // the 'remote_actor' and when it gets event messages from that actor
        // it will run the py_callback
        anon_mail(remote_actor, uuid).send(message_callback_handler_actor_);

    } else {
        throw std::runtime_error("Set message callback expecting tuple of size 2 "
                                 "(remote_event_group_actor, callack_func).");
    }
    return uuid;
}

void py_context::py_remove_message_callback(const xstudio::utility::Uuid &id) {

    if (message_callback_handler_actor_) {
        anon_mail(id).send(message_callback_handler_actor_);
    }
    auto p = message_callback_funcs_.find(id);
    if (p != message_callback_funcs_.end()) {
        message_callback_funcs_.erase(p);
    } else {
        throw std::runtime_error(
            "py_remove_message_callback - callback handler ID not recognised.");
    }
}

void py_context::disconnect() {

    host_                  = "";
    port_                  = 0;
    remote_                = actor();
    embedded_python_actor_ = caf::actor();
    self_->send_exit(message_callback_handler_actor_, caf::exit_reason::user_shutdown);
    message_callback_handler_actor_ = caf::actor();
    message_callback_funcs_.clear();
}

bool py_context::connect_remote(std::string host, uint16_t port) {
    disconnect();

    // This will be null in xstudio_python case, but we expect that
    embedded_python_actor_ =
        system_.registry().template get<caf::actor>(xstudio::embedded_python_registry);

    auto actor = system_.middleman().remote_actor(host, port);
    if (actor) {
        remote_ = *actor;
        host_   = host;
        port_   = port;
    }
    return static_cast<bool>(actor);
}

bool py_context::connect_local(caf::actor actor) {

    disconnect();

    // This will be null in xstudio_python case, but we expect that
    embedded_python_actor_ =
        system_.registry().template get<caf::actor>(xstudio::embedded_python_registry);

    remote_ = actor;
    return static_cast<bool>(actor);
}

} // namespace caf::python