// SPDX-License-Identifier: Apache-2.0

import QtQuick

import QtQuick.Layouts

import xstudio.qml.bookmarks 1.0

import QuickFuture 1.0
import QuickPromise 1.0

import xStudio 1.0
import xstudio.qml.conform 1.0
import xstudio.qml.models 1.0
import xstudio.qml.helpers 1.0
import xstudio.qml.viewport 1.0


Item{

	XsConformEngine	{
		id: engine

        onRowsInserted: {
            replaceModel.notifyModel = engine
            replaceModel.rootIndex = engine.index(-1,-1)
            compareModel.notifyModel = engine
            compareModel.rootIndex = engine.index(-1,-1)
        }

        Component.onCompleted: {
            replaceModel.notifyModel = engine
            replaceModel.rootIndex = engine.index(-1,-1)
            compareModel.notifyModel = engine
            compareModel.rootIndex = engine.index(-1,-1)
        }
	}


   Component.onCompleted: {
        // make sure the 'Add' sub-menu appears in the correct place
        helpers.setMenuPathPosition("More", "media_list_menu_", 199)
    }

    XsPreference {
        id: siblingPref
        path: "/core/conform/conform_sibling_count"
    }

       XsHotkey {
        id: next_version_hotkey
        sequence: "Alt+n"
        name: "Next Version"
        description: "Replace With Next Version"
        onActivated: (menuContext) =>  {
            replaceSelection("Next Version", mediaSelectionModel.selectedIndexes)
        }
    }
    XsHotkey {
        id: previous_version_hotkey
        sequence: "Alt+p"
        name: "Previous Version"
        description: "Replace With Previous Version"
        onActivated: (menuContext) =>  {
            replaceSelection("Previous Version", mediaSelectionModel.selectedIndexes)
        }
    }
    XsHotkey {
        id: latest_version_hotkey
        sequence: "Alt+l"
        name: "Latest Version"
        description: "Replace With Latest Version"
        onActivated: (menuContext) =>  {
            replaceSelection("Latest Version", mediaSelectionModel.selectedIndexes)
        }
    }

    function removeDuplicates(selection) {
        // build list of clips..
        if(selection.length) {
            let clips = []
            let model = selection[0].model
            for(let i = 0; i< selection.length; i++) {
                let type = model.get(selection[i], "typeRole")

                if(type == "Clip" && !model.get(selection[i], "lockedRole"))
                    clips.push(selection[i])
                else if(type == "Audio Track" || type == "Video Track" ) {
                    for(let j = 0; j < model.rowCount(selection[i]); j++) {
                        let item = model.index(j, 0, selection[i])
                        if(model.get(item, "typeRole") == "Clip" && !model.get(item, "lockedRole"))
                            clips.push(item)
                    }
                }
            }

            if(clips.length) {
                let timeline_index = model.getTimelineIndex(clips[0])
                for(let i =0; i< clips.length; i++) {
                    Future.promise(
                        engine.conformFindRelatedFuture(
                            "",
                            clips[i],
                            timeline_index)
                    ).then(
                        function(clip_uuid_list) {
                            for(let j=0 ; j< clip_uuid_list.length; j++) {
                                let cind = model.searchRecursive(clip_uuid_list[j], "idRole", timeline_index)

                                if(cind.valid && cind.parent != clips[i].parent) {
                                    model.removeTimelineItems([clips[i]])
                                    break
                                }
                            }
                        },
                        function() {
                        }
                    )
                }
            }
        }
    }

    function replaceSelection(task, selection) {
        for(let i=0; i< selection.length; i++) {
            let pl_index = selection[i].model.getContainerIndex(selection[i])
            Future.promise(
                engine.conformItemsFuture(
                    task,
                    pl_index,
                    selection[i], true, true)
            ).then(
                function(media_uuid_list) {
                    // add to current selection..
                    // As we replaced something that was selected.
                    mediaSelectionModel.selectNewMedia(pl_index, media_uuid_list, -1, ItemSelectionModel.Select)
                },
                function() {
                }
            )
        }
    }

    function replaceSelectionTimeline(task, selection) {
        for(let i=0; i< selection.length; i++) {
            Future.promise(
                engine.conformItemsFuture(task,
                    selection[i].model.getContainerIndex(selection[i]),
                    selection[i], true, false)
            ).then(
                function(media_uuid_list) {
                    // console.log(media_uuid_list)
                },
                function() {
                }
            )
        }
    }

    function autoConformSelectionTimeline(task, src, dst) {
        // purge dst and clone src into it.
        if(theSessionData.replaceTimelineTrack(src, dst)){
            dst.model.set(dst, task, "nameRole")
            Future.promise(
                engine.conformItemsFuture(task,
                    dst.model.getContainerIndex(dst),
                    dst, true, true)
            ).then(
                function(media_uuid_list) {
                    // console.log(media_uuid_list)
                },
                function() {
                }
            )
        }
    }

    function conformSelectionTimeline(task, selection) {
        let clips = theSessionData.duplicateTimelineClips(selection, task, "");
        for(let i=0; i< clips.length; i++) {
            // reset clip colours.
            clips[i].model.set(clips[i], "", "flagColourRole")
            Future.promise(
                engine.conformItemsFuture(task,
                    clips[i].model.getContainerIndex(clips[i]),
                    clips[i], true, true)
            ).then(
                function(media_uuid_list) {
                    // console.log(media_uuid_list)
                },
                function() {
                }
            )
        }
    }

	function compareSelection(task, selection) {
    	for(let i=0; i< selection.length; i++) {
            Future.promise(
                engine.conformItemsFuture(task,
                    selection[i].model.getContainerIndex(selection[i]),
                    selection[i], true, false)
            ).then(
            	function(media_uuid_list) {
                    // create new selection.
        			// console.log(media_uuid_list)

                    let tmp = []
                    for(let i=0;i<selection.length;i++)
                        tmp.push(selection[i].model.get(selection[i], "actorUuidRole"))

                    for(let i=0;i<media_uuid_list.length;i++)
                        tmp.push(helpers.QVariantFromUuidString(media_uuid_list[i]))

                    mediaSelectionModel.selectNewMedia(selection[i].model.getContainerIndex(selection[i]), tmp)

            	},
            	function() {
            	}
            )
    	}
	}

    function replaceToSequence(selection, sequenceIndex, conformTrackIndex=engine.index(-1,-1)) {
        if(selection.length && sequenceIndex.valid && sequenceIndex.model.get(sequenceIndex, "typeRole") == "Timeline") {
            Future.promise(
                engine.conformToSequenceFuture(
                    selection[0].model.getPlaylistIndex(selection[0]),
                    selection,
                    sequenceIndex,
                    conformTrackIndex,
                    true)
            ).then(
                function(media_uuid_list) {
                    // create new selection.
                    // console.log(media_uuid_list)
                },
                function() {
                }
            )
        }
    }

    function conformToSequence(selection, sequenceIndex, trackName="", conformTrackIndex=engine.index(-1,-1)) {
        if(selection.length && sequenceIndex.valid && sequenceIndex.model.get(sequenceIndex, "typeRole") == "Timeline") {
            Future.promise(
                engine.conformToSequenceFuture(
                    selection[0].model.getPlaylistIndex(selection[0]),
                    selection,
                    sequenceIndex,
                    conformTrackIndex,
                    false,
                    trackName)
            ).then(
                function(media_uuid_list) {
                    // create new selection.
                    // console.log(media_uuid_list)
                },
                function() {
                }
            )
        }
    }

    function conformToNewSequence(selection, playlistIndex=helpers.qModelIndex()) {
        if(selection.length) {
            Future.promise(
                engine.conformToNewSequenceFuture(
                    selection, "", -1, playlistIndex
                )
            ).then(
                function(uuid_list) {
                    // console.log("nope", uuid_list)
                },
                function(err) {
                    dialogHelpers.errorDialogFunc("Conform With", "No Sequence found for media")
                }
            )
        }
    }

    function conformWithToNewSequence(selection, task, playlistIndex=helpers.qModelIndex()) {
        if(selection.length) {
            Future.promise(
                engine.conformToNewSequenceFuture(
                    selection, task, -1, playlistIndex
                )
            ).then(
                function(uuid_list) {
                    // console.log("nope", uuid_list)
                },
                function(err) {
                    dialogHelpers.errorDialogFunc("Conform With", "No Sequence found for media")
                }
            )
        }
    }

    function conformWithSiblingsToNewSequence(selection, task, playlistIndex=helpers.qModelIndex()) {
        if(selection.length) {
            Future.promise(
                engine.conformToNewSequenceFuture(
                    selection, task, siblingPref.value, playlistIndex
                )
            ).then(
                function(uuid_list) {
                    // console.log("nope", uuid_list)
                },
                function(err) {
                    dialogHelpers.errorDialogFunc("Conform With", "No Sequence found for media")
                }
            )
        }
    }

    function conformTracksToSequence(trackIndexes, sequenceIndex) {
        if(trackIndexes.length) {
            Future.promise(
                engine.conformTracksToSequenceFuture(
                    trackIndexes, sequenceIndex
                )
            ).then(
                function(uuid_list) {
                },
                function() {
                }
            )
        }
    }

    function autoConformFromTrackName(conformSourceIndex, selectedIndexes) {
        for(let i=0;i<selectedIndexes.length;i++) {
            let type = selectedIndexes[i].model.get(selectedIndexes[i], "typeRole")
            if(["Audio Track", "Video Track"].includes(type)) {
                let task = selectedIndexes[i].model.get(selectedIndexes[i], "nameRole")
                // make sure the task is valid..
                // let task_index = engine.search(task, "nameRole")
                // if(task_index.valid)
                autoConformSelectionTimeline(task, conformSourceIndex, selectedIndexes[i])
            }
        }
    }


    function conformPrepareSequence(sequenceIndex, onlyCreateConfrom=true, callback=null) {
        if(sequenceIndex.valid && sequenceIndex.model.get(sequenceIndex, "typeRole") == "Timeline") {
            Future.promise(
                engine.conformPrepareSequenceFuture(
                    sequenceIndex, onlyCreateConfrom
                )
            ).then(
                function(result) {
                    if(callback)
                        callback(sequenceIndex, result)
                },
                function() {
                }
            )
        }
    }

    XsMenuModelItem {
        text: "Conform"
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 100
        menuModelName: "media_list_menu_"
    }

    XsMenuModelItem {
        text: "Replace"
        // menuItemType: "button"
        menuPath: ""
        menuItemPosition: 101
        menuModelName: "media_list_menu_"
    }

    XsMenuModelItem {
        text: "Compare"
        // menuItemType: "button"
        menuPath: ""
        menuItemPosition: 102
        menuModelName: "media_list_menu_"
    }

    XsMenuModelItem {
        text: "Conform Selected"
        // menuItemType: "button"
        menuPath: ""
        menuItemPosition: 103
        menuModelName: "media_list_menu_"
        onActivated: (menuContext) =>  conformToNewSequence(menuContext.mediaSelection)
    }

    XsMenuModelItem {
        text: "Conform With Neighbours"
        // menuItemType: "button"
        menuPath: ""
        menuItemPosition: 104
        menuModelName: "media_list_menu_"
    }

    XsMenuModelItem {
        text: "Conform With Track"
        // menuItemType: "button"
        menuPath: ""
        menuItemPosition: 105
        menuModelName: "media_list_menu_"
    }

    XsMenuModelItem {
        text: "Conform To Current Sequence"
        // menuItemType: "button"
        menuPath: "More"
        menuItemPosition: 1
        menuModelName: "media_list_menu_"
        onActivated: (menuContext) =>  conformToSequence(menuContext.mediaSelection, viewportCurrentMediaContainerIndex)
    }


    XsMenuModelItem {
        text: "Auto-Conform"
        menuPath: ""
        menuItemPosition: 8
        menuModelName: "timeline_clip_menu_"
    }

    XsMenuModelItem {
        text: "Replace"
        menuPath: ""
        menuItemPosition: 8.5
        menuModelName: "timeline_clip_menu_"
    }

    XsMenuModelItem {
        text: "Auto-Conform"
        menuPath: ""
        menuItemPosition: 12
        menuModelName: "timeline_track_menu_"
    }

    XsMenuModelItem {
        text: "Auto-Conform From Track Name"
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 13
        menuModelName: "timeline_track_menu_"
        onActivated: (menuContext) =>  autoConformFromTrackName(
            menuContext.theTimeline.conformSourceIndex, menuContext.theTimeline.timelineSelection.selectedIndexes
        )
    }

    XsMenuModelItem {
        text: "Remove Duplicates"
        menuItemType: "button"
        menuPath: ""
        menuItemPosition: 14.6
        menuModelName: "timeline_track_menu_"
        onActivated: (menuContext) =>  removeDuplicates(
            menuContext.theTimeline.timelineSelection.selectedIndexes
        )
    }


    XsMenuModelItem {
        text: "Replace Clips"
        menuPath: ""
        menuItemPosition: 14
        menuModelName: "timeline_track_menu_"
    }

    XsMenuModelItem {
        text: "Conform To Selected Sequence"
        menuPath: ""
        menuItemPosition: 14.5
        menuModelName: "timeline_track_menu_"
        onActivated: (menuContext) =>  conformTracksToSequence(menuContext.theTimeline.timelineSelection.selectedIndexes, currentMediaContainerIndex)
    }

    XsMenuModelItem {
        menuItemType: "divider"
        menuPath: ""
        menuItemPosition: 50
        menuModelName: "timeline_menu_"
    }

    XsMenuModelItem {
        text: "Create Conform Track"
        // menuItemType: "button"
        menuPath: ""
        menuItemPosition: 51
        menuModelName: "timeline_menu_"
        onActivated: (menuContext) =>  conformPrepareSequence(menuContext.theTimeline.timelineModel.rootIndex.parent)
    }

	DelegateModel {
		id: replaceModel
		property var notifyModel: null
		model: notifyModel
		delegate :
			Item {
				XsMenuModelItem {
					text: nameRole
					menuItemType: "button"
					menuPath: "Replace"
					menuItemPosition: 34 + index
					menuModelName: "media_list_menu_"
			        onActivated: (menuContext) =>  replaceSelection(text, menuContext.mediaSelection)
                    hotkeyUuid: {
                        if(nameRole == "Next Version")
                            return next_version_hotkey.uuid
                        else if(nameRole == "Previous Version")
                            return previous_version_hotkey.uuid
                        else if(nameRole == "Latest Version")
                            return latest_version_hotkey.uuid
                        helpers.QVariantFromUuidString("")
                    }
				}
                XsMenuModelItem {
                    text: nameRole
                    menuItemType: "button"
                    menuPath: "Auto-Conform"
                    menuItemPosition: index
                    menuModelName: "timeline_track_menu_"
                    onActivated: (menuContext) =>  autoConformSelectionTimeline(text, menuContext.theTimeline.conformSourceIndex, menuContext.theTimeline.timelineSelection.selectedIndexes[0])
                }
                XsMenuModelItem {
                    text: nameRole
                    menuItemType: "button"
                    menuPath: "Replace Clips"
                    menuItemPosition: index
                    menuModelName: "timeline_track_menu_"
                    onActivated: (menuContext) =>  replaceSelectionTimeline(text, menuContext.theTimeline.timelineSelection.selectedIndexes)
                }

                XsMenuModelItem {
                    text: nameRole
                    menuItemType: "button"
                    menuPath: "Replace"
                    menuItemPosition: index
                    menuModelName: "timeline_clip_menu_"
                    onActivated: (menuContext) =>  replaceSelectionTimeline(text, menuContext.theTimeline.timelineSelection.selectedIndexes)
                }

                XsMenuModelItem {
                    text: nameRole
                    menuItemType: "button"
                    menuPath: "Auto-Conform"
                    menuItemPosition: index
                    menuModelName: "timeline_clip_menu_"
                    onActivated: (menuContext) =>  conformSelectionTimeline(text, menuContext.theTimeline.timelineSelection.selectedIndexes)
                }

                // XsMenuModelItem {
                //     text: nameRole
                //     menuItemType: "button"
                //     menuPath: "Auto-Conform Around"
                //     menuItemPosition: index
                //     menuModelName: "timeline_clip_menu_"
                //     onActivated: (menuContext) =>  {
                //         let items = [].concat(menuContext.theTimeline.timelineSelection.selectedIndexes)
                //         let newitems = []
                //         for(let i = 0; i < items.length; i++) {
                //             let expanded = theSessionData.modifyItemSelectionHorizontal([items[i]], 1, 1)
                //             newitems = newitems.concat(
                //                 expanded.filter(value => !newitems.includes(value))
                //             )
                //         }

                //         conformSelectionTimeline(text, newitems)
                //     }
                // }

                XsMenuModelItem {
                    text: nameRole
                    menuItemType: "button"
                    menuPath: "Conform With Track"
                    menuItemPosition: index
                    menuModelName: "media_list_menu_"
                    onActivated: (menuContext) =>  conformWithToNewSequence(menuContext.mediaSelection, text)
                }
                XsMenuModelItem {
                    text: nameRole
                    menuItemType: "button"
                    menuPath: "Conform With Neighbours"
                    menuItemPosition: index
                    menuModelName: "media_list_menu_"
                    onActivated: (menuContext) =>  conformWithSiblingsToNewSequence(menuContext.mediaSelection, text)
                }
			}
	}

	DelegateModel {
		id: compareModel
		property var notifyModel: null
		model: notifyModel
		delegate :
			Item {
				XsMenuModelItem {
					text: nameRole
					menuItemType: "button"
					menuPath: "Compare"
					menuItemPosition: index
					menuModelName: "media_list_menu_"
			        onActivated: (menuContext) =>  compareSelection(text, menuContext.mediaSelection)
				}
			}
	}

    Repeater {
		model: replaceModel
	}

    Repeater {
		model: compareModel
	}
}
