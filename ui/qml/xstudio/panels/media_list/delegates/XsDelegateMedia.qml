// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.14
import Qt.labs.qmlmodels 1.0
import QtGraphicalEffects 1.0
import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import xstudio.qml.module 1.0
import xstudio.qml.helpers 1.0

DelegateChoice {
    roleValue: "Media"

    Component {
		Rectangle {
			id: control
		    width: itemWidth
		    height: itemHeight + (insertionFlag ? 20 : 0)

		    z: -100000
		    color:  ((index % 2 == 1) ? XsStyle.mediaListItemBG2 : XsStyle.mediaListItemBG1)

		    property int thumb_size: thumbSize
		    property int selection_indicator_width: selectionIndicatorWidth
		    property int filename_left: filenameLeft
		    property var playhead: viewport ? viewport.playhead : undefined
		    property bool online: mediaStatusRole ? mediaStatusRole == "Online" : true
		    property bool corrupt: mediaStatusRole ? mediaStatusRole == "Corrupt" : false
		    property real preview_frame: -1
		    property int imageVisible: 1
			property int selection_index: -1

			property var mediaSourceUuid: imageActorUuidRole

			property alias thumb: thumb_image1

		    property bool insertionFlag: false

		    property var media_item_model_index: control.DelegateModel.model.srcModel.index(-1,-1)
		    property var image_source_model_index: control.DelegateModel.model.srcModel.index(-1,-1)

		    // may not be required..
		    property var idrole: idRole
		    onIdroleChanged: updateProperties()

		    property int indexOfDelegate: index
		    onIndexOfDelegateChanged: updateProperties()

		    property bool moving: false
		    property bool copying: false

		    Component.onCompleted: {
	        	control.DelegateModel.model.srcModel.fetchMore(modelIndex())
				control.updateProperties()
	        	control.updateSelected()
		    }

		    Connections {
		    	target: dragContainer.dragged_media
		    	function onSelectionChanged() {
		    		if(dragContainer.dragged_media.selectedIndexes.length) {
		    			if(dragContainer.dragged_media.isSelected(modelIndex())) {
		    				if(dragContainer.Drag.supportedActions == Qt.CopyAction)
		    					copying = true
		    				else
		    					moving = true
		    			}
		    		} else {
		    			moving = false
		    			copying = false
		    		}
		    	}
		    }

			function modelIndex() {
				return media_item_model_index
			}

			function imageSouceIndex() {
				return image_source_model_index
			}

		    XsModuleAttributes {
		        id: colour_settings
		        attributesGroupNames: "colour_pipe_attributes"
		        onValueChanged: {
		            if(key == "display" || key == "view") {

						// the colour managment settings have changed. We
						// update the thumbnails to match.
						if(media_thumbnail.value) {
			                Future.promise(control.media_thumbnail.index.model.getThumbnailURLFuture(control.media_thumbnail.index, 0.5)
			                    ).then(function(url){
			                        control.media_thumbnail.value = url
									control.setSource(url)
			                    }
			                )
			            }
		                //control.imageChanged()
		            }
		        }
		    }

		    XsTimer {
		        id: delayTimer
    		}

		    property alias fileName: media_path.fileName

		    XsModelProperty {
		        id: media_path
		        role: "pathRole"
		        property string fileName: value ? helpers.fileFromURL(value) : "-"
		    }

		    XsModelProperty {
		        id: media_thumbnail
		        role: "thumbnailURLRole"
		        onValueChanged: control.imageChanged()
		    }

		    property alias media_thumbnail: media_thumbnail
		    property alias media_path: media_path

		    function updateProperties() {
		    	// console.log(control.DelegateModel.model.srcModel)
		    	// console.log(control.DelegateModel.model.rootIndex)
		    	if(control.DelegateModel.model.srcModel) {

			    	control.media_item_model_index = helpers.makePersistent(control.DelegateModel.model.srcModel.index(
			    		index, 0, control.DelegateModel.model.rootIndex
			    	))

			    	if(control.media_item_model_index.valid) {
						control.image_source_model_index = helpers.makePersistent(control.media_item_model_index.model.search_recursive(
							imageActorUuidRole, "actorUuidRole", control.media_item_model_index
						))

						if(control.image_source_model_index.valid) {
						    control.media_thumbnail.index = control.image_source_model_index
					    	control.media_path.index = control.image_source_model_index
						}
					}
				}
			}

			function handlePressedEvent(mouse) {
		        if (mouse.button == Qt.LeftButton) {
		            if(mouse.modifiers & Qt.ShiftModifier) {
		            	let sel = app_window.mediaSelectionModel.selectedIndexes
		            	let c = modelIndex().row
		            	let d = 0
		            	let r = c

		            	// find nearest
		            	for(let i =0; i< sel.length; i++) {
		            		let a = Math.abs(c-sel[i].row)

		            		if(d == 0 || a < d) {
		            			d = a
		            			r = sel[i].row
		            		}
		            	}

		            	let parent_ind = modelIndex().parent
		            	let indexs = []

		            	if(c < r)
			            	for(let i=r; i>=c; i--) {
			            		indexs.push(parent_ind.model.index(i,0,parent_ind))
			            	}
			            else
			            	for(let i=r; i<=c; i++) {
			            		indexs.push(parent_ind.model.index(i,0,parent_ind))
			            	}


		    			app_window.mediaSelectionModel.select(helpers.createItemSelection(indexs), ItemSelectionModel.Select)
		            } else if(mouse.modifiers & Qt.ControlModifier) {
		   				app_window.mediaSelectionModel.select(modelIndex(), ItemSelectionModel.Toggle)
		            } else if(mouse.modifiers & Qt.AltModifier) {
						// alt + click will launch a 'quick viewer'
						app_window.launchQuickViewer([actorRole], "Off")
				 	} else if(mouse.modifiers == Qt.NoModifier) {
		            	if(currentSource.index == screenSource.index){
			            	if(selection_index == -1) {
								app_window.sessionFunction.setActiveMedia(modelIndex(), true)
			            	} else {
								app_window.sessionFunction.setActiveMedia(modelIndex(), false)
			            	}
		            	} else {
		            		if(selection_index == -1)
			            		app_window.mediaSelectionModel.select(modelIndex(), ItemSelectionModel.ClearAndSelect)
		            	}
		            }
		        }
			}

		    onMediaSourceUuidChanged: updateProperties()

	    	MouseArea {
			    id: ma
				anchors.fill: parent

			    acceptedButtons: Qt.LeftButton | Qt.RightButton
				hoverEnabled: true

	       		onDoubleClicked: {
	       			app_window.sessionFunction.setActivePlaylist(modelIndex().parent.parent)
                    delayTimer.setTimeout(function() {
		       			app_window.sessionFunction.setActiveMedia(modelIndex(), false)
                    }, 100)
	       		}

			    onPressed: handlePressedEvent(mouse)

		        onReleased: {
		            if (mouse.button == Qt.RightButton) {
		                // mouse is relative to control, if we're inside a scrollable area
		                // we need to translate..
		                media_menu.x = mouse.x
		                media_menu.y = media_menu.parent.mapFromItem(control, mouse.x, mouse.y).y
		                media_menu.visible = true
		                return;
		            }
				}

			    Connections {
			    	target: app_window.mediaSelectionModel
			    	// could optimise ?
			    	function onSelectionChanged(selected, deselected) {
			    		control.updateSelected()
			    	}
			    }
			}

			Connections {
			    target: control.DelegateModel.model.srcModel

			    // function onRowsMoved(parent, first, count, target, first) {
			    // 	if(parent == modelIndex().parent || parent == modelIndex().parent.parent.parent)
				   //  	console.log(parent, first, count, target, first)
					  //   updateProperties()
			    // }

			    function onDataChanged(indexa,indexb,role) {
			    	if(control.modelIndex() == indexa && (!role.length || role.includes(sess.model.roleId("childrenRole")))) {
						updateProperties()
			    	}
			    }
			}

			function updateSelected() {
	    		let mi = modelIndex()
	    		if(app_window.mediaSelectionModel.isSelected(mi)) {
	    			selection_index = app_window.mediaSelectionModel.selectedIndexes.indexOf(mi)
	    		}
		    	else
		    		selection_index = -1
			}

		    Timer {
		        id: updateImage
		        interval: 100; running: true; repeat: false
		        onTriggered: imageChanged()
		    }

		    function imageChanged() {

	            if(imageVisible === 1) {
                    thumb_image2.source = media_thumbnail.value ? media_thumbnail.value : "qrc:///feather_icons/film.svg"
                    function finishImage(){
                        if(thumb_image2.status !== Image.Loading) {
                            thumb_image2.statusChanged.disconnect(finishImage);
                            imageVisible = 2
                            thumb_image1.source = media_thumbnail.value ? media_thumbnail.value : "qrc:///feather_icons/film.svg"
                        }
                    }
                    if (thumb_image2.status === Image.Loading){
                        thumb_image2.statusChanged.connect(finishImage);
                    } else {
                        finishImage();
                    }
	            } else {
                    thumb_image1.source = media_thumbnail.value ? media_thumbnail.value : "qrc:///feather_icons/film.svg"
                    function finishImage(){
                        if(thumb_image1.status !== Image.Loading) {
                            thumb_image1.statusChanged.disconnect(finishImage);
                            imageVisible = 1
                            thumb_image2.source = media_thumbnail.value ? media_thumbnail.value : "qrc:///feather_icons/film.svg"
                        }
                    }
                    if (thumb_image1.status === Image.Loading){
                        thumb_image1.statusChanged.connect(finishImage);
                    }
                    else {
                        finishImage();
                    }
	            }
		    }

		    function setSource(source){

		        var imageNew = imageVisible === 1 ? thumb_image2 : thumb_image1;
		        var imageOld = imageVisible === 2 ? thumb_image2 : thumb_image1;

		        imageNew.source = source;

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

		    function nearestPowerOf2(n) {
		        return 1 << 31 - Math.clz32(n);
		    }

		    // property alias control: control

		    Rectangle {
		    	anchors.fill: parent
		    	color: "transparent"
				anchors.topMargin: (insertionFlag ? 20 : 0)

			    Rectangle {
			        z:1
			    	anchors.fill: parent
			    	color: "transparent"
			    	border.width: 1
					border.color: XsStyle.highlightColor
			        anchors.margins: 1
			        visible: ma.containsMouse || thumb.containsMouse
			    }


			    Rectangle {
			        color: flagColourRole != undefined ? flagColourRole : "#00000000"
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


			    Rectangle {
			        id: selection_order

			        color: "transparent"
			        x: 2
			        width: control.selection_indicator_width
			        anchors.top: parent.top
			        anchors.left: parent.left
			        anchors.bottom: parent.bottom

			        Text {
			            anchors.fill: parent
			            anchors.margins: 4
			            text: selection_index + 1
			            visible: selection_index != -1 && app_window.mediaSelectionModel.selectedIndexes.length > 1
			            color: "white"
			            verticalAlignment: Text.AlignVCenter
			            horizontalAlignment: Text.AlignHCenter
			        }

			    }

			    MouseArea {
			    	id: thumb
			        width: thumb_size - 6
			        anchors.left: selection_order.right
			        anchors.top: parent.top
			        anchors.bottom: parent.bottom
			        anchors.margins: 3
	                hoverEnabled: true
	                z: 2
				    acceptedButtons: Qt.LeftButton

				    onPressed: handlePressedEvent(mouse)

	                onExited: {
			            if(media_thumbnail.value) {
		                	preview_frame = -1.0
	    	            	control.imageChanged()
	    	            }
	                }

		       		onDoubleClicked: {
		       			app_window.sessionFunction.setActivePlaylist(modelIndex().parent.parent)
	                    delayTimer.setTimeout(function() {
			       			app_window.sessionFunction.setActiveMedia(modelIndex(), false)
	                    }, 100)
		       		}

	                onPositionChanged: {
			            if(media_thumbnail.value) {
		                	preview_frame = mouseX / width
			                Future.promise(control.media_thumbnail.index.model.getThumbnailURLFuture(control.media_thumbnail.index, preview_frame)
			                    ).then(function(url){
			                        setSource(url)
			                    }
			                )
			            }
	                }

				    Image {
				        id: thumb_image1
				        z:1

				        source: media_thumbnail.value ? media_thumbnail.value : "qrc:///feather_icons/film.svg"
				        anchors.fill: parent

				        sourceSize.width: Math.max(128, nearestPowerOf2(width))
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

	                    onStatusChanged: {
	                        if (status == Image.Error) {
	                        	thumb_image1.source = "qrc:///feather_icons/film.svg"
	                        	thumb_image2.source = "qrc:///feather_icons/film.svg"
	                        	// parent.failed=true;
	                        	// opacity=0
	                        }
	                        // else if (status == Image.Ready)
	                        // 	opacity=1
	                        // else
	                        // 	opacity=0
	                    }
				    }

				    Image {
				        id: thumb_image2
				        source: media_thumbnail.value ? media_thumbnail.value : "qrc:///feather_icons/film.svg"

				        anchors.fill: parent
				        sourceSize.width: Math.max(128, nearestPowerOf2(width))

				        z:1
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
	                    onStatusChanged: {
	                        if (status == Image.Error) {
	                        	thumb_image1.source = "qrc:///feather_icons/film.svg"
	                        	thumb_image2.source = "qrc:///feather_icons/film.svg"
	                        	// parent.failed=true;
	                        	// opacity=0
	                        }
	                        // else if (status == Image.Ready)
	                        // 	opacity=1
	                        // else
	                        // 	opacity=0
	                    }

				    }
				}

				Rectangle {
			        color: "white"
			        anchors.fill: parent
			        visible: online && selection_index != -1
			        opacity: 0.2
					z: -1
			    }

			    Rectangle {
			        z:3
			        width: 12
			        height: 12
			        radius: 6
			        color: XsStyle.highlightColor
			        gradient: styleGradient.accent_gradient
			        anchors.bottom: thumb.bottom
			        anchors.right: thumb.right
			        anchors.margins: 1
			        visible: app_window.mediaSelectionModel.currentIndex == modelIndex()
			        id: playing_indicator
			    }

			    XsLabel {
			        id: bookmark_indicator
			        z:3
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
			        anchors.top: thumb.top
			        anchors.right: thumb.right
					visible: false

					property var actorUuid: actorUuidRole
					onActorUuidChanged: {
						rescan_for_bookmarks()
					}

					function rescan_for_bookmarks() {
						var vis = false
						var idx = app_window.bookmarkModel.search(actorUuidRole, "ownerRole")
						if (idx.valid) {
							var foo = app_window.bookmarkModel.search_list(actorUuidRole, "ownerRole", idx.parent, 0, -1)
							for (var i = 0; i < foo.length; ++i) {
								if (app_window.bookmarkModel.get(foo[i], "visibleRole")) {
									vis = true
									break
								}
							}
						}
						bookmark_indicator.visible = vis						
					}
					Connections {
			            target: app_window.bookmarkModel
			            function onLengthChanged() {
			                callback_delay_timer.setTimeout(
			                	function(){
									bookmark_indicator.rescan_for_bookmarks()
			                	},
			                	500
			                );
			            }
			        }

			        XsTimer {
			            id: callback_delay_timer
			        }
			    }

			    DropShadow {
			        anchors.fill: playing_indicator
			        horizontalOffset: 3
			        verticalOffset: 3
			        radius: 6.0
			        samples: 12
			        color: "#ff000000"
			        source: playing_indicator
			        visible: playing_indicator.visible
			        z:3
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
			        z:3
			    }

			    Rectangle {
			        color: "transparent"
			        x: filename_left
			        width: parent.width-control.filename_left
			        anchors.top: parent.top
			        anchors.bottom: parent.bottom

			        Text {
			            anchors.fill: parent
			            anchors.margins: 4
			            text: control.fileName
			            color: "white"
			            verticalAlignment: Text.AlignVCenter
			            elide: Text.ElideLeft
			        }
			    }

			    Rectangle {
			        color: "white"
			        anchors.fill: parent
			        visible: (online && selection_index != -1 && !moving && !copying)
			        opacity: 0.2
			    }

			    Rectangle {
			        color: "black"
			        anchors.fill: parent
			        visible: moving
			        opacity: 0.8
			    }

			    Rectangle {
			        color: "white"
			        anchors.fill: parent
			        visible: copying
			        opacity: 0.4
			    }

			    Rectangle {
			        color: corrupt ? (selection_index != -1 ? Qt.lighter("orange",1.2) : "orange") : (selection_index != -1 ?  Qt.lighter("red",1.2) : "red")
			        anchors.fill: parent
			        visible: !online
			        opacity: 0.2
			    }
			}
		}
	}

}