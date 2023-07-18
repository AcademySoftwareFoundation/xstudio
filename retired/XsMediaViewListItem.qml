// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0
import xstudio.qml.module 1.0
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0

Rectangle {
    id: control

    XsModuleAttributes {
        id: colour_settings
        attributesGroupNames: "colour_pipe_attributes"
        onValueChanged: {
            if(key == "display" || key == "view") {
                if(media_source && media_source.durationFrames != ""){
                    imageChanged()
                }
                else{
                    updateImage.running = true
                }
            }
        }
    }

    anchors.fill: parent
    color:  ((index & 1 == 1) ? XsStyle.mediaListItemBG2 : XsStyle.mediaListItemBG1)

    property var playhead: viewport ? viewport.playhead : undefined
    property var media_item: media_ui_object
    property var media_item_uuid: media_item ? media_item.uuid : null
    // property bool invalid: (media_item && media_source && media_source.streams ? false : true)
    // property bool missing_media: (!invalid && ! media_source.streams.length ? true : false)
    property bool online: media_item ? media_item.mediaOnline : false
    property bool corrupt: media_item ? media_item.mediaStatus == "Corrupt" : false

    property var media_source: media_item ? media_item.mediaSource : null

    property var thumb_width: thumb_image1.width
    property real preview_frame: -1
    property int imageVisible: 1

    z: -100000

    Timer {
        id: updateImage
        interval: 0; running: true; repeat: false
        onTriggered: imageChanged()
    }

    // delay event if object isn't ready..
    onOnlineChanged: {if(! updateImage.running){ imageChanged() }}

    function imageChanged() {
         if(online && media_source && media_source.durationFrames != "") {
            // Update invisible first..
            if(imageVisible === 1) {
                Future.promise(media_source.getThumbnailURLFuture(-1)
                    ).then(function(url){
                        thumb_image2.source_url = url
                        function finishImage(){
                            if(thumb_image2.status !== Image.Loading) {
                                thumb_image2.statusChanged.disconnect(finishImage);
                                imageVisible = 2
                                Future.promise(media_source.getThumbnailURLFuture(-1)
                                    ).then(function(url){
                                        thumb_image1.source_url = url
                                    }
                                )
                            }
                        }
                        if (thumb_image2.status === Image.Loading){
                            thumb_image2.statusChanged.connect(finishImage);
                        }
                        else {
                            finishImage();
                        }
                    }
                )
            } else {
                Future.promise(media_source.getThumbnailURLFuture(-1)
                    ).then(function(url){
                        thumb_image1.source_url = url
                        function finishImage(){
                            if(thumb_image1.status !== Image.Loading) {
                                thumb_image1.statusChanged.disconnect(finishImage);
                                imageVisible = 1
                                Future.promise(media_source.getThumbnailURLFuture(-1)
                                    ).then(function(url){
                                        thumb_image2.source_url = url
                                    }
                                )
                            }
                        }
                        if (thumb_image1.status === Image.Loading){
                            thumb_image1.statusChanged.connect(finishImage);
                        }
                        else {
                            finishImage();
                        }
                    }
                )
            }
        } else {
            if(!media_source || media_source.durationFrames == "") {
                // try again..
                updateImage.interval = 500
                updateImage.running = true
            } else {
                thumb_image1.source_url = "qrc:///feather_icons/film.svg"
                thumb_image2.source_url = "qrc:///feather_icons/film.svg"
            }
        }
    }

    function setSource(source){
        var imageNew = imageVisible === 1 ? thumb_image2 : thumb_image1;
        var imageOld = imageVisible === 2 ? thumb_image2 : thumb_image1;

        imageNew.source_url = source;

        function finishImage(){
            if(imageNew.status === Component.Ready) {
                imageNew.statusChanged.disconnect(finishImage);
                imageVisible = imageVisible === 1 ? 2 : 1;
            }
        }

        if (imageNew.status === Component.Loading){
            imageNew.statusChanged.connect(finishImage);
        }
        else {
            finishImage();
        }
    }

    onPreview_frameChanged: {
        if(!online || preview_frame == -1) {
            // restore bindings
            control.imageChanged()
        } else if (visible) {
            if(media_source && media_source.durationFrames != "") {
                Future.promise(media_source.getThumbnailURLFuture(preview_frame * (media_source.durationFramesNumeric-1))
                    ).then(function(url){
                        setSource(url)
                    }
                )
            }
        }
    }

    onVisibleChanged: {

        if (visible) {
            if(media_source && media_source.durationFrames != "") {
                var frame = preview_frame == -1 ? -1: preview_frame * (media_source.durationFramesNumeric-1);
                Future.promise(media_source.getThumbnailURLFuture(frame)
                    ).then(function(url){
                        setSource(url)
                    }
                )
            }
            thumb_image1.source = thumb_image1.source_url
            thumb_image2.source = thumb_image2.source_url
        }

    }

    Rectangle {
        color: media_item ? media_item.flag : "#00000000"
        width:3
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 1
    }

    XsBusyIndicator {
        id: loading_image1
        property bool loading: (imageVisible === 1 && (thumb_image1.status == Image.Loading || thumb_image1.status == Image.Null)) || (imageVisible === 2 && (thumb_image2.status == Image.Loading || thumb_image2.status == Image.Null))
        x: thumb_image1.x + thumb_image1.width/2 - width/2
        y: 3
        width: height
        height: parent.height-6
        z: 100
        visible: loading
        running: loading
    }

    function nearestPowerOf2(n) {
        return 1 << 31 - Math.clz32(n);
    }

    Image {
        id: thumb_image1
        z:1

        property var source_url: "qrc:///feather_icons/film.svg"
        source: "qrc:///feather_icons/film.svg"

        width: header.thumb_size-6
        sourceSize.width: Math.max(128, nearestPowerOf2(width))

        anchors.left: selection_order.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 3
        asynchronous: true
        fillMode: Image.PreserveAspectFit
        cache: true
        smooth: true
        transformOrigin: Item.Center
        layer {
            enabled: !online
            effect: ColorOverlay {
                color: XsStyle.controlTitleColor
            }
        }
        visible: imageVisible === 1

        onSource_urlChanged: {
            if (control.visible) {
                source = source_url
            }
        }
    }

    Image {
        id: thumb_image2

        property var source_url: "qrc:///feather_icons/film.svg"
        source: "qrc:///feather_icons/film.svg"

        width: header.thumb_size-6
        sourceSize.width: Math.max(128, nearestPowerOf2(width))

        z:1
        anchors.left: selection_order.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 3
        asynchronous: true
        fillMode: Image.PreserveAspectFit
        cache: true
        smooth: true
        transformOrigin: Item.Center
        layer {
            enabled: !online
            effect: ColorOverlay {
                color: XsStyle.controlTitleColor
            }
        }
        visible: imageVisible === 2

        onSource_urlChanged: {
            if (control.visible) {
                source = source_url
            }
        }

    }

    Rectangle {
        z:1
        width: 12
        height: 12
        radius: 6
        color: XsStyle.highlightColor
        gradient: styleGradient.accent_gradient
        anchors.bottom: thumb_image1.bottom
        anchors.right: thumb_image1.right
        anchors.margins: 1
        visible: playhead ? playhead.media.uuid == uuid : false
        id: playing_indicator
    }

    XsLabel {
        id: bookmark_indicator
        z:1
        // font.bold: true
        font.weight: Font.Black
        // style: Text.Outline
        // styleColor: "black"
        // sourceSize: 12
        text: "N"
        width: 12
        height: 12
        // opacity: 0.5
        font.pixelSize: 14
        anchors.margins: 1
        color: XsStyle.highlightColor
        anchors.top: thumb_image1.top
        anchors.right: thumb_image1.right
        // anchors.topMargin: 1
        // anchors.bottomMargin: 1
        // anchors.leftMargin: 1
        // anchors.rightMargin: 4
        visible: app_window.bookmarkModel.search(uuid, "ownerRole").valid

        Connections {
            target: app_window.bookmarkModel
            function onLengthChanged() {
                callback_delay_timer.setTimeout(function(){ bookmark_indicator.visible = app_window.bookmarkModel.search(uuid, "ownerRole").valid }, 500);
            }
        }

        Timer {
            id: callback_delay_timer
            function setTimeout(cb, delayTime) {
                callback_delay_timer.interval = delayTime;
                callback_delay_timer.repeat = false;
                callback_delay_timer.triggered.connect(cb);
                callback_delay_timer.triggered.connect(function release () {
                    callback_delay_timer.triggered.disconnect(cb); // This is important
                    callback_delay_timer.triggered.disconnect(release); // This is important as well
                });
                callback_delay_timer.start();
            }
        }
    }
    // XsColoredImage{
    //     id: bookmark_indicator
    //     // sourceSize: 12
    //     source: "qrc:/icons/notes.png"
    //     width: 12
    //     height: 12
    //     // opacity: 0.5
    //     iconColor: XsStyle.highlightColor
    //     anchors.top: thumb_image1.top
    //     anchors.right: thumb_image1.right
    //     anchors.margins: 1
    //     visible: session.bookmarks.ownedBookmarks.hasOwnProperty(uuid.toString().substr(1,36))
    // }


    // Rectangle {
    //     id: bookmark_indicator

    //     width: 12
    //     height: 12
    //     radius: 6
    //     opacity: 0.5
    //     color: XsStyle.highlightColor
    //     gradient: styleGradient.accent_gradient
    //     anchors.top: thumb_image1.top
    //     anchors.right: thumb_image1.right
    //     anchors.margins: 1
    //     visible: session.bookmarks.ownedBookmarks.hasOwnProperty(uuid.toString().substr(1,36))
    // }

    DropShadow {
        anchors.fill: playing_indicator
        horizontalOffset: 3
        verticalOffset: 3
        radius: 6.0
        samples: 12
        color: "#ff000000"
        source: playing_indicator
        visible: playing_indicator.visible
    }

    DropShadow {
        anchors.fill: bookmark_indicator
        horizontalOffset: 3
        verticalOffset: 3
        radius: 6.0
        samples: 12
        color: "#ff000000"
        source: bookmark_indicator
        visible: bookmark_indicator.visible
    }

    Rectangle {

        color: "transparent"
        x: 2
        width: header.selection_indicator_width
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        id: selection_order

        Text {
            anchors.fill: parent
            anchors.margins: 4
            text: selection_index
            visible: selection_index != 0 && selection_uuids.length > 1
            color: "white"
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }

    }

    Rectangle {
        color: "transparent"
        x: header.filename_left
        width: parent.width-header.filename_left
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        Text {
            anchors.fill: parent
            anchors.margins: 4
            text: media_item ? media_source ? media_source["fileName"] : "-" : "-"
            color: "white"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideLeft
        }
    }

    Rectangle {
        color: "white"
        anchors.fill: parent
        visible: online && selection_index != 0
        opacity: 0.2
    }

    Rectangle {
        color: corrupt ? "orange" : "red"
        anchors.fill: parent
        visible: !online
        opacity: 0.2
    }

    /*Rectangle {
        color: "grey"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
    }*/

}
