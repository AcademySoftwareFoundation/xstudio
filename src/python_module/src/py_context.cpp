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
        self_->send(dest, *msg);
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

void py_context::py_join(const py::args &xs) {
    if (xs.size() < 1) {
        set_py_exception("Too few arguments to call CAF.join");
        return;
    }
    auto i   = xs.begin();
    auto grp = (*i).cast<group>();

    if (grp) {
        self_->join(grp);
        // spdlog::warn("{}", self_->joined_groups().size());
    }
}

void py_context::py_leave(const py::args &xs) {
    if (xs.size() < 1) {
        set_py_exception("Too few arguments to call CAF.leave");
        return;
    }
    auto i   = xs.begin();
    auto grp = (*i).cast<group>();
    if (grp)
        self_->leave(grp);
}

py::tuple py_context::py_tuple_from_wrapped_message(const py::args &xs) {

    auto i = xs.begin();
    try {
        if (i == xs.end()) {
            throw std::runtime_error("Empty args passed to tuple_from_message");
        }
        auto msg = (*i).cast<caf::message>();
        py::tuple result(msg.size());

        for (size_t i = 0; i < msg.size(); ++i) {
            auto tid = msg.type_at(i);
            if (auto meta_obj = detail::global_meta_object(tid)) {
                auto kvp = portable_bindings().find(to_string(meta_obj->type_name));
                if (kvp == portable_bindings().end()) {
                    set_py_exception(
                        R"(Unable to add element of type B ")",
                        to_string(meta_obj->type_name),
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
        return result;
    } catch (std::exception &e) {
        set_py_exception(e.what());
        return py::tuple{};
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
        if (auto meta_obj = detail::global_meta_object(tid)) {
            auto kvp = portable_bindings().find(to_string(meta_obj->type_name));
            if (kvp == portable_bindings().end()) {
                set_py_exception(
                    R"(Unable to add element of type C ")",
                    to_string(meta_obj->type_name),
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

void py_context::py_run_xstudio_message_loop() {

    while (self_) {
        self_->await_data();
        mailbox_element_ptr ptr = self_->next_message();
        try {

            auto addr    = caf::actor_cast<caf::actor_addr>(ptr->sender);
            const auto p = message_handler_callbacks_.find(addr);
            const auto q = message_conversion_function_.find(addr);
            if (p != message_handler_callbacks_.end()) {

                caf::message r = ptr->content();
                py::tuple py_msg(1);
                PyTuple_SetItem(py_msg.ptr(), 0, py::cast(r).release().ptr());

                if (q != message_conversion_function_.end()) {

                    // we have a py funct that will convert the caf message to
                    // a tuple
                    auto message_as_tuple = q->second(py_msg);
                    for (auto &func : p->second) {
                        func(message_as_tuple);
                    }

                } else {

                    for (auto &func : p->second) {
                        func(py_msg);
                    }
                }
            }

        } catch (std::exception &e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        }
    }
}

void py_context::py_add_message_callback(const py::args &xs) {

    try {

        if (xs.size() >= 2 && xs.size() <= 4) {

            auto i            = xs.begin();
            auto remote_actor = (*i).cast<caf::actor>();
            i++;
            auto callback_func = (*i).cast<py::function>();
            auto addr          = caf::actor_cast<caf::actor_addr>(remote_actor);

            if (xs.size() >= 3) {
                // Let's say we want to receive event messages from a MediaActor.
                // We need to call 'join_broadcast' with the event_group actor
                // belonging to the MediaActor. However, in
                // push_caf_message_to_py_callbacks we need to resolve who sent
                // the message in order to call the correct python callback.
                // The 'sender' when a message comes via an event_group actor is
                // the owner (in this case the MediaActor). So here, the 3rd
                // argument is the owner of the event group and we use this to
                // set the actor address that we register the callback against.
                i++;
                auto parent_actor = (*i).cast<caf::actor>();
                addr              = caf::actor_cast<caf::actor_addr>(parent_actor);
            }

            if (xs.size() == 4) {
                i++;
                // as an optional 4th argument, we take a function that will convert
                // a caf message into a tuple. This conversion function is provided
                // by the Link class in the xstuduio python module which we can't
                // get to from the C++ side here, hence we take a python function
                // that lets us run the conversion. Awkward.
                i++;
                auto convert_function = (*i).cast<py::function>();
                if (convert_function) {
                    message_conversion_function_[addr] = convert_function;
                }
            }

            const auto p = message_handler_callbacks_.find(addr);
            if (p != message_handler_callbacks_.end()) {
                auto q = p->second.begin();
                while (q != p->second.end()) {
                    if ((*q).is(callback_func)) {
                        return;
                    }
                    q++;
                }
            }

            message_handler_callbacks_[addr].push_back(callback_func);
            self_->send(
                remote_actor,
                xstudio::broadcast::join_broadcast_atom_v,
                caf::actor_cast<caf::actor>(self_));

        } else {
            throw std::runtime_error(
                "Set message callback expecting tuple of size 2 or 3 "
                "(remote_event_group_actor, callack_func, (optional: parent_actor)).");
        }

    } catch (std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
    }
}

void py_context::py_remove_message_callback(const py::args &xs) {

    try {

        if (xs.size() == 2) {

            auto i            = xs.begin();
            auto remote_actor = (*i).cast<caf::actor>();
            i++;
            auto callback_func = (*i).cast<py::function>();
            auto addr          = caf::actor_cast<caf::actor_addr>(remote_actor);

            const auto p = message_handler_callbacks_.find(addr);
            if (p != message_handler_callbacks_.end()) {
                auto q = p->second.begin();
                while (q != p->second.end()) {
                    if ((*q).is(callback_func)) {
                        q = p->second.erase(q);
                    } else {
                        q++;
                    }
                }
            }
            self_->send(
                remote_actor,
                xstudio::broadcast::leave_broadcast_atom_v,
                caf::actor_cast<caf::actor>(self_));

        } else {
            throw std::runtime_error("Remove message callback expecting tuple of size 2 "
                                     "(remote_actor, callack_func).");
        }

    } catch (std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
    }
}


void py_context::disconnect() {
    host_   = "";
    port_   = 0;
    remote_ = actor();
}

bool py_context::connect_remote(std::string host, uint16_t port) {
    disconnect();
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
    remote_ = actor;
    return static_cast<bool>(actor);
}


} // namespace caf::python