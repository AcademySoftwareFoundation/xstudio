var fs = require('fs');

var fileData = fs.readFileSync('../../qml/QuickPromise/promise.js',
                               'utf8');

var res = fileData.replace(/.pragma library/i,"")
                  .replace(/.import.*/i,"")
		  .replace(/.*createComponent.*/i,"")
		  .replace(/.*function setTimeout/i,"function __setTimeout")
                  .replace(/QP.QPTimer./g,"");

eval(res);

var adapter = {
    deferred: function() {
        var p = promise();
        return {
            promise: p,
            resolve: function(x) {p.resolve(x)},
            reject: function(x) {p.reject(x)}
        }
    },
    rejected: function(reason) {
        var p = promise();
        p.reject(reason);
        return p;
    },
    resolved: function(value) {
        var p = promise();
        p.resolve(value);
        return p;
    }
}

module.exports = adapter;
