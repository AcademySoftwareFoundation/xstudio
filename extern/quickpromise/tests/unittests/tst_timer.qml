import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0

TestSuite {
    name : "Timer"

    function test_exception() {
        // It should print exception.
        Q.setTimeout(function() {
            global = 1;
        },0);
        wait(10);
    }

    function test_setTimeout() {
        var count1 = 0, count2 = 0;
        var id1 = Q.setTimeout(function(){
            count1++
        }, 0);
        var id2 = Q.setTimeout(function(){
            count2++;
        }, 0);
        compare(count1, 0);
        compare(count2, 0);
        compare(parseInt(id1), id1);
        compare(parseInt(id2), id2);
        compare(id1 !== id2, true);
        wait(0);
        compare(count1, 1);
        compare(count2, 1);
        wait(100);
        compare(count1, 1);
        compare(count2, 1);
    }

    function test_clearTimeout() {
        var count = 0;
        var id = Q.setTimeout(function(){
            count++
        }, 0);
        compare(count, 0);
        compare(id !== 0, true);
        Q.clearTimeout(id);
        wait(100);
        compare(count, 0);
        compare(Object.keys(Q._timers).length, 0);
    }

}
