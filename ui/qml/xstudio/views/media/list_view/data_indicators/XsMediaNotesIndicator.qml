// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.14
import xStudio 1.0
import QtQuick.Layouts 1.15

Item{

    id: notesDiv
    clip: true

    property var gotBookmark: false
    property var gotBookmarkAnnotation: false
    property var gotBookmarkGrade: false
    property var gotBookmarkTransform: false
    property var gotBookmarkUuids_: bookmarkUuidsRole

    onGotBookmarkUuids_Changed: {
        var bm = false
        var anno = false
        var grade = false
        if (bookmarkUuidsRole && bookmarkUuidsRole.length) {
            for (var i in bookmarkUuidsRole) {
                var idx = bookmarkModel.searchRecursive(bookmarkUuidsRole[i], "uuidRole")
                if (idx.valid) {
                    if(bookmarkModel.get(idx,"userTypeRole") == "Grading") {
                        grade = true
                    } else if (bookmarkModel.get(idx,"hasAnnotationRole")) {
                        anno = true
                    }
                    if(bookmarkModel.get(idx,"noteRole") != "")
                        bm = true
                }
            }
        }
        gotBookmark = bm
        gotBookmarkGrade = grade
        gotBookmarkAnnotation = anno
    }

    RowLayout{

        id: attachmentIconsRow
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        spacing: itemPadding/2

        // TODO: add notes/annotation/colour/transform status data to
        // MediaItem in the main data model to drive these buttons ...

        XsSecondaryButton {

            enabled: false

            Layout.minimumWidth: XsStyleSheet.secondaryButtonStdWidth
            Layout.maximumWidth: XsStyleSheet.secondaryButtonStdWidth
            Layout.minimumHeight: XsStyleSheet.secondaryButtonStdWidth
            Layout.maximumHeight: XsStyleSheet.secondaryButtonStdWidth
            imgSrc: "qrc:/icons/sticky_note.svg"
            isColoured: gotBookmark
            onlyVisualyEnabled: gotBookmark
        }

        XsSecondaryButton {

            enabled: false

            Layout.minimumWidth: XsStyleSheet.secondaryButtonStdWidth
            Layout.maximumWidth: XsStyleSheet.secondaryButtonStdWidth
            Layout.minimumHeight: XsStyleSheet.secondaryButtonStdWidth
            Layout.maximumHeight: XsStyleSheet.secondaryButtonStdWidth
            imgSrc: "qrc:/icons/brush.svg"
            isColoured: gotBookmarkAnnotation
            onlyVisualyEnabled: gotBookmarkAnnotation

        }

        XsSecondaryButton {

            enabled: false

            Layout.minimumWidth: XsStyleSheet.secondaryButtonStdWidth
            Layout.maximumWidth: XsStyleSheet.secondaryButtonStdWidth
            Layout.minimumHeight: XsStyleSheet.secondaryButtonStdWidth
            Layout.maximumHeight: XsStyleSheet.secondaryButtonStdWidth
            imgSrc: "qrc:/icons/tune.svg"
            isColoured: gotBookmarkGrade
            onlyVisualyEnabled: gotBookmarkGrade

        }

        XsSecondaryButton {

            enabled: false
            Layout.minimumWidth: XsStyleSheet.secondaryButtonStdWidth
            Layout.maximumWidth: XsStyleSheet.secondaryButtonStdWidth
            Layout.minimumHeight: XsStyleSheet.secondaryButtonStdWidth
            Layout.maximumHeight: XsStyleSheet.secondaryButtonStdWidth
            imgSrc: "qrc:/icons/open_with.svg"
            isColoured: gotBookmarkTransform
            onlyVisualyEnabled: gotBookmarkTransform

        }

    }
    Rectangle{
        width: headerThumbWidth;
        height: parent.height
        anchors.right: parent.right
        color: headerThumbColor
    }

}