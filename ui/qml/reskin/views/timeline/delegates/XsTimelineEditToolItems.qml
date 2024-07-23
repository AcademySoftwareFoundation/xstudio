// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.15
import QtQuick.Layouts 1.15

import xStudioReskin 1.0


XsPrimaryButton{ id: btnDiv

    isActiveIndicatorAtLeft: true

    imageDiv.rotation: _name=="Move UD" || _name=="Roll"? 90 : 0

}
