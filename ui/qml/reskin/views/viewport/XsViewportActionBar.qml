import QtQuick 2.12
import QtQuick.Layouts 1.15
// import QtQml.Models 2.14

import xStudioReskin 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import "./widgets"

Item{id: actionDiv
    width: parent.width;
    height: btnHeight+(panelPadding*2)

    property real btnHeight: XsStyleSheet.widgetStdHeight+4
    property real panelPadding: XsStyleSheet.panelPadding

    property string actionbar_model_data_name

    /*************************************************************************

        Access Playhead data

    **************************************************************************/

    // Get the UUID of the current onscreen media from the playhead
    XsModelProperty {
        id: __playheadSourceUuid
        role: "value"
        index: currentPlayheadData.search_recursive("Current Media Uuid", "title")
    }
    XsModelProperty {
        id: __playheadMediaSourceUuid
        role: "value"
        index: currentPlayheadData.search_recursive("Current Media Source Uuid", "title")
    }

    Connections {
        target: currentPlayheadData // this bubbles up from XsSessionWindow
        function onJsonChanged() {
            __playheadSourceUuid.index = currentPlayheadData.search_recursive("Current Media Uuid", "title")
            __playheadMediaSourceUuid.index = currentPlayheadData.search_recursive("Current Media Source Uuid", "title")
        }
    }
    property alias mediaUuid: __playheadSourceUuid.value
    property alias mediaSourceUuid: __playheadMediaSourceUuid.value

    // When the current onscreen media changes, search for the corresponding
    // node in the main session data model
    onMediaUuidChanged: {

        // TODO - current this gets us to media actor, not media source actor,
        // so we can't get to the file name yet
        mediaData.index = theSessionData.search_recursive(
            mediaUuid,
            "actorUuidRole",
            viewedMediaSetIndex
            )
    }

    onMediaSourceUuidChanged: {
        mediaSourceData.index = theSessionData.search_recursive(
            mediaSourceUuid,
            "actorUuidRole",
            viewedMediaSetIndex
            )
    }

    // this gives us access to the 'role' data of the entry in the session model
    // for the current on-screen media 
    XsModelPropertyMap {
        id: mediaData
        index: theSessionData.invalidIndex()
    }

     // this gives us access to the 'role' data of the entry in the session model
    // for the current on-screen media SOURCE
    XsModelPropertyMap {
        id: mediaSourceData
        property var fileName: {
            let result = "TBD"
            if(index.valid && values.pathRole != undefined) {
                result = helpers.fileFromURL(values.pathRole)
            }
            return result
        }
    }

    /*************************************************************************/

    RowLayout{
        x: panelPadding
        spacing: 1
        width: parent.width-(x*2)
        height: btnHeight
        anchors.verticalCenter: parent.verticalCenter

        XsPrimaryButton{ id: transformBtn
            Layout.preferredWidth: 40
            Layout.preferredHeight: parent.height
            imgSrc: "qrc:/icons/open_with.svg"
            onClicked:{
                isActive = !isActive
            }
        }
        XsPrimaryButton{ id: colourBtn
            Layout.preferredWidth: 40
            Layout.preferredHeight: parent.height
            imgSrc: "qrc:/icons/tune.svg"
            onClicked:{
                isActive = !isActive
            }
        }
        XsPrimaryButton{ id: drawBtn
            Layout.preferredWidth: 40
            Layout.preferredHeight: parent.height
            imgSrc: "qrc:/icons/brush.svg"
            onClicked:{
                isActive = !isActive
            }
        }
        XsPrimaryButton{ id: notesBtn
            Layout.preferredWidth: 40
            Layout.preferredHeight: parent.height
            imgSrc: "qrc:/icons/sticky_note.svg"
            onClicked:{
                isActive = !isActive
            }
        }
        XsText{
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height
            text: mediaSourceData.fileName
            font.bold: true
        }
        XsPrimaryButton{ id: resetBtn
            Layout.preferredWidth: 40
            Layout.preferredHeight: parent.height
            imgSrc: "qrc:/icons/reset_tv.svg"
            onClicked:{
                zoomBtn.isZoomMode = false
                panBtn.isActive = false
            }
        }

        XsModuleData {
            id: actionbar_model_data
            modelDataName: actionbar_model_data_name
        }

        Repeater {

            id: the_view
            model: actionbar_model_data

            delegate: XsPrimaryButton{
                id: zoomBtn
                Layout.preferredWidth: 40
                Layout.preferredHeight: parent.height
                imgSrc: title == "Zoom (Z)" ? "qrc:/icons/zoom_in.svg" : "qrc:/icons/pan.svg"
                isActive: value
                onClicked:{
                    value = !value
                }
            }
        }

        /*XsPrimaryButton{ id: zoomBtn
            Layout.preferredWidth: 40
            Layout.preferredHeight: parent.height
            imgSrc: "qrc:/icons/zoom_in.svg"
            isActive: isZoomMode
            property bool isZoomMode: false
            onClicked:{
                isZoomMode = !isZoomMode
                panBtn.isActive = false
            }
        }
        XsPrimaryButton{ id: panBtn
            Layout.preferredWidth: 40
            Layout.preferredHeight: parent.height
            imgSrc: "qrc:/icons/pan.svg"
            onClicked:{
                isActive = !isActive
                zoomBtn.isZoomMode = false
            }
        }*/

        XsPrimaryButton{ id: moreBtn
            Layout.preferredWidth: 40
            Layout.preferredHeight: parent.height
            imgSrc: "qrc:/icons/more_vert.svg"
        }
    }

}