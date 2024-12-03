import QtQuick 2.15
import QtQuick.Layouts 1.3
import xStudio 1.0

Item {

    id: view
    property var possibleSources: ["No Media"]
    property var parent_menu
    onVisibleChanged: {
        if (visible) {
            // use first item in media list selection
            var l = mediaSelectionModel.selectedIndexes;
            if (!l.length) {
                possibleSources = []
                return
            }
            // get the indexes to the media sources for the media item
            possibleSources = theSessionData.getMediaSourceNames(l[0], true)
        }
    }

    property var minWidth: 50
    width: parent.width
    height: layout.height

    function setMinWidth(mw) {
        if (mw > minWidth) {
            minWidth = mw
        }
    }
    ColumnLayout {
        id: layout
        width: minWidth
        Repeater {
            model: possibleSources
            XsMenuItemBasic {
                Layout.fillWidth: true
                menu_item_text: possibleSources[index]
                onClicked: {
                    var l = mediaSelectionModel.selectedIndexes;
                    for (var i in l) {
                        theSessionData.setMediaSource(l[i], menu_item_text, true)
                    }
                    if (parent_menu) parent_menu.closeAll()
                }
                onMinWidthChanged: {
                    view.setMinWidth(minWidth)
                }
            }
        }
    }
}