// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQml 2.15
import xstudio.qml.bookmarks 1.0
import QtQml.Models 2.14
import QtQuick.Dialogs 1.3 //for ColorDialog
import QtGraphicalEffects 1.15 //for RadialGradient

import xStudio 1.0
import xstudio.qml.models 1.0
import Grading 2.0

Item {
    property real dividerWidth: 2

    Rectangle{ id: bg
        anchors.fill: parent
        color: XsStyleSheet.baseColor
    }

    RowLayout{ id: controlsDiv
        anchors.fill: bg
        anchors.margins: spacing
        spacing: 0

        Repeater{ id: repeater
            model: attrs.grading_sliders_model

            Row{
                Layout.minimumWidth: (parent.width-(dividerWidth*(repeater.count-1)))/repeater.count + dividerWidth
                Layout.preferredWidth: (parent.width-(dividerWidth*(repeater.count-1)))/repeater.count + dividerWidth
                Layout.fillHeight: true

                GTSliderDiv{
                    titleText: abbr_title
                    width: parent.width - dividerWidth
                    height: parent.height
                }
                Rectangle{ id: divider
                    width: dividerWidth
                    height: parent.height
                    color: XsStyleSheet.panelBgColor
                }
            }
        }

        Item{
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
        } 
    }

}