
// Below operates mainly on Qt.point type but should also be
// compatible with Qt.vector2d to some extend

function toVector2d(v1) {
    return Qt.vector2d(v1.x, v1.y);
}

function addVec(v1, v2) {
    // Accounts for Qt top -> down Y axis
    return Qt.point(v1.x + v2.x, v1.y - v2.y);
}

function subtractVec(v1, v2) {
    // Accounts for Qt top -> down Y axis
    return Qt.point(v1.x - v2.x, v2.y - v1.y);
}

function scaleVec(v1, s) {
    return Qt.point(v1.x * s, v1.y * s);
}

function dotProduct(v1, v2) {
    return v1.x * v2.x + v1.y * v2.y;
}

function crossProduct(v1, v2) {
    return v1.x * v2.y - v2.x * v1.y;
}

function normVec(v) {
    var norm = lengthVec(v);
    return Qt.point(v.x / norm, v.y / norm);
}

function lengthVec(v) {
    return Math.sqrt(v.x*v.x + v.y*v.y);
}

function checkConvex(pt, prevprev, prev, next, nextnext) {
    // Check point crossing prev (clockwise) side (extended)
    var ptOrig = subtractVec(pt, prev);
    var sideDir = normVec(subtractVec(prev, prevprev));
    var cross = crossProduct(normVec(ptOrig), sideDir);

    if (cross < 0) {
        var t = dotProduct(ptOrig, sideDir);
        pt.x = prev.x + sideDir.x * t;
        pt.y = prev.y - sideDir.y * t;
    }

    // Check point crossing next (clockwise) side (extended)
    var ptOrig = subtractVec(pt, next);
    var sideDir = normVec(subtractVec(next, nextnext));
    var cross = crossProduct(normVec(ptOrig), sideDir);

    if (cross >= 0) {
        var t = dotProduct(ptOrig, sideDir);
        pt.x = next.x + sideDir.x * t;
        pt.y = next.y - sideDir.y * t;
    }

    // Check point crossing opposite triangle boundary
    var ptOrig = subtractVec(pt, next);
    var sideDir = normVec(subtractVec(prev, next));
    var cross = crossProduct(normVec(ptOrig), sideDir);

    if (cross < 0) {
        var t = dotProduct(ptOrig, sideDir);
        pt.x = next.x + sideDir.x * t;
        pt.y = next.y - sideDir.y * t;
    }

    return pt;
}

function extendSides(pt, prevprev, prev, curr, next, nextnext, delta) {
    // Compute prev point position
    var prevDir = normVec(subtractVec(prev, prevprev));
    var tOrig = dotProduct(prevDir, subtractVec(curr, prevprev));
    var tNew = dotProduct(prevDir, subtractVec(pt, prevprev));
    var lengthOrig = lengthVec(subtractVec(prev, prevprev));
    var t = lengthOrig + (tNew - tOrig);

    // Prevent collapse of prev and prevprev points position
    if (t <= delta) {
        t = delta;
        var sideDir = normVec(subtractVec(curr, prev));
        var t2 = dotProduct(sideDir, subtractVec(pt, prevprev));
        pt.x = prevprev.x + prevDir.x * t + sideDir.x * t2;
        pt.y = prevprev.y - prevDir.y * t - sideDir.y * t2;
    }

    var newPrev = Qt.point(
        prevprev.x + prevDir.x * t,
        prevprev.y - prevDir.y * t
    );

    // Compute next point position
    var nextDir = normVec(subtractVec(next, prevprev));
    var tOrig = dotProduct(nextDir, subtractVec(curr, prevprev));
    var tNew = dotProduct(nextDir, subtractVec(pt, prevprev));
    var lengthOrig = lengthVec(subtractVec(next, prevprev));
    var t = lengthOrig + (tNew - tOrig);

    // Prevent collapse of next and prevprev points position
    if (t <= delta) {
        t = delta;
        var sideDir = normVec(subtractVec(curr, next));
        var t2 = dotProduct(sideDir, subtractVec(pt, prevprev));
        pt.x = prevprev.x + prevDir.x * t + sideDir.x * t2;
        pt.y = prevprev.y - prevDir.y * t - sideDir.y * t2;
    }

    var newNext = Qt.point(
        prevprev.x + nextDir.x * t,
        prevprev.y - nextDir.y * t
    );

    return [newPrev, newNext];
}

function centerOfPoints(points) {
    var mean_x = 0.0;
    var mean_y = 0.0;
    for (var i = 0; i < points.length; i++) {
        mean_x += points[i].x;
        mean_y += points[i].y;
    }
    mean_x /= points.length;
    mean_y /= points.length;
    return Qt.point(mean_x, mean_y);
}

function rotateVec(v, origin, angle) {
    // TODO: Could do this manually instead of using Qt.matrix
    var m = Qt.matrix4x4();
    m.rotate(angle, Qt.vector3d(0,0,1));

    var v3d = toVector2d(v).toVector3d();
    var o3d = toVector2d(origin).toVector3d();

    v3d = v3d.minus(o3d);
    v3d = m.times(v3d);
    v3d = v3d.plus(o3d);

    return Qt.point(v3d.x, v3d.y);
}

// From https://stackoverflow.com/a/1501725
function sqr(x) { return x * x }
function dist2(a, b) { return sqr(a.x - b.x) + sqr(a.y - b.y) }
function distToSegment2(a, b, pt) {
    var l2 = dist2(a, b);
    // Distance to point
    if (l2 == 0) return dist2(pt, a);
    // Project point onto the line segment (dot product(pt - a, b - a)), normalize by segment length
    var t = ((pt.x - a.x) * (b.x - a.x) + (pt.y - a.y) * (b.y - a.y)) / l2;
    // Ignore projections outside the segment
    t = Math.max(0, Math.min(1, t));
    // Distance from point to projection on segment (v + t (b - a))
    return dist2(pt, { x: a.x + t * (b.x - a.x), y: a.y + t * (b.y - a.y) });
}
