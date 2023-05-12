// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3

import xStudio 1.0
import xstudio.qml.helpers 1.0


XsDialogModal {
    id: dlg
    width: 700
    height: 300

    centerOnOpen: true

    XsModelProperty {
        id: feedback
        role: "valueRole"
        index: app_window.globalStoreModel.search_recursive("/ui/qml/feedback", "pathRole")
    }

    property bool isUrl: !feedback.value.includes("@")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            Image {
                id: xplayerIcon
                source: "qrc:/images/xstudio_logo_256_v1.svg"
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.topMargin: 15
                anchors.leftMargin: 10
                sourceSize.height: 128
                sourceSize.width: 128
                height: 128
                width: 128
            }
            Rectangle {
                color:"transparent"
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: xplayerIcon.right
                anchors.right: parent.right
                anchors.leftMargin: 10
                TextEdit {
                    id: titleText
                    readOnly: true
                    wrapMode: Text.WordWrap
                    selectByMouse: true
                    text: "Feedback for " + Qt.application.name + ' v' + Qt.application.version
                    font.pixelSize: 40
                    font.hintingPreference: Font.PreferNoHinting
                    font.family: XsStyle.fontFamily
                    anchors.top: parent.top
                    //                anchors.left: parent.left
                    color: XsStyle.hoverColor
                    padding: 10
                    font.bold: true
                }
                Rectangle {
                    height: 1
    //                width: 100
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    anchors.top: titleText.bottom
                    gradient: styleGradient.accent_gradient
                    color: XsStyle.highlightColor
                }

                TextEdit {
                    id: authorText
                    text:"Please "+(isUrl? "create" : "send") +" bug reports and suggestions " + (isUrl ? "at " : "to ") + feedback.value
                    readOnly: true
                    wrapMode: Text.WordWrap
                    selectByMouse: true
                    font.family: XsStyle.fontFamily
                    font.pixelSize: XsStyle.menuFontSize
                    font.hintingPreference: Font.PreferNoHinting
                    anchors.top: titleText.bottom
                    anchors.topMargin: 10
                    anchors.left: parent.left
                    anchors.right: parent.right
                    color: XsStyle.hoverColor
                    padding: 10
                }

                Rectangle {
                    id: btmDivLine
                    height: 1
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    anchors.top: parent.bottom
                    gradient: styleGradient.accent_gradient
                    color: XsStyle.highlightColor
                }
            }
        }


        DialogButtonBox {
            id: myFooter
            horizontalPadding: 20
            verticalPadding: 20
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.topMargin: 10
            Layout.minimumHeight: 35

            background: Rectangle {
                anchors.fill: parent
                color: "transparent"
            }

            Button {
                id: btnOK
                text: qsTr("OK")
                width: 100
                height: 36
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                background: Rectangle {
    //                color: XsStyle.highlightColor//mouseArea.containsMouse?:XsStyle.controlBackground
                    color: mouseArea.containsMouse?XsStyle.primaryColor:XsStyle.controlBackground
                    gradient:mouseArea.containsMouse?styleGradient.accent_gradient:Gradient.gradient
                    anchors.fill: parent
                }
                contentItem: Text {
                    text: btnOK.text
                    color: XsStyle.hoverColor//:XsStyle.mainColor
                    font.family: XsStyle.fontFamily
                    font.hintingPreference: Font.PreferNoHinting
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                MouseArea {
                    id: mouseArea
                    hoverEnabled: true
                    anchors.fill: btnOK
                    cursorShape: Qt.PointingHandCursor
                    onClicked: close()
                }
            }
        }
    }

}
