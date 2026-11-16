// Scripted walk for the movement benchmark (LARA_WALK in src/main.cpp) and the ANGLE
// reference (tools/host_ref, HOST_WALK=1), so both replay the same path at a fixed 33 ms step.
// Call once per frame before Game::update(); returns false after the last frame.
#ifndef LARA_WALK_SCRIPT_H
#define LARA_WALK_SCRIPT_H

#define LARA_WALK_FRAMES 600

static bool lara_walk_input(int f) {
    // segments: [first frame, keys] - U up, L left, R right, D down (back), '-' nothing
    static const struct { int from; const char *keys; } seg[] = {
        {   0, "-"  },   // settle
        {  30, "U"  },   // run down the corridor into the big room
        { 190, "L"  },   // short turn left
        { 205, "U"  },   // run across the room
        { 320, "R"  },   // turn right
        { 340, "U"  },
        { 430, "L"  },   // turn round
        { 480, "U"  },   // run back
        { 600, "-"  },
    };
    if (f >= LARA_WALK_FRAMES) return false;
    const char *k = "-";
    for (unsigned i = 0; i < sizeof seg / sizeof seg[0]; ++i) if (f >= seg[i].from) k = seg[i].keys;
    bool u = false, l = false, r = false, d = false;
    for (const char *c = k; *c; ++c) { u |= *c == 'U'; l |= *c == 'L'; r |= *c == 'R'; d |= *c == 'D'; }
    Input::setJoyDown(0, jkUp, u);
    Input::setJoyDown(0, jkLeft, l);
    Input::setJoyDown(0, jkRight, r);
    Input::setJoyDown(0, jkDown, d);
    return true;
}

#endif
