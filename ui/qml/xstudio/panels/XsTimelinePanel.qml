// SPDX-License-Identifier: Apache-2.0
import QtQuick.Controls 2.3
import QtQuick 2.14
import QtQuick.Layouts 1.3

import xStudio 1.0

Rectangle {
    id: timeline
    color: XsStyle.mainBackground

    Flickable {
      contentWidth: image.width
      contentHeight: image.height
      flickableDirection: Flickable.HorizontalFlick

      Image {
        id: image
        source: "qrc:/images/timeline_mockup.png"
      }
      ScrollBar.vertical: ScrollBar {
        policy: ScrollBar.AlwaysOn
      }
      ScrollBar.horizontal: ScrollBar {
        policy: ScrollBar.AlwaysOn
      }

    }

    Rectangle {
      z: 100
      anchors.fill: parent
      color: XsStyle.mainBackground
      opacity: 0.6
      Label {

        text: 'Coming soon'
        color: XsStyle.controlTitleColor
        opacity: 0.6
        rotation: 20
        anchors.centerIn: parent
        verticalAlignment: Qt.AlignVCenter
        horizontalAlignment: Qt.AlignHCenter
        font.pixelSize: 50
  
      }
  
    }
}