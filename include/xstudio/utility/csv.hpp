// SPDX-License-Identifier: Apache-2.0

#pragma once

// MS-DOS-style lines that end with (CR/LF) characters (optional for the last line).
// An optional header record (there is no sure way to detect whether it is present, so care is
// required when importing). Each record should contain the same number of comma-separated
// fields. Any field may be quoted (with double quotes). Fields containing a line-break,
// double-quote or commas should be quoted. (If they are not, the file will likely be impossible
// to process correctly.) If double-quotes are used to enclose fields, then a double-quote in a
// field must be represented by two double-quote characters.

#include <string>
#include <algorithm>
#include "xstudio/utility/string_helpers.hpp"

namespace xstudio {
namespace utility {

    inline std::string escape_csv(const std::string &src) {
        if (not(src.find('"') != std::string::npos or src.find(',') != std::string::npos or
                src.find('\n') != std::string::npos))
            return src;

        return "\"" + replace_all(src, "\"", "\"\"") + "\"";
    }

    std::string to_csv_row(const std::vector<std::string> &row, const int columns = 0) {
        std::string result;
        auto row_count = static_cast<int>(row.size());
        auto fields    = (columns ? columns : row_count);

        for (auto i = 0; i < fields; i++) {
            if (i < row_count)
                result += escape_csv(row[i]);

            if (i != fields - 1)
                result += ",";
        }

        return result;
    }
    std::string to_csv(const std::vector<std::vector<std::string>> &rows) {
        std::string result;

        if (not rows.empty()) {
            auto columns = rows[0].size();
            for (const auto &i : rows) {
                result += to_csv_row(i, columns) + "\r\n";
            }
        }

        return result;
    }
} // namespace utility
} // namespace xstudio