// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio::global {
typedef enum { ST_NONE = 0x0L, ST_BUSY = 1 << 0L } StatusType;

inline StatusType operator~(StatusType a) {
    return static_cast<StatusType>(~static_cast<int>(a));
}
inline StatusType operator|(StatusType a, StatusType b) {
    return static_cast<StatusType>(static_cast<int>(a) | static_cast<int>(b));
}
inline StatusType operator^(StatusType a, StatusType b) {
    return static_cast<StatusType>(static_cast<int>(a) ^ static_cast<int>(b));
}
inline StatusType operator&(StatusType a, StatusType b) {
    return static_cast<StatusType>(static_cast<int>(a) & static_cast<int>(b));
}

} // namespace xstudio::global
