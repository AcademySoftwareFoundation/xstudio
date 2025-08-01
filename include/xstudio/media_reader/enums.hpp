// SPDX-License-Identifier: Apache-2.0
#pragma once
#undef NO_ERROR

namespace xstudio {
namespace media_reader {
    typedef enum { MRC_NO = 0, MRC_MAYBE, MRC_YES, MRC_FULLY, MRC_FORCE } MRCertainty;
    typedef enum { NO_ERROR = 0, HAS_ERROR } BufferErrorState;

} // namespace media_reader
} // namespace xstudio