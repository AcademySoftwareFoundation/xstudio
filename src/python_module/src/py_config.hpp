// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <vector>
#include <string>

#include <caf/all.hpp>
#include <caf/config.hpp>
#include <caf/io/all.hpp>

#define PYBIND11_DETAILED_ERROR_MESSAGES

CAF_PUSH_WARNINGS
#include "pybind11_json/pybind11_json.hpp"
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>
CAF_POP_WARNINGS


namespace caf::python {

namespace py = pybind11;

class binding {
  public:
    binding(std::string py_name, bool builtin_type)
        : python_name_(std::move(py_name)), builtin_(builtin_type) {
        // nop
    }

    virtual ~binding() {
        // nop
    }

    inline void docstring(std::string x) { docstring_ = std::move(x); }

    inline const std::string &docstring() const { return docstring_; }

    inline const std::string &python_name() const { return python_name_; }

    inline bool builtin() const { return builtin_; }

    virtual void append(message_builder &xs, py::handle x) const = 0;

  private:
    std::string python_name_;
    std::string docstring_;
    bool builtin_;
};

class py_binding : public binding {
  public:
    py_binding(std::string name) : binding(name, true) {
        // nop
    }
};

template <class T> class default_py_binding : public py_binding {
  public:
    using py_binding::py_binding;

    void append(message_builder &xs, py::handle x) const override { xs.append(x.cast<T>()); }
};

class cpp_binding : public binding {
  public:
    using binding::binding;

    virtual py::object to_object(const message &xs, size_t pos) const = 0;
};

template <class T> class default_cpp_binding : public cpp_binding {
  public:
    using cpp_binding::cpp_binding;

    void append(message_builder &xs, py::handle x) const override { xs.append(x.cast<T>()); }

    py::object to_object(const message &xs, size_t pos) const override {
        return py::cast(xs.get_as<T>(pos));
    }
};

using binding_ptr     = std::unique_ptr<binding>;
using py_binding_ptr  = std::unique_ptr<py_binding>;
using cpp_binding_ptr = std::unique_ptr<cpp_binding>;

template <typename T> class has_register_struct {
  private:
    template <typename U>
    static auto test(U *x) -> decltype(register_class(
        x, std::declval<py::module &>(), std::declval<const std::string &>()));

    static auto test(...) -> std::false_type;

    using type = decltype(test(static_cast<T *>(nullptr)));

  public:
    static constexpr bool value = std::is_same<type, void>::value;
};

template <class T> class has_to_string {
  private:
    template <class U> static auto test(U *x) -> decltype(to_string(*x));

    static auto test(...) -> void;

    using type = decltype(test(static_cast<T *>(nullptr)));

  public:
    static constexpr bool value = std::is_same<type, std::string>::value;
};


template <class T> class has_register_class {
  private:
    template <class U>
    static auto test(U *x) -> decltype(register_class(
        x, std::declval<py::module &>(), std::declval<const std::string &>()));

    static auto test(...) -> std::false_type;

    using type = decltype(test(static_cast<T *>(nullptr)));

  public:
    static constexpr bool value = std::is_same<type, void>::value;
};

template <class T>
typename std::enable_if<has_register_class<T>::value>::type
default_python_class_init(py::module &m, const std::string &name) {
    register_class(static_cast<T *>(nullptr), m, name);
}

template <class T>
typename std::enable_if<
    !has_register_class<T>::value && !has_register_struct<T>::value &&
    has_to_string<T>::value>::type
default_python_class_init(py::module &m, const std::string &name) {
    auto str_impl = [](const T &x) { return to_string(x); };
    py::class_<T>(m, name.c_str()).def("__str__", str_impl);
}

template <class T>
typename std::enable_if<!has_register_class<T>::value && !has_to_string<T>::value>::type
default_python_class_init(py::module &m, const std::string &name) {
    auto str_impl = [](const T &x) { return to_string(x); };
    py::class_<T>(m, name.c_str());
}

void register_playlistitem_class(py::module &m, const std::string &name);
void register_playlistcontainer_class(py::module &m, const std::string &name);

class py_config : public caf::actor_system_config {
  public:
    std::string pre_run;

