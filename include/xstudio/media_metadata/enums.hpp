// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio {
namespace media_metadata {
    typedef enum { MMC_NO = 0, MMC_MAYBE, MMC_YES, MMC_FULLY, MMC_FORCE } MMCertainty;

    inline MMCertainty from_string(const std::string &value) {
        auto result = MMC_NO;

        if (value == "MMC_NO")
            result = MMC_NO;
        else if (value == "MMC_MAYBE")
            result = MMC_MAYBE;
        else if (value == "MMC_YES")
            result = MMC_YES;
        else if (value == "MMC_FULLY")
            result = MMC_FULLY;
        else if (value == "MMC_FORCE")
            result = MMC_FORCE;

        return result;
    }

    inline std::string to_string(const MMCertainty value) {
        std::string result;
        switch (value) {
        default:
        case MMC_NO:
            result = "MMC_NO";
            break;
        case MMC_MAYBE:
            result = "MMC_MAYBE";
            break;
        case MMC_YES:
            result = "MMC_YES";
            break;
        case MMC_FULLY:
            result = "MMC_FULLY";
            break;
        case MMC_FORCE:
            result = "MMC_FORCE";
            break;
        }
        return result;
    }


} // namespace media_metadata
} // namespace xstudio