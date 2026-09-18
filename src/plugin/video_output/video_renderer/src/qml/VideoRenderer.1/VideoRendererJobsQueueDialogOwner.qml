import QtQuick

import xStudio 1.0
import VideoRenderer 1.0
import xstudio.qml.models 1.0

Item {

    Loader {
        id: loader        
    }

    Component {
        id: render_queue_dlg
        VideoRendererJobsQueueDialog {
            title: "Video Render Queue"
            onVisibleChanged: {
                if (!visible) render_queue_visible = false
            }
        }
    }

    // access 'attribute' data exposed by our C++ plugin
    XsModuleData {
        id: video_render_attrs
        modelDataName: "video_render_attrs"
    }
    property alias video_render_attrs: video_render_attrs

    XsAttributeValue {
        id: __render_queue_visible
        attributeTitle: "render_queue_visible"
        model: video_render_attrs
    }
    property alias render_queue_visible: __render_queue_visible.value

    onRender_queue_visibleChanged: {
        if (render_queue_visible) {
            loader.sourceComponent = render_queue_dlg
            loader.item.visible = true
        } else {
            if (loader.item) loader.item.visible = false
        }
    }

}