// SPDX-License-Identifier: Apache-2.0
#pragma once
#undef NO_ERROR

namespace xstudio {
namespace media_reader {
    typedef enum { MRC_NO = 0, MRC_MAYBE, MRC_YES, MRC_FULLY, MRC_FORCE } MRCertainty;
    typedef enum { NO_ERROR = 0, HAS_ERROR } BufferErrorState;

    inline MRCertainty from_string(const std::string &value) {
        auto result = MRC_NO;

        if (value == "MRC_NO")
            result = MRC_NO;
        else if (value == "MRC_MAYBE")
            result = MRC_MAYBE;
        else if (value == "MRC_YES")
            result = MRC_YES;
        else if (value == "MRC_FULLY")
            result = MRC_FULLY;
        else if (value == "MRC_FORCE")
            result = MRC_FORCE;

        return result;
    }

    inline std::string to_string(const MRCertainty value) {
        std::string result;
        switch (value) {
        default:
        case MRC_NO:
            result = "MRC_NO";
            break;
        case MRC_MAYBE:
            result = "MRC_MAYBE";
            break;
        case MRC_YES:
            result = "MRC_YES";
            break;
        case MRC_FULLY:
            result = "MRC_FULLY";
            break;
        case MRC_FORCE:
            result = "MRC_FORCE";
            break;
        }
        return result;
    }


} // namespace media_reader
} // namespace xstudio