import QtQuick 2.15
import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

Item {

    id: the_playhead

    property var uuid // set this to the UUID of the playhead

    XsModuleData {

        id: playhead_attrs_model
        // each playhead exposes its attributes in a model named after the
        // playhead UUID. We connect to the model like this:
        modelDataName: "" + uuid
    }

    XsAttributeValue {
        id: __playheadLogicalFrame
        attributeTitle: "Logical Frame"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadMediaFrame
        attributeTitle: "Media Frame"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadPositionSeconds
        attributeTitle: "Position Seconds"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadPositionFlicks
        attributeTitle: "Position Flicks"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadDurationFrames
        attributeTitle: "Duration Frames"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadDurationSeconds
        attributeTitle: "Duration Seconds"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadFrameRate
        attributeTitle: "Frame Rate"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadCachedFrames
        attributeTitle: "Cached Frames"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadBookmarkedFrames
        attributeTitle: "Bookmarked Frames"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadBookmarkedFrameColours
        role: "combo_box_options"
        attributeTitle: "Bookmarked Frames"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadLoopStartFrame
        attributeTitle: "Loop Start Frame"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadLoopEndFrame
        attributeTitle: "Loop End Frame"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadEnableLoopRange
        attributeTitle: "Enable Loop Range"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadPlaying
        attributeTitle: "playing"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadPlayingForward
        attributeTitle: "forward"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadFFWD
        attributeTitle: "Velocity Multiplier"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadLoopMode
        attributeTitle: "Loop Mode"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadSourceUuid
        attributeTitle: "Current Media Uuid"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadMediaSourceUuid
        attributeTitle: "Current Media Source Uuid"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadImageSourceName
        attributeTitle: "Source Name"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadCompareMode
        attributeTitle: "Compare"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadTimeCode
        attributeTitle: "Timecode"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadTimeCodeAsFrame
        attributeTitle: "Timecode As Frame"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadKeyPlayheadIndex
        attributeTitle: "Key Playhead Index"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadNumSubPlayheads
        attributeTitle: "Num Sub Playheads"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __playheadScrubbing
        attributeTitle: "User Is Frame Scrubbing"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __mediaTransitionFrames
        attributeTitle: "Media Transition Frames"
        model: playhead_attrs_model
    }        

    XsAttributeValue {
        id: __sourceOffsetFrames
        attributeTitle: "Source Offset Frames"
        model: playhead_attrs_model
    }            

    XsAttributeValue {
        id: __pinnedSourceMode
        attributeTitle: "Pinned Source Mode"
        model: playhead_attrs_model
    }            

        // access the value of the attribute called 'Compare' which is exposed in the
    // viewport _toolbar. 
    XsAttributeValue {
        id: __compare_mode
        attributeTitle: "Compare"
        model: playhead_attrs_model
    }

    XsAttributeValue {
        id: __timelineMode
        attributeTitle: "Timeline Mode"
        model: playhead_attrs_model
    }

    property alias logicalFrame: __playheadLogicalFrame.value
    property alias mediaFrame: __playheadMediaFrame.value
    property alias positionSeconds: __playheadPositionSeconds.value
    property alias positionFlicks:__playheadPositionFlicks.value
    property alias durationFrames: __playheadDurationFrames.value
    property alias durationSeconds: __playheadDurationSeconds.value
    property alias frameRate: __playheadFrameRate.value
    property alias playing: __playheadPlaying.value
    property alias cachedFrames: __playheadCachedFrames.value
    property alias bookmarkedFrames: __playheadBookmarkedFrames.value
    property alias bookmarkedFrameColours: __playheadBookmarkedFrameColours.value
    property alias loopStartFrame: __playheadLoopStartFrame.value
    property alias loopEndFrame: __playheadLoopEndFrame.value
    property alias enableLoopRange: __playheadEnableLoopRange.value
    property alias loopMode: __playheadLoopMode.value
    property alias velocityMultiplier: __playheadFFWD.value
    property alias playingForwards: __playheadPlayingForward.value
    property var mediaUuid: helpers.QVariantFromUuidString(__playheadSourceUuid.value)
    property var mediaSourceUuid: helpers.QVariantFromUuidString(__playheadMediaSourceUuid.value)
    property alias imageSourceName: __playheadImageSourceName.value
    property alias compareMode: __playheadCompareMode.value
    property alias timecode: __playheadTimeCode.value
    property alias timecodeAsFrame: __playheadTimeCodeAsFrame.value
    property alias keySubplayheadIndex: __playheadKeyPlayheadIndex.value
    property alias numSubPlayheads: __playheadNumSubPlayheads.value    
    property alias scrubbingFrames: __playheadScrubbing.value
    property alias mediaTransitionFrames: __mediaTransitionFrames.value
    property alias sourceOffsetFrames: __sourceOffsetFrames.value
    property alias pinnedSourceMode: __pinnedSourceMode.value
    property alias compare_mode: __compare_mode.value
    property alias timelineMode: __timelineMode.value

    /* This gives us a 'model' with one row - the row is the attribute data
    for the "Auto Align" attribute of the current playhead. We use it below
    to build the Auto Align button */
    XsFilterModel {
        id: auto_align_attr_data
        sourceModel: playhead_attrs_model
        sortAscending: true
        Component.onCompleted: {
            setRoleFilter("Auto Align", "title")
        }
    }
    property alias autoAlignAttrData: auto_align_attr_data


}