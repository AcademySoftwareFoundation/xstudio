#pragma once

namespace colors {

  template <typename E>
  typename std::underlying_type<E>::type value_of(E e) {
    return static_cast<typename std::underlying_type<E>::type>(e);
  }

}
