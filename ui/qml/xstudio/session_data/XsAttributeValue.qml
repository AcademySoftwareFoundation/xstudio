import xstudio.qml.helpers 1.0
import xstudio.qml.models 1.0

XsModelProperty {

    role: "value"
    property var model
    property string attributeTitle: ""
    index: model ? model.searchRecursive(attributeTitle, "title") : null
    property var modelLength: model ? model.length : 0
    signal indexBecameValid()

    onModelLengthChanged: {
        var was_valid = index.valid
        index = model.searchRecursive(attributeTitle, "title")
        if (!was_valid && index.valid) {
            indexBecameValid()
        }
    }

}