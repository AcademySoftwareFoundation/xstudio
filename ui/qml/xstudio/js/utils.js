
// function getMethods(obj) {
//     let properties = new Set()
//     let currentObj = obj
//     do {
//         Object.getOwnPropertyNames(currentObj).map(item => properties.add(item))
//     } while ((currentObj = Object.getPrototypeOf(currentObj)))
//     return [...properties.keys()].filter(item => typeof obj[item] === 'function')
// }


// function getControlByKeyword(trays, keyword) {
//     //    var trays = [leftTray, rightTray, mediaToolsTray, transportTray, sessionSettingsTray];
//     var control = null;
//     for (var index in trays) {
//         control = trays[index].getControlByKeyword(keyword);
//         if (control !== null) {
//             return control;
//         }
//     }
//     return null;
// }

function cloneArray(list) {
    let items = []

    for(let i=0;i<list.length;++i)
        items[i] = list[i]

    return items
}

function getTimeCodeStr(displayValue)
{
    // e.g. minus 1 so that frame 30 for 30fps should still be 00:00
    // displayValue = displayValue
    if(playhead) {
        var fps = app_window.mediaImageSource.values.rateFPSRole ? app_window.mediaImageSource.values.rateFPSRole : 24.0
        var frames = displayValue % Math.round(fps)
        var seconds = Math.floor(displayValue / fps)
        var minutes = Math.floor(seconds / 60)
        var hours = Math.floor(minutes / 60)

        var FF = pad(frames, 2)
        var SS = pad(seconds % 60, 2)
        var MM = pad(minutes % 60, 2)
        var HH = pad(hours, 2)
        return HH + ':' + MM + ':' + SS + ':' + FF
    }
    return "--"
}

function getTimeStr(displayValue) {
    // e.g. minus 1 so that frame 30 for 30fps should still be 00:00
    if(playhead) {
        displayValue = displayValue - 1
        var fps = app_window.mediaImageSource.values.rateFPSRole ? app_window.mediaImageSource.values.rateFPSRole : 24.0
        var seconds = Math.floor(displayValue / fps)
        var minutes = Math.floor(seconds / 60)
        var hours = Math.floor(minutes / 60)

        var SS = pad(seconds % 60, 2)
        var MM = pad(minutes % 60, 2)
        var str
        if (hours > 0) {
            var HH = hours % 24
            str = HH + ':' + MM + ':' + SS
        } else {
            str = MM + ':' + SS
        }
        return str
    }
    return "--"
}

function pad(n, width, z) {
    z = z || '0';
    n = n + '';
    return n.length >= width ? n : new Array(width - n.length + 1).join(z) + n;
}

function replaceMenuText(txt)
{
    // there is a bug in Qt where a MenuItem delegate will
    // not correctly display the ampersand text; This is a
    // workaround.
    var index = txt.indexOf("&");
    if(index >= 0)
        txt = txt.replace(txt.substr(index, 2), ("<u>" + txt.substr(index + 1, 1) +"</u>"));
    return txt;
}

function getPlatformShortcut(txt) {
    //    console.log(Qt.platform.os)
    //if (Qt.platform.os === "linux" ||
    // Qt.platform.os === "windows" ||
    // Qt.platform.os === "osx" ||
    // Qt.platform.os === "unix") {
    if (Qt.platform.os === "osx") {
        txt = txt.replace(/Ctrl\+/g, "⌘");
        txt = txt.replace(/Alt\+/g, "⌥");
    }
    return txt;
}

function calcMax(arr){
    return Math.max(...arr);
}

function calcMin(arr){
    return Math.min(...arr);
}

function calcRange(arr){
    mx = calcMax(arr);
    mn = calcMin(arr);
    return mx-mn;
}

function nearest(val, inc) {
    return (Math.round(val / inc)+(val % inc ? 1:0)) * inc
}


function sumArr(arr){
    var a = arr.slice();
    return a.reduce(function(a, b) { return parseFloat(a) + parseFloat(b); });
}

function sortArr(arr){
    var ary = arr.slice();
    ary.sort(function(a,b){ return parseFloat(a) - parseFloat(b);});
    return ary;
}
// qml: [0,0,0,2,0,0,0,1,2,3,3,0] [7,1,2,2] 3

function calcPreferred(arr){
    return calcMax(arr)
}

function openDialogCenter(qml_path, parent=app_window) {
    return openDialog(qml_path, parent, {"anchors.centerIn": parent})
}

function openDialogPlaylist(qml_path, parent=app_window) {
    return openDialog(qml_path, parent, {"anchors.centerIn": parent})
}

function openDialog(qml_path, parent=app_window, params={}) {
    var dialog = null
    var component = Qt.createComponent(qml_path);
    if (component.status == Component.Ready) {
        dialog = component.createObject(parent, params)
    } else {
        console.log("Error loading component:", component.errorString());
    }
    return dialog
}

function centerYInParent(holder, parent, height) {
    var py = 0
    if(parent) {
        var oy = (parent.height/2)-(height/2);
        var py = mapToItem(holder,0,oy).y;
        if(py<0){
            py = mapFromItem(holder, 0, 0).y;
        } else if(py+height > holder.height){
            py = mapFromItem(holder, 0, holder.height-height).y;
        } else {
            py = oy;
        }
    }
    return py;
}

function centerXInParent(holder, parent, width) {
    var px = 0;

    if(parent) {
        var ox = (parent.width/2)-(width/2);
        var px = mapToItem(holder, ox,0).x;
        if(px<0){
            px = mapFromItem(holder, 0, 0).x;
        } else if(px+width > holder.width){
            px = mapFromItem(holder, holder.width-width, 0).x;
        } else {
            px = ox;
        }
    }
    return px;
}

//only top level
function getSelectedCuuids(obj, recursive=false, func=null) {
    var cuuids = []

    if(obj) {
        var model = obj.itemModel
        if(model) {
            for(var i=0;i<model.size();i++) {
                var cobj = model.get_object(i)

                if(cobj.selected && (!func ||func(cobj))) {
                    cuuids.push(cobj.cuuid)
                }

                if(recursive) {
                    var more_cuuids = getSelectedCuuids(cobj, recursive, func)
                    cuuids.push(...more_cuuids)
                }
            }
        }
    }
    return cuuids
}

function getSelectedUuids(obj, recursive=false, func=null) {
    var uuids = []

    if(obj) {
        var model = obj.itemModel
        if(model) {
            for(var i=0;i<model.size();i++) {
                var cobj = model.get_object(i)

                if(cobj.selected && (!func ||func(cobj))) {
                    uuids.push(cobj.uuid)
                }

                if(recursive) {
                    var more_uuids = getSelectedUuids(cobj, recursive, func)
                    uuids.push(...more_uuids)
                }
            }
        }
    }
    return uuids
}

function forAllItems(obj, parent, func, value) {
    var result = false;
    if(parent) {
        result = func(parent, obj, value)
    }
    var model = obj.itemModel
    if(model) {
        for(var i=0;i<model.size();i++) {
            if(forAllItems(model.get_object(i), obj, func, value)) {
                result = true
            }
        }
    }
    return result
}

function forFirstItem(obj, parent, func, value) {
    if(parent) {
        if(func(parent,obj,value)) {
            return true
        }
    }
    var model = obj.itemModel
    if(model) {
        for(var i=0;i<model.size();i++) {
            if(forFirstItem(model.get_object(i), obj, func, value)){
                return true
            }
        }
    }
    return false
}



function stem(str) {
    return (str.slice(0, str.lastIndexOf("/")))
}

function basename(str) {
    return (str.slice(str.lastIndexOf("/")+1))
}

function yyyymmdd(separator="", date=new Date()) {
    return String(date.getFullYear()) + separator + String(date.getMonth()+1).padStart(2, '0') + separator + String(date.getUTCDate()).padStart(2, '0')
}

