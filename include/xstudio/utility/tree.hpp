// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>
#include <list>

namespace xstudio {
namespace utility {


    template <typename T> class Tree;

    template <typename T> using TreeList = std::list<class Tree<T>>;

    template <typename T> class Tree : private TreeList<T> {
      public:
        Tree() : data_() {}
        Tree(T data, Tree<T> *parent = nullptr) : data_(std::move(data)), parent_(parent) {}
        ~Tree() = default;

        [[nodiscard]] T data() const { return data_; }
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
        using TreeList<T>::splice;

        typename TreeList<T>::iterator
        insert(typename TreeList<T>::const_iterator position, const T &val) {
            return TreeList<T>::insert(position, Tree<T>(val, this));
        }

        [[nodiscard]] size_t total_size() const {
            size_t result = TreeList<T>::size();
            for (const auto &i : *this)
                result += i.size();
            return result;
        }

        [[nodiscard]] Tree *parent() const { return parent_; }

      protected:
        void set_parent(Tree &parent) { parent_ = parent; }

      private:
        Tree *parent_{nullptr};
        T data_;
    };

} // namespace utility
} // namespace xstudio