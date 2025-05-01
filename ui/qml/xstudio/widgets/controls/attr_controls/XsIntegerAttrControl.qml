// SPDX-License-Identifier: Apache-2.0
import QtQuick


import QtQuick.Layouts


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
            if (sourceModel) setRoleFilter(attrTitle, "title")
        }
        onSourceModelChanged: {
            if (attrTitle) setRoleFilter(attrTitle, "title")
        }
    }

    Repeater {

        model: attr_data
        XsIntegerValueControl {
            text: root.text
            fromValue: integer_min
            toValue: integer_max
            width: 100
            height: 50
        }
    }
}