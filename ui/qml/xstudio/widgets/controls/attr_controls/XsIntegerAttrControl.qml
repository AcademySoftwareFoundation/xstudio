// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

import xStudio 1.0

import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0

RowLayout {

    id: root
    property string text

    // this property should be set to the name of the attrib that the attribute was
    // added
    property var attr_group_model

    // this property should be set to the title of the attribute that you
    // want to control
    property string attr_title

    /* Now we filter the attribute model data to just show us the attribute
    that we want */
    XsFilterModel {
        id: attr_data
        sourceModel: attr_group_model
        sortAscending: true
        property var attrTitle: attr_title
        onAttrTitleChanged: {
            setRoleFilter(attrTitle, "title")
        }
    }

    Repeater {

        model: attr_data
        XsIntegerValueControl {
            text: root.text
            fromValue: integer_min
            toValue: integer_max
        }
    }
}