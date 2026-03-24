// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string_view>


namespace xstudio::ui {

enum class EventType { Move, Drag, ButtonDown, ButtonRelease, MouseWheel, DoubleClick };

constexpr std::string_view EventType_to_str(EventType type) {
    switch (type) {
    case EventType::Move:
        return "Move";
    case EventType::Drag:
        return "Drag";
    case EventType::ButtonDown:
        return "ButtonDown";
    case EventType::ButtonRelease:
        return "ButtonRelease";
    case EventType::MouseWheel:
        return "MouseWheel";
    case EventType::DoubleClick:
        return "DoubleClick";
    default:
        return "Undefined";
    }
}

} // namespace xstudio::ui
