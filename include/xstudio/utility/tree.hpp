// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>
#include <list>
#include <nlohmann/json.hpp>

#include "xstudio/utility/logging.hpp"

namespace xstudio {
namespace utility {

    template <typename T> class Tree;

    template <typename T> using TreeList = std::list<class Tree<T>>;

    template <typename T> class Tree : private TreeList<T> {
      public:
        Tree() : data_() {}
        Tree(T data) : data_(std::move(data)) {}
        ~Tree() = default;

        Tree(const Tree &t) : data_(t.data_), TreeList<T>(t) { reparent(); }


        Tree &operator=(const Tree<T> &obj) {
            if (this != &obj) {
                this->data_  = obj.data_;
                this->base() = obj.base();
                this->reparent();
            }
            return *this;
        }

        [[nodiscard]] const T &data() const { return data_; }

        [[nodiscard]] T &data() { return data_; }

        void set_data(T data) { data_ = std::move(data); }

        using typename TreeList<T>::iterator;
        using typename TreeList<T>::const_iterator;
        using TreeList<T>::clear;
        using TreeList<T>::begin;
        using TreeList<T>::end;
        using TreeList<T>::cbegin;
        using TreeList<T>::cend;
        using TreeList<T>::crbegin;
        using TreeList<T>::crend;
        using TreeList<T>::empty;
        using TreeList<T>::size;
        using TreeList<T>::erase;
        using TreeList<T>::front;
        using TreeList<T>::back;

        TreeList<T> &base() { return *this; }
        [[nodiscard]] const TreeList<T> &base() const { return *this; }

        typename TreeList<T>::iterator
        insert(typename TreeList<T>::const_iterator position, const T &val) {
            auto it = TreeList<T>::insert(position, Tree<T>(val));
            it->set_parent(this);
            return it;
        }

        typename TreeList<T>::iterator
        insert(typename TreeList<T>::const_iterator position, const Tree<T> &val) {
            auto it = TreeList<T>::insert(position, val);
            it->set_parent(this);
            it->reparent();
            return it;
        }

        void splice(typename TreeList<T>::const_iterator pos, TreeList<T> &other) {
            TreeList<T>::splice(pos, other);
            reparent();
        }

        void splice(
            typename TreeList<T>::const_iterator pos,
            TreeList<T> &other,
            typename TreeList<T>::const_iterator first,
            typename TreeList<T>::const_iterator last) {
            TreeList<T>::splice(pos, other, first, last);
            reparent();
        }

        typename TreeList<T>::iterator child(const size_t index) {
            if (index >= size())
                return end();
            return std::next(begin(), index);
        }

        typename TreeList<T>::const_iterator child(const size_t index) const {
            if (index >= size())
                return cend();
            return std::next(cbegin(), index);
        }

        [[nodiscard]] size_t index() const {
            auto result = 0u;

            if (parent_ != nullptr) {
                for (const auto &c : *parent_) {
                    if (&c == this)
                        break;
                    result++;
                }
            }

            return result;
        };

        [[nodiscard]] size_t total_size() const {
            size_t result = TreeList<T>::size();
            for (const auto &i : *this)
                result += i.size();
            return result;
        }

        [[nodiscard]] Tree<T> *parent() const { return parent_; }

        [[nodiscard]] bool contains(const Tree<T> *node) const {
            auto result = false;
            if (this == node) {
                result = true;
            } else {
                for (const auto &i : *this) {
                    result = i.contains(node);
                    if (result)
                        break;
                }
            }
            return result;
        }

        template <class C> void do_sort(C compare) { TreeList<T>::sort(compare); }

      protected:
        void reparent() {
            for (auto &i : *this) {
                if (i.parent() != this) {
                    i.set_parent(this);
                    i.reparent();
                }
            }
        }


        void set_parent(Tree<T> *parent) { parent_ = parent; }

      private:
        Tree<T> *parent_{nullptr};
        T data_;
    };

    using JsonTree = Tree<nlohmann::json>;


    inline void dump_tree(const JsonTree &node, const int depth = 0) {
        spdlog::warn("{:>{}} {}", " ", depth * 4, fmt::ptr(&node));
        for (const auto &i : node.base())
            dump_tree(i, depth + 1);
    }


    inline nlohmann::json::json_pointer
    tree_to_pointer(const JsonTree &node, const std::string &childname) {
        std::vector<size_t> indexes;

        auto ptr = &node;

        while (ptr and ptr->parent()) {
            indexes.emplace_back(ptr->index());
            ptr = ptr->parent();
        }

        auto result = nlohmann::json::json_pointer("");

        for (auto it = indexes.crbegin(); it != indexes.crend(); ++it) {
            result /= childname;
            result /= *it;
        }

        return result;
    }

    inline JsonTree *pointer_to_tree(
        JsonTree &node, const std::string &childname, nlohmann::json::json_pointer ptr) {
        JsonTree *result = &node;

        std::vector<int> indexes;

        while (not ptr.empty()) {
            try {
                indexes.push_back(std::stoi(ptr.back()));
                ptr.pop_back();
                if (ptr.back() != childname)
                    throw std::runtime_error("bad child");
                ptr.pop_back();
            } catch (...) {
                result = nullptr;
                break;
            }
        }

        auto *n = &node;
        for (auto it = indexes.rbegin(); it != indexes.rend(); ++it) {
            auto c = n->child(*it);
            if (c == n->end()) {
                result = nullptr;
                break;
            } else {
                n      = &(*c);
                result = &(*c);
            }
        }

        return result;
    }

    inline nlohmann::json tree_to_json(
        const JsonTree &node, const std::string &childname = "children", const int depth = -1) {
        // unroll..
        auto jsn = node.data();

        if (depth and not node.empty()) {
            jsn[childname] = R"([])"_json;
            for (const auto &i : node)
                jsn[childname].push_back(tree_to_json(i, childname, depth - 1));
        }

        return jsn;
    }

    inline JsonTree json_to_tree(const nlohmann::json &jsn, const std::string &childname) {

        JsonTree node(R"({})"_json);

        if (jsn.is_array()) {
            node.data()[childname] = R"([])"_json;
            for (const auto &i : jsn) {
                node.insert(node.end(), json_to_tree(i, childname));
            }

        } else if (jsn.is_object()) {
            for (const auto &[k, v] : jsn.items()) {
                if (k != childname)
                    node.data()[k] = v;
                else {
                    if (v.is_array()) {
                        node.data()[childname] = R"([])"_json;
                        for (const auto &i : v)
                            node.insert(node.end(), json_to_tree(i, childname));
                    } else {
                        node.data()[k] = v;
                    }
                }
            }
        }

        return node;
    }


} // namespace utility
} // namespace xstudio