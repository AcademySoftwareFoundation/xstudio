// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.14

import xStudio 1.0
import ShotBrowser 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0

XsWindow{

    title: "ShotGrid Authentication"
    property string message: ""

    property real itemHeight: btnHeight
    property real itemSpacing: 5

    width: 460
    height: 240
    minimumWidth: width
    maximumWidth: width
    minimumHeight: height
    maximumHeight: height
    palette.base: XsStyleSheet.panelTitleBarColor

    XsModuleData {
        id: shotbrowser_datasource_preference_model
        modelDataName: "shotbrowser_datasource_preference"
    }

    XsAttributeValue {
        id: __authentication_method
        attributeTitle: "authentication_method"
        model: shotbrowser_datasource_preference_model
    }
    property alias authentication_method: __authentication_method.value

    XsAttributeValue {
        id: __client_id
        attributeTitle: "client_id"
        model: shotbrowser_datasource_preference_model
    }
    property alias client_id: __client_id.value

    XsAttributeValue {
        id: __client_secret
        attributeTitle: "client_secret"
        model: shotbrowser_datasource_preference_model
    }
    property alias client_secret: __client_secret.value


    XsAttributeValue {
        id: __username
        attributeTitle: "username"
        model: shotbrowser_datasource_preference_model
    }
    property alias username: __username.value

    XsAttributeValue {
        id: __password
        attributeTitle: "password"
        model: shotbrowser_datasource_preference_model
    }
    property alias password: __password.value

    XsAttributeValue {
        id: __session_token
        attributeTitle: "session_token"
        model: shotbrowser_datasource_preference_model
    }
    property alias session_token: __session_token.value


    XsAttributeValue {
        id: __authentication_methods
        attributeTitle: "authentication_method"
        model: shotbrowser_datasource_preference_model
        role: "combo_box_options"
    }
    property alias authentication_methods: __authentication_methods.value

    ColumnLayout {
        anchors.fill: parent
        spacing: itemSpacing

        Item{
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight/2
        }
        XsTextWithComboBox{ id: authMethod
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            text: "Authentication Method :"
            property bool ready: false

            onCurrentIndexChanged: {
                if(ready && currentIndex != -1 ) {
                    authentication_method = model[currentIndex]
                }
            }

            model: authentication_methods && authentication_methods.length ? authentication_methods : []
            onModelChanged: {
                if(model.length && authentication_method != undefined) {
                    ready = true
                    currentIndex = valueDiv.find(authentication_method)
                }
            }

            property var auth_method: authentication_method ? authentication_method : null
            onAuth_methodChanged: {
                if(ready && valueDiv.find(authentication_method) != -1 && currentIndex != valueDiv.find(authentication_method)){
                    currentIndex = valueDiv.find(authentication_method)
                }
            }

        }
        Item{
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight/2
        }

        XsTextWithInputField{ id: clId
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            label: "Client Identifier :"
            visible: authentication_method == "client_credentials"
            value: client_id ? client_id : null

            onEditingCompleted: {client_id = text}
        }
        XsTextWithInputField{ id: clSecret
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            label: "Client Secret :"
            echoMode: TextInput.Password
            visible: authentication_method == "client_credentials"
            value: client_secret ? client_secret : null

            onEditingCompleted: {client_secret = text}
        }

        XsTextWithInputField{ id: userName
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            label: "Username :"
            visible: authentication_method == "password"
            value: username ? username : null

            onEditingCompleted: {username = text}
        }
        XsTextWithInputField{ id: passWord
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            label: "Password :"
            echoMode: TextInput.Password //PasswordEchoOnEdit
            visible: authentication_method == "password"
            value: password ? password : null

            onEditingCompleted: {password = text}
        }

        XsTextWithInputField{ id: sessToken
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            label: "Session Token :"
            visible: authentication_method == "session_token"
            value: session_token ? session_token : null

            onEditingCompleted: {username = text}
        }
        Item{ id: sessDummy
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            visible: authentication_method == "session_token"
        }

        Item{ id: msgDiv
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            XsText{
                width: parent.width - itemSpacing*2
                height: message? itemHeight : 0
                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                color: XsStyleSheet.errorColor
                wrapMode: Text.Wrap
                text: message
            }
        }
        Item{ id: authBtnDiv
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight

            RowLayout{
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 10

                Item{
                    Layout.preferredWidth: parent.width/2
                    Layout.fillHeight: true
                }
                XsPrimaryButton{
                    Layout.fillWidth: true
                    Layout.preferredWidth: parent.width/4
                    Layout.fillHeight: true
                    text: "Cancel"
                    onClicked: {
                        close()
                    }
                }
                XsPrimaryButton{ id: authBtn
                    Layout.fillWidth: true
                    Layout.preferredWidth: parent.width/4
                    Layout.fillHeight: true
                    text: "Authenticate"
                    onClicked: {
                        ShotBrowserEngine.authenticate(false)

                        forceActiveFocus()
                        if(message == "") close()
                    }
                }
            }
        }

        Item{
            Layout.fillWidth: true
            Layout.preferredHeight: itemHeight/2
        }

    }



}

