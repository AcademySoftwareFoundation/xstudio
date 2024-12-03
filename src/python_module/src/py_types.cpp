// SPDX-License-Identifier: Apache-2.0
#ifdef __GNUC__ // Check if GCC compiler is being used
#pragma GCC diagnostic ignored "-Wattributes"
#endif
#include "py_opaque.hpp"

#include "py_config.hpp"
#include "xstudio/atoms.hpp"

namespace caf::python {

void register_addr_class(py::module &m, const std::string &name);
void register_group_class(py::module &m, const std::string &name);
void register_actor_class(py::module &m, const std::string &name);

void py_config::add_types() {
    // allow CAF to convert native Python types to C++ types
    // add_py<int>("int");
    add_int_py();
    add_py<bool>("bool");
    add_py<float>("float");
    add_py<double>("double");
    add_py<std::string>("str");

    // create Python bindings for builtin CAF types
    add_cpp<actor>("actor", "caf::actor", register_actor_class);
    add_cpp<group>("group", "caf::actor", register_group_class);
    add_cpp<actor_addr>("addr", "caf::actor_addr", register_addr_class);
    add_cpp<message>("message", "caf::message");
    add_cpp<error>("error", "caf::error");
    // add_cpp<uri>("uri", "uri");

    // fill list for native type bindings
    add_cpp<bool>("bool", "bool", nullptr);
    add_cpp<float>("float", "float", nullptr);
    add_cpp<double>("double", "double", nullptr);
    add_cpp<int32_t>("int32_t", "int32_t", nullptr);
    add_cpp<uint64_t>("uint64_t", "uint64_t", nullptr);
    add_cpp<int64_t>("int64_t", "int64_t", nullptr);
    add_cpp<int>("int", "int", nullptr);
    add_cpp<std::string>("str", "std::string", nullptr);
    add_cpp<std::vector<std::string>>("strvec", "std::vector<std::string>", nullptr);
}
} // namespace caf::python
