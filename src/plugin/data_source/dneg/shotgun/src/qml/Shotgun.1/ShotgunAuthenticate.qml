// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import xStudio 1.1

import xstudio.qml.module 1.0

XsDialogModal {
    id: dlg
    property string message: ""
    title: "ShotGrid Authentication" + (message ? " - "+message:"")

    width: 300
    height: 200

    XsModuleAttributes {
        id: attrs_options
        attributesGroupNames: "shotgun_datasource_preference"
        roleName: "combo_box_options"
    }

    XsModuleAttributes {
        id: attrs_values
        attributesGroupNames: "shotgun_datasource_preference"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            id: main_layout
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            XsLabel {
                id: label
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
                text: "Authentication Method : "
            }

            XsComboBox {
                Layout.fillWidth: true
                Layout.rightMargin: 6
                Layout.preferredHeight: label.height*2
                property var auth_method: attrs_values.authentication_method ? attrs_values.authentication_method : null
                property bool inited: false

                font.pixelSize: XsStyle.sessionBarFontSize
                font.family: XsStyle.controlTitleFontFamily
                font.hintingPreference: Font.PreferNoHinting

                model: attrs_options.authentication_method ? attrs_options.authentication_method : null

                onModelChanged: {
                    if(model && attrs_values.authentication_method != undefined) {
                        currentIndex = find(attrs_values.authentication_method)
                        // currentText = attrs_values.authentication_method
                    }
                }
                onAuth_methodChanged: {
                    if(currentIndex != find(attrs_values.authentication_method)){
                        inited = true
                        currentIndex = find(attrs_values.authentication_method)
                    }
                }

                onCurrentIndexChanged: {
                    if(inited) {
                        attrs_values.authentication_method = model[currentIndex]
                    }
                }
            }

            XsLabel {
                visible: attrs_values.authentication_method == "client_credentials"
                text: "Client Identifier : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }

            TextField {
                Layout.fillWidth: true
                Layout.rightMargin: 6
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
                selectByMouse: true
                font.family: XsStyle.fontFamily
                font.hintingPreference: Font.PreferNoHinting
                color: XsStyle.hoverColor
                selectionColor: XsStyle.highlightColor
                background: Rectangle {
                    anchors.fill: parent
                    color: XsStyle.popupBackground
                    radius: 5
                }

                visible: attrs_values.authentication_method == "client_credentials"
                text: attrs_values.client_id ? attrs_values.client_id : null
                onEditingFinished: {attrs_values.client_id = text;}
            }

            XsLabel {
                visible: attrs_values.authentication_method == "password"
                text: "Username : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }

            TextField {
                Layout.fillWidth: true
                Layout.rightMargin: 6
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
                selectByMouse: true
                font.family: XsStyle.fontFamily
                font.hintingPreference: Font.PreferNoHinting
                color: XsStyle.hoverColor
                selectionColor: XsStyle.highlightColor
                background: Rectangle {
                    anchors.fill: parent
                    color: XsStyle.popupBackground
                    radius: 5
                }

                visible: attrs_values.authentication_method == "password"
                text: attrs_values.username ? attrs_values.username : null
                onEditingFinished: {attrs_values.username = text;}
            }


            XsLabel {
                visible: attrs_values.authentication_method == "session_token"
                text: "Session Token : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }

            TextField {
                Layout.fillWidth: true
                Layout.rightMargin: 6
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
                selectByMouse: true
                font.family: XsStyle.fontFamily
                font.hintingPreference: Font.PreferNoHinting
                color: XsStyle.hoverColor
                selectionColor: XsStyle.highlightColor
                background: Rectangle {
                    anchors.fill: parent
                    color: XsStyle.popupBackground
                    radius: 5
                }

                visible: attrs_values.authentication_method == "session_token"
                text: attrs_values.session_token ? attrs_values.session_token : null
                onEditingFinished: {attrs_values.session_token = text;}
            }

            XsLabel {
                visible: attrs_values.authentication_method == "client_credentials"
                text: "Client Secret : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }

            TextField {
                Layout.fillWidth: true
                Layout.rightMargin: 6
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft

                echoMode: TextInput.Password
                selectByMouse: true
                font.family: XsStyle.fontFamily
                font.hintingPreference: Font.PreferNoHinting
                color: XsStyle.hoverColor
                selectionColor: XsStyle.highlightColor
                background: Rectangle {
                    anchors.fill: parent
                    color: XsStyle.popupBackground
                    radius: 5
                }

                visible: attrs_values.authentication_method == "client_credentials"
                text: attrs_values.client_secret ? attrs_values.client_secret : null
                onEditingFinished: {
                    attrs_values.client_secret = text
                    // text = attrs_values.client_secret
                }
            }

            XsLabel {
                visible: attrs_values.authentication_method == "password"
                text: "Password : "
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
            }

            TextField {
                Layout.fillWidth: true
                Layout.rightMargin: 6
                Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft

                echoMode: TextInput.Password
                selectByMouse: true
                font.family: XsStyle.fontFamily
                font.hintingPreference: Font.PreferNoHinting
                color: XsStyle.hoverColor
                selectionColor: XsStyle.highlightColor
                background: Rectangle {
                    anchors.fill: parent
                    color: XsStyle.popupBackground
                    radius: 5
                }

                visible: attrs_values.authentication_method == "password"
                text: attrs_values.password ? attrs_values.password : null
                onEditingFinished: {
                    attrs_values.password = text
                    // text = attrs_values.password
                }
            }


            XsLabel {
                Layout.alignment: Qt.AlignVCenter|Qt.AlignRight
                Layout.fillHeight: true
            }
        }


        DialogButtonBox {
            id: myFooter
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.topMargin: 10
            Layout.minimumHeight: 35
            horizontalPadding: 12
            verticalPadding: 18
            background: Rectangle {
                anchors.fill: parent
                color: "transparent"
            }

            XsLabel {
                text: message
            }

            RoundButton {
                id: btnOK
                text: qsTr("Authenticate")
                width: 80
                height: 30
                radius: 5
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                background: Rectangle {
                    radius: 5
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
                    onClicked: {forceActiveFocus() ;accept()}
                }
            }
        }
    }
}

