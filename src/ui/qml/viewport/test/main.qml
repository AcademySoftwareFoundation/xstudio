// SPDX-License-Identifier: Apache-2.0
import QtQuick

import QtQuick.Layouts

import xstudio.qml.json_store 1.0

Window {
    visible: true
    width: 640
    height: 480
    title: qsTr("JSON Store")
    id: test_window

    JsonStore {
    	id: json_store1
    	objectName: "json_store1"
    }

    JsonStore {
    	id: json_store2
    	objectName: "json_store2"
    }

    JsonStore {
    	id: json_store3
    	objectName: "json_store3"
    }


    RowLayout {
        id: rowLayout
        anchors.fill: parent

        TextArea {
            id: js_edit1
            Layout.fillWidth: true
            Layout.fillHeight: true
            font.pixelSize: 12
	        text: json_store1.json_string
    	    onTextChanged: json_store1.json_string = text
        }

        TextArea {
            id: js_edit2
            Layout.fillWidth: true
            Layout.fillHeight: true
            font.pixelSize: 12
	        text: json_store2.json_string
    	    onTextChanged: json_store2.json_string = text
        }

        TextArea {
            id: js_edit3
            Layout.fillWidth: true
            Layout.fillHeight: true
            font.pixelSize: 12
	        text: json_store3.json_string
    	    onTextChanged: json_store3.json_string = text
        }
    }
}

/*##^##
Designer {
    D{i:1;anchors_height:20;anchors_width:80;anchors_x:190;anchors_y:152}
}
##^##*/


