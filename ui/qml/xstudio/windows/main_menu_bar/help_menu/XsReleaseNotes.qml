// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Controls 2.14
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.3
import xstudio.qml.helpers 1.0
import xstudio.qml.semver 1.0

import xStudio 1.0

Item {

    property bool auto_show: false

    Loader {
        id: loader
    }

    // do the check 0.5s after UI has started up
    onAuto_showChanged: {

        if (auto_show) {
            callbackTimer.setTimeout(function() { return function() {
                semantic_version.show_features()
            }}(), 500);
        }
    }

    function showDialog() {
        loader.sourceComponent = release_notes
        loader.item.visible = true
    }

    /*****************************************************************
     *
     * New Version Check and show Release Notes
     *
     ****************************************************************/

    XsPreference {
        id: latest_version
        path:"/ui/qml/latest_version"
    }

    SemVer {
        id: semantic_version
        version: Qt.application.version

        function show_features() {

            if (semantic_version.compare(latest_version.value) > 0) {
                loader.sourceComponent = release_notes
                loader.item.visible = true
            }
            semantic_version.store()

        }

        function store() {
            if(semantic_version.compare(latest_version.value) > 0) {
                // immediate update..
                latest_version.value = Qt.application.version
            }
        }

    }

    Component {

        id: release_notes

        XsWindow {
            id: dlg

            height: 600
            width: 800

            XsPreference {
                id: features
                path: "/ui/qml/features"
            }

            property string myHTMLTitle: "<h1>Welcome to xSTUDIO v" + Qt.application.version + "</h1>"
            property string feature_text: ""

            Component.onCompleted: {
                if(features.value && features.value.length) {
                var xhr = new XMLHttpRequest;
                    xhr.open('GET', features.value+Qt.application.version);
                        xhr.onreadystatechange = function() {
                            if (xhr.readyState === XMLHttpRequest.DONE) {
                                feature_text = xhr.responseText
                            }
                        };
                    xhr.send();
                    // feature_text = features.value + Qt.application.version
                } else {
                    let t = helpers.readFile(studio.releaseDocsUrl())
                    const regex1 = /^[^@]+<div class="section" id="release-notes">/im;
                    t = t.replace(regex1, '');

                    const regex2 = /<footer>[^@]+$/im;
                    t = t.replace(regex2, '');

                    const regex3 = /<a[^>]+>[^<]+<\/a>/ig;
                    t = t.replace(regex3, '');
                    feature_text = t
                }

            }

            Item {
                id: wrapper

                anchors.fill: parent

                Image {
                    id: xIcon
                    source: "qrc:/images/xstudio_logo_256_v1.svg"
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.topMargin: 10
                    sourceSize.height: 128
                    sourceSize.width: 128
                    height: 128
                    width: 128
                }

                TextEdit{
                    id: titleText
                    wrapMode: TextEdit.Wrap
                    anchors.bottom: xIcon.bottom
                    anchors.left: xIcon.right
                    anchors.right: parent.right
                    anchors.leftMargin: 20
                    anchors.bottomMargin: 0
                    readOnly:true

                    text: myHTMLTitle
                    textFormat:TextArea.RichText
                    font.pixelSize: 16
                    font.family: XsStyleSheet.fontFamily
                    font.hintingPreference: Font.PreferNoHinting
                    color:"#fff"
                }

                Rectangle {
                    height: 1
        //                width: 100
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    anchors.bottom: flickArea.top
                    anchors.bottomMargin: 10
                    // gradient: styleGradient.accent_gradient
                    color: XsStyleSheet.accentColor
                }

                Flickable {
                    id: flickArea
                    anchors.top: xIcon.bottom
                    anchors.topMargin: 20
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: foot.top
                    anchors.bottomMargin: 20
                    anchors.leftMargin: 10
                    anchors.rightMargin: 7
                    height: 300
                    implicitHeight: 300
                    contentWidth: helpText.width
                    contentHeight: helpText.height

                    flickableDirection: Flickable.VerticalFlick
                    clip: true

                    Text {
                        id: helpText
                        wrapMode: Text.Wrap
                        anchors.top: parent.top
                        anchors.left: parent.left
                        width: 700
                        text: '<style>a:link { color:'+XsStyleSheet.accentColor+'; }</style>'+feature_text
                        textFormat: TextArea.RichText
                        font.pixelSize: 12
                        font.family: XsStyleSheet.fontFamily
                        font.hintingPreference: Font.PreferNoHinting
                        color:"#fff"

                        onLinkActivated: Qt.openUrlExternally(link)
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton // we don't want to eat clicks on the Text
                                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }
                    }
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AlwaysOn
                    }
                }

                Rectangle {
                    height: 1
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    anchors.top: flickArea.bottom
                    anchors.topMargin: 10
                    // gradient: styleGradient.accent_gradient
                    color: XsStyleSheet.accentColor
                }

                Rectangle {

                    id:foot
                    height: 40
                    color: "transparent"
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.left: parent.left
                    anchors.topMargin: 20

                    XsPrimaryButton{
                        id: btnOK
                        text: qsTr("OK")
                        width: XsStyleSheet.primaryButtonStdWidth*2
                        height: XsStyleSheet.primaryButtonStdHeight

                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.rightMargin: 8

                        onClicked: {
                            close()
                        }
                    }
                }
            }
            
        }
    }
}