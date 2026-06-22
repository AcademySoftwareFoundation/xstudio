import xstudio.qml.models 1.0
import xStudio 1.0


XsPopupMenu {

    id: timelineMenu
    visible: false
    menu_model_name: "timeline_new_track_menu_"+timelineMenu

    property var panelContext: helpers.contextPanel(timelineMenu)
    property var theTimeline: panelContext.theTimeline
    property var timelineSelection: theTimeline.timelineSelection


    XsMenuModelItem {
        text: qsTr("New Video Track")
        menuPath: ""
        menuItemPosition: 1
        menuModelName: timelineMenu.menu_model_name
        onActivated: theTimeline.addTrack("Video Track")
    }
    XsMenuModelItem {
        text: qsTr("New Audio Track")
        menuPath: ""
        menuItemPosition: 2
        menuModelName: timelineMenu.menu_model_name
        onActivated: theTimeline.addTrack("Audio Track")
    }
}