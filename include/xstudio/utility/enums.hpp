// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio {
namespace utility {
    enum class TimeSourceMode { FIXED = 1, REMAPPED = 2, DYNAMIC = 3 };
    enum NotificationType {
        NT_INFO = 0,
        NT_WARN,
        NT_PROCESSING,
        NT_PROGRESS_RANGE,
        NT_PROGRESS_PERCENTAGE
    };
} // namespace utility
} // namespace xstudio