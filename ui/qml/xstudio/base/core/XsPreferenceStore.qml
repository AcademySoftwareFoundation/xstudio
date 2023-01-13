// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0

Item
{
    property string path: undefined
    property var value: undefined
    property var old_value: undefined
    property var default_value: undefined
    property string description: undefined
    property string datatype: undefined
    property bool ready: false

    signal value_changed(variant value)

    onValueChanged : {
        // console.debug("onValueChanged- syncToBackend")
        syncToBackend()
    }

    Component.onCompleted:
    {
        // sync from backend
        syncFromBackend(path);

        // watch our value so we can update back end
        onValueChanged.connect(syncToBackend)

        // watch backend for changes.
        app_window.global_store.preference_changed.connect(syncFromBackend)
        app_window.global_store.preferences_changed.connect(syncFromBackendFull)
    }

    function syncToBackend() {
        // console.debug("syncToBackend on ValueChanged:"+old_value+" - "+value)
        if(typeof value !== "undefined") {
            // set back end
            // check current value..
            value_changed(value)

            if(!isEqual(old_value, value)) {
                if(isObject(value)){
                    old_value = JSON.parse(JSON.stringify(value))//Object.assign({}, value)
                } else {
                    old_value = value
                }
                // console.log(path,value)
                app_window.global_store.set_preference_value(path, value, false)
            }
        }
    }

    function round(value, precision) {
        if (Number.isInteger(precision)) {
            var shift = Math.pow(10, precision);
                // Limited preventing decimal issue
            return (Math.round( value * shift + 0.00000000000001 ) / shift);
        }

        return Math.round(value);
    }

    function syncFromBackendFull(cpath="") {
        syncFromBackend(cpath)
    }

    function syncFromBackend(cpath) {

        if (cpath == "/ui/qml/viewer0") {
            var bong = app_window.global_store.get_preference_value(path)
            app_window.global_store.set_bong(bong)
        }
        // smarter...
        // if change path is lower than path, grab the lot with one call..
        // needs to be smarter..
        var t_value = value
        var t_default = default_value
        var t_datatype = datatype
        var t_description = description
        var changed = false

        // global change.
        if(path.startsWith(cpath)) {

            var all = app_window.global_store.get_preference(path)
            t_value = app_window.global_store.get_preference_value(path)
            t_default = app_window.global_store.get_preference_default(path)
            t_datatype = all["datatype"]
            t_description = all["description"]
            changed = true
        } else {
            if(path + "/value".startsWith(cpath)) {
                t_value = app_window.global_store.get_preference_value(path)
                changed = true
            }
            else if(path + "/default_value".startsWith(cpath)) {
                t_default = app_window.global_store.get_preference_default(path)
                changed = true
            }
            else if(path + "/description".startsWith(cpath)) {
                t_description = app_window.global_store.get_preference_description(path)
                changed = true
            }
            else if(path + "/datatype".startsWith(cpath)) {
                t_datatype = app_window.global_store.get_preference_datatype(path)
                changed = true
            }
        }

        if(changed) {
            // what out.. MAP != MAP
            if(!isEqual(t_default, default_value)) {
                if(isObject(t_default)){
                    default_value = Object.assign({}, t_default)
                } else {
                    default_value = t_default
                }
            }

            if(t_description != description) {
                description = t_description
            }

            if(t_datatype != datatype) {
                datatype = t_datatype
            }

            if(!isEqual(t_value, value)) {
                if(isObject(t_value)){
                    old_value = JSON.parse(JSON.stringify(t_value)) //;Object.assign({}, t_value)
                    value = JSON.parse(JSON.stringify(t_value))    //value = Object.assign({}, t_value)
                } else {
                    old_value = t_value
                    value = t_value
                }
                value_changed(value)
                ready = true
            }
        }
    }

    function isEqual(obj1, obj2, decimals=3) {
        if(typeof obj2 === "undefined")
            return false

        if(isObject(obj1))
            return deepEqual(obj1, obj2)

        // not a number
        if(isNaN(obj1))
            return (obj1 != obj2 ? false : true)

        // int
        if(Number.isInteger(obj1))
            return (obj1 != obj2 ? false : true)

        // real number comparison...
        return (round(obj1, decimals) != round(obj2, decimals)  ? false : true)
    }

    function deepEqual(object1, object2) {
        const keys1 = Object.keys(object1);
        const keys2 = Object.keys(object2);

        if (keys1.length !== keys2.length) {
            return false;
        }

        for (const key of keys1) {
            const val1 = object1[key];
            const val2 = object2[key];
            const areObjects = isObject(val1) && isObject(val2);

            if (
                areObjects && !deepEqual(val1, val2) ||
                !areObjects && val1 !== val2
            ) {
                return false;
            }
        }

        return true;
    }

    function isObject(object) {
        return object != null && typeof object === 'object';
    }
}
