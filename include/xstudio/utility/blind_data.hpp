// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>
#include <functional>
#include <Imath/ImathMatrix.h>

namespace xstudio {
namespace utility {

    /* This empty class allows us to pass arbitrary data through the caf messaging
    system, to allow plugins to define their own data structure for storing
    data. For example, to render data on the viewport, a plugin will need to
    compute and store some data which xstudio requests. The format of the data is
    a shared pointer to the plugin's own custom class (derived from BlindDataObject).
    Then at draw time the plugin's static render function receives the data and
    the plugin is responsible for casting it back to the plugin's custom class
    and using the data to draw to the screen. */

    class BlindDataObject {
      public:
        BlindDataObject()          = default;
        virtual ~BlindDataObject() = default;

        // override this to output debug messages
        virtual void debug() const {}
    };

    typedef std::shared_ptr<const BlindDataObject> BlindDataObjectPtr;

} // namespace utility
} // namespace xstudio