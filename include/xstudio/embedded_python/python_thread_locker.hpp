#pragma once

#include "xstudio/utility/blind_data.hpp"

namespace xstudio {
namespace embedded_python {

    class PythonThreadLocker : public utility::BlindDataObject {

      public:
        // Mutex shared between actors needing to execute python and the main
        // embedded python actor
        std::mutex mutex_;
    };

} // end namespace embedded_python
} // namespace xstudio
