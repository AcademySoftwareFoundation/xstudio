import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0

TestSuite {
    name : "PromiseJS_Resolve_Qt_Binding"

    Item {
        id: item
        property int value1 : 0
        property int value2 : 0
    }

    function test_resolve_qt_binding() {
        var binding = Qt.binding(function() {
           return item.value1;
        });
        var exceptionOccurred = false;

        item.value2 = binding;
        item.value1 = 13;
        compare(item.value2,13);

        item.value1 = 0;

        var promise = Q.promise();

        // Don't pass the result of Qt.binding() to resolve().
        // It will throw exception.
        try {
            promise.resolve(binding);
        } catch (e) {
            exceptionOccurred = true;
        }

        compare(exceptionOccurred,true);
    }

}