    using register_fun = std::function<void(py::module &, const std::string &)>;

    py_config(int argc = 0, char **argv = nullptr) {
        auto i = parse(argc, argv);

        add_types();

        register_funs_.push_back(
            [=](py::module &m) { register_playlistitem_class(m, "PlaylistItem"); });
        register_funs_.push_back(
            [=](py::module &m) { register_playlistcontainer_class(m, "PlaylistItemTree"); });

        add_atoms();
        add_messages();

        load<caf::io::middleman>();
    }

    template <class T>
    py_config &
    add_message_type(std::string name, register_fun reg = &default_python_class_init<T>) {
        return add_message_type<T>(name, name, reg);
    }


    template <class T>
    py_config &add_message_type(
        std::string pyname,
        std::string name,
        register_fun reg = &default_python_class_init<T>) {
        add_cpp<T>(pyname, name, std::move(reg));
        // actor_system_config::add_message_type<T>(std::move(name));
        return *this;
    }

    void py_init(py::module &x) const {
        for (auto &f : register_funs_)
            f(x);
    }

    std::string full_pre_run_script() const {
        return pre_run; // init_script + pre_run;
    }

    const std::unordered_map<std::string, binding *> &bindings() const { return bindings_; }

    const std::unordered_map<std::string, cpp_binding *> &portable_bindings() const {
        return portable_bindings_;
    }

    const std::unordered_map<std::string, cpp_binding_ptr> &cpp_bindings() const {
        return cpp_bindings_;
    }

  private:
    void add_atoms();
    void add_types();
    void add_messages();

    class int_py_binding : public py_binding {
      public:
      using py_binding::py_binding;
      void append(message_builder &xs, py::handle x) const override { 
        // Awkward! PyBind chucks an error if you try to cast a python int 
        // (which is actually a long or long long) to a C int if the python 
        // int value > INT_MAX.
        long foo = 12412;
        int64_t a = PyLong_AsLong(x.ptr());
        int b = int(a);
        if (a != int64_t(b)) {
          xs.append(a); 
        } else {
          xs.append(b); 
        }        
      }
    };

     void add_int_py() {
        auto ptr = new int_py_binding("int");
        py_bindings_.emplace("int", py_binding_ptr{ptr});
        bindings_.emplace(std::move("int"), ptr);
    }

    template <class T> void add_py(std::string name) {
        auto ptr = new default_py_binding<T>(name);
        py_bindings_.emplace(name, py_binding_ptr{ptr});
        bindings_.emplace(std::move(name), ptr);
    }
    template <class T>
    void add_cpp(
        std::string py_name,
        std::string cpp_name,
        const register_fun &reg = &default_python_class_init<T>) {
        if (reg)
            register_funs_.push_back([=](py::module &m) { reg(m, py_name); });
        auto ptr = new default_cpp_binding<T>(py_name, reg != nullptr);
        // all type names are prefix with "CAF."
        py_name.insert(0, "xstudio.core.__pybind_xstudio.");
        cpp_bindings_.emplace(py_name, cpp_binding_ptr{ptr});
        bindings_.emplace(std::move(py_name), ptr);
        portable_bindings_.emplace(std::move(cpp_name), ptr);
    }


    template <class T> void add_cpp(std::string name) { add_cpp<T>(name, name); }

  private:
    std::unordered_map<std::string, cpp_binding *> portable_bindings_;
    std::unordered_map<std::string, binding *> bindings_;
    std::unordered_map<std::string, cpp_binding_ptr> cpp_bindings_;
    std::unordered_map<std::string, py_binding_ptr> py_bindings_;

    std::vector<std::function<void(py::module &)>> register_funs_;
};
} // namespace caf::python
