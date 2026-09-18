import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import xStudio 1.0

import ".."
import "."

// Tree Container
Rectangle {

    SplitView.minimumWidth: showDirectoryTree ? 150 : 30
    SplitView.maximumWidth: showDirectoryTree ? 1000 : 30
    color: XsStyleSheet.panelBgColor

    // Collapsed State (Sidebar)
    Rectangle {

        anchors.fill: parent
        visible: !showDirectoryTree
        color: XsStyleSheet.widgetBgNormalColor
        
        ColumnLayout {
            anchors.fill: parent
                        
            XsSecondaryButton {
                Layout.preferredHeight: 24
                Layout.preferredWidth: 24
                Layout.topMargin: 4
                Layout.alignment: Qt.AlignHCenter
                imgSrc: "qrc:/icons/chevron_right.svg"
                imageSrcSize: width
                onClicked: showDirectoryTree = true
                text: "Show directory tree"
            }
            
            XsText {
                Layout.fillHeight: true
                Layout.fillWidth: true
                text: "Directories"
                rotation: 90
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    // Tree with Header
    ColumnLayout {
        anchors.fill: parent
        visible: showDirectoryTree
        spacing: 0
        
        // Header            
        Rectangle {
            color: XsStyleSheet.widgetBgNormalColor
            Layout.fillWidth: true
            Layout.preferredHeight: XsStyleSheet.primaryButtonStdHeight
                        
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 5
                
                XsText {
                    text: "Directories"
                    Layout.fillWidth: true
                }

                XsSecondaryButton {
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: 24
                    Layout.alignment: Qt.AlignVCenter
                    imgSrc: "qrc:/icons/chevron_right.svg"
                    rotation: 180
                    imageSrcSize: width
                    onClicked: showDirectoryTree = false
                    text: "Hide directory tree"
                }

            }
        }
                
        // Directory Tree Component
        FSDirectoryTreeBody {
            id: dirTree
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            currentPath: current_path_attr.value            
            onSendCommand: (cmd) => root.sendCommand(cmd)
            
        }
    }
}
