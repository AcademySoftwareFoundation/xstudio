// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12
import QtQml.Models 2.14
import xStudioReskin 1.0

Rectangle{ 
    
    id: thumbnailDiv
    onWidthChanged: {
        rowHeight = width / (16/9) //to keep 16:9 ratio  
    }
    color: "transparent"
    border.width: 1
    border.color: "transparent"
    
    XsText{
        text: "No\nImage" //#TODO
        width: parent.width - itemPadding*2
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        font.weight: isActive? Font.ExtraBold : Font.Normal
        visible: !thumbnailURLRole && !isMissing
        color: hintColor
        //lineHeight: activeStateOnIndex? (width/100)*1.5 : .7 
        // font.pixelSize: XsStyleSheet.fontSize-2
    }
    XsImage{ id: thumbnailImgDiv
        visible: source!=""
        clip: true
        width: parent.width - border.width*2
        height: parent.height
        anchors.centerIn: parent
        fillMode: Image.PreserveAspectFit //isMissing ? Image.PreserveAspectFit : Image.Stretch 
        source: isMissing? "qrc:/icons/error.svg" : thumbnailURLRole ? thumbnailURLRole : "qrc:/icons/theaters.svg"
        rotation: source=="qrc:/icons/theaters.svg"? 90 : 0
        imgOverlayColor: "transparent"//isMissing? errorColor : _thumbnail? "transparent" : hintColor
    }
   // Rectangle{visible: isActive && !activeStateOnIndex; anchors.fill: parent; color: "transparent"; border.width: 2; border.color: borderColorHovered}

    Rectangle{
        width: headerThumbWidth; 
        height: parent.height
        anchors.right: parent.right
        color: bgColorPressed
    }

}