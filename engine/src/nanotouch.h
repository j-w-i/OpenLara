#ifndef H_NANOTOUCH
#define H_NANOTOUCH

// Touch HUD and contextual controls for single-touch panels (iPod nano 7).
//
// The panel reports ONE contact, so the scheme is built around that:
//  - left side: a floating move stick (origin = where the thumb lands): run, turn, back. It never jumps;
//    jumping comes only from the jump button (tap it right after lifting the stick for a running jump).
//  - right side: round buttons. Their set and icons change with Lara's real state (see
//    refresh()): ledge/wall climb, hang, block push, swim, surface, switch/pickup/key,
//    weapons when an enemy is near. Toggles (walk, auto-fire, swim) and auto-held actions
//    (grab while jumping, hold while hanging) stand in for the second finger.
//  - a button tapped right after the stick was lifted keeps the stick direction for a moment
//    ("sticky stick"), which gives running jumps / dives.
// Everything is drawn with solid quads in the game's own UI pass (UI::begin .. UI::end).
// nanotouch.h is included at the end of game.h; level.h only forward-declares render().

namespace NanoTouch {

    enum Act {
        A_NONE, A_JUMP, A_ACTION, A_ROLL, A_WALK, A_WEAPON, A_FIRE, A_CLIMB, A_PULLUP, A_DROP,
        A_SWIM, A_DIVE, A_SURFACE, A_OUT, A_LOOK, A_INV, A_OK, A_BACK, A_UP, A_DOWN, A_LEFT, A_RIGHT, A_MAX
    };

    enum Icon {
        I_NONE, I_JUMP, I_HAND, I_LEVER, I_TAKE, I_KEY, I_PUSH, I_ROLL, I_WALK, I_GUN, I_FIRE, I_CLIMB,
        I_PULLUP, I_DROP, I_SWIM, I_DIVE, I_SURFACE, I_EYE, I_MENU, I_OK, I_BACK, I_UP, I_DOWN, I_LEFT, I_RIGHT
    };

    enum Near { N_NONE, N_SWITCH, N_PICKUP, N_KEY, N_BLOCK };
    enum Mode { M_MENU, M_GROUND, M_AIR, M_HANG, M_BLOCK, M_UNDER, M_SURF, M_DEAD };
    enum Slot { S_JUMP, S_ACT, S_WPN, S_C, S_D, S_LOOK, S_INV, S_MAX };

    struct Btn {
        uint8 act, icon, slot;
        float x, y, r;
        const char *label;
        bool  hot, on, dim;
    };

    #define NT_SLOTS 5
    struct Sample { int down, pressed, released, x, y; };   // one finger (SDK nano_touch_t slot)

    #define NT_PHYS_JUMP  1u
    #define NT_PHYS_ACT   2u

    static const float STICK_R  = 46.0f;    // px
    static const float STICK_X  = 200.0f;   // stick may start left of this
    static const float STICK_TOP = 44.0f;   // and below this

    static Btn    btns[14];
    static int    nb;
    static int    mode, nearWhat;
    static bool   enemyNear, canClimb, fight, updated;
    static uint32 tick;

    static float  T;
    static int    own[NT_SLOTS];    // per finger: 0 none, 1 stick, 2 button, 3 dead touch
    static int    ownA[NT_SLOTS];   // the button's action while own == 2
    static int    stickSlot = -1, lookSlot = -1;
    static vec2   origin, stickPos, stick, lookPos, lookVec;
    static vec2   lastStick, latchVec;
    static float  lastStickT = -9.0f, latchUntil = -1.0f, suppressAuto = -1.0f;
    static float  until[A_MAX];
    static bool   walkOn, fireOn, swimOn;

    static Lara *player() { return Game::level ? Game::level->player : NULL; }

    static bool inMenu() {
        return inventory && (inventory->active || inventory->isActive());
    }

    // ------------------------------------------------------------------ context probes
    static void probe(Lara *L) {
        nearWhat = N_NONE;
        enemyNear = canClimb = false;
        if (!L) return;

        fight = L->wpnReady() && !L->emptyHands();

        // enemy within reach: highlights the weapon button
        for (Controller *c = Controller::first; c; c = c->next) {
            if (!c->getEntity().isEnemy()) continue;
            Character *e = (Character*)c;
            if (!e->isActiveTarget()) continue;
            if ((c->pos - L->pos).length() < 6144.0f) { enemyNear = true; break; }
        }

        if (mode != M_GROUND) return;

        int room = L->getRoomIndex();
        TR::Level &lv = *L->level;

        // switch / key hole: Lara stands on the trigger cell of one (same data checkTrigger uses)
        TR::Level::FloorInfo info;
        L->getFloorInfo(room, L->pos, info);
        if (info.trigCmdCount && L->state == Lara::STATE_STOP) {
            if (info.trigger == TR::Level::Trigger::SWITCH) {
                Controller *c = (Controller*)lv.entities[info.trigCmd[0].args].controller;
                if (c && c->flags.state != TR::Entity::asActive) nearWhat = N_SWITCH;
            } else if (info.trigger == TR::Level::Trigger::KEY) {
                Controller *c = (Controller*)lv.entities[info.trigCmd[0].args].controller;
                if (c && c->flags.state == TR::Entity::asNone) nearWhat = N_KEY;
            }
        }

        vec3 dir = L->getDir();
        dir.y = 0.0f;
        dir = dir.normal();

        for (int i = 0; i < lv.entitiesCount && nearWhat == N_NONE; i++) {
            TR::Entity &e = lv.entities[i];
            if (!e.controller) continue;
            bool pick = e.isPickup(), block = e.isBlock();
            if (!pick && !block) continue;
            Controller *c = (Controller*)e.controller;
            if (pick && (c->getRoomIndex() != room || c->flags.invisible)) continue;
            vec3 d = c->pos - L->pos;
            if (fabsf(d.y) > 900.0f) continue;
            float dx = d.x, dz = d.z, l2 = dx * dx + dz * dz;
            if (pick) {
                if (l2 < 1000.0f * 1000.0f) nearWhat = N_PICKUP;
            } else if (l2 < 1400.0f * 1400.0f && l2 > 1.0f) {
                float inv = 1.0f / sqrtf(l2);
                if ((dx * dir.x + dz * dir.z) * inv > 0.7f) nearWhat = N_BLOCK;
            }
        }

        // wall / ledge in front that FORTH+ACTION can climb (checkClimb's own conditions)
        if (L->state == Lara::STATE_STOP && L->emptyHands()) {
            vec3 p = L->pos;
            Collision cl(L, room, p, vec3(0.0f), vec3(0.0f), LARA_RADIUS * 1.1f, L->angle.y, 0, LARA_HEIGHT, 256 + 128, 0xFFFFFF);
            if (cl.side == Collision::FRONT) {
                float h = L->pos.y - cl.info[Collision::FRONT].floor;
                float clear = cl.info[Collision::FRONT].floor - cl.info[Collision::FRONT].ceiling;
                canClimb = h >= 256.0f && h <= 7 * 256 + 128 && (clear >= LARA_HEIGHT || h > 512.0f);
            }
        }
    }

    static bool isJumpState(int s) {
        return s == Lara::STATE_FORWARD_JUMP || s == Lara::STATE_UP_JUMP || s == Lara::STATE_LEFT_JUMP ||
               s == Lara::STATE_RIGHT_JUMP || s == Lara::STATE_BACK_JUMP || s == Lara::STATE_REACH;
    }

    // ------------------------------------------------------------------ layout
    static void add(int slot, int act, int icon, const char *label, bool hot, bool on, bool dim) {
        static const float pos[S_MAX][3] = {
            { 386, 170, 34 },   // jump / primary
            { 316, 196, 30 },   // action
            { 392,  98, 25 },   // weapon
            { 316, 126, 26 },   // context C
            { 246, 200, 23 },   // context D
            { 194,  22, 17 },   // look
            { 240,  22, 17 },   // inventory
        };
        Btn &b = btns[nb++];
        b.act = (uint8)act; b.icon = (uint8)icon; b.slot = (uint8)slot;
        b.x = pos[slot][0]; b.y = pos[slot][1]; b.r = pos[slot][2];
        b.label = label; b.hot = hot; b.on = on; b.dim = dim;
    }

    static void layout(Lara *L) {
        nb = 0;
        if (mode == M_MENU) {
            static const float dp[4][2] = { { 80, 126 }, { 80, 214 }, { 36, 170 }, { 124, 170 } };
            static const uint8 da[4] = { A_UP, A_DOWN, A_LEFT, A_RIGHT }, di[4] = { I_UP, I_DOWN, I_LEFT, I_RIGHT };
            for (int i = 0; i < 4; i++) {
                Btn &b = btns[nb++];
                b.act = da[i]; b.icon = di[i]; b.slot = S_MAX; b.x = dp[i][0]; b.y = dp[i][1]; b.r = 24; b.label = NULL; b.hot = b.on = b.dim = false;
            }
            add(S_JUMP, A_OK,   I_OK,   "OK",   false, false, false);
            add(S_ACT,  A_BACK, I_BACK, "BACK", false, false, false);
            return;
        }
        if (mode == M_DEAD) return;

        bool hasWpn = L && L->wpnCurrent != TR::Entity::NONE;
        bool wpnSlot = hasWpn && (mode == M_GROUND || mode == M_AIR);

        switch (mode) {
            case M_GROUND : {
                add(S_JUMP, A_JUMP, I_JUMP, NULL, false, false, false);
                if (fight)
                    add(S_ACT, A_FIRE, I_FIRE, "FIRE", false, fireOn, false);
                else switch (nearWhat) {
                    case N_SWITCH : add(S_ACT, A_ACTION, I_LEVER, "USE",   true,  false, false); break;
                    case N_KEY    : add(S_ACT, A_ACTION, I_KEY,   "USE",   true,  false, false); break;
                    case N_PICKUP : add(S_ACT, A_ACTION, I_TAKE,  "TAKE",  true,  false, false); break;
                    case N_BLOCK  : add(S_ACT, A_ACTION, I_PUSH,  "PUSH",  true,  false, false); break;
                    default       : add(S_ACT, A_ACTION, I_HAND,  NULL,    false, false, false);
                }
                if (canClimb) add(S_C, A_CLIMB, I_CLIMB, "CLIMB", true, false, false);
                else          add(S_C, A_ROLL,  I_ROLL,  NULL,    false, false, false);
                add(S_D, A_WALK, I_WALK, NULL, false, walkOn, false);
                break;
            }
            case M_AIR :
                add(S_JUMP, A_JUMP, I_JUMP, NULL, false, false, true);
                if (fight) add(S_ACT, A_FIRE, I_FIRE, "FIRE", false, fireOn, false);
                else       add(S_ACT, A_ACTION, I_HAND, "GRAB", false, L && L->emptyHands(), false);
                break;
            case M_HANG :
                add(S_JUMP, A_PULLUP, I_PULLUP, "PULL UP", true, false, false);
                add(S_ACT,  A_DROP,   I_DROP,   "DROP",    false, false, false);
                break;
            case M_BLOCK :
                add(S_ACT, A_DROP, I_DROP, "LET GO", false, false, false);
                break;
            case M_UNDER :
                add(S_JUMP, A_SWIM, I_SWIM, "SWIM", false, swimOn, false);
                switch (nearWhat) {
                    default : add(S_ACT, A_ACTION, I_HAND, NULL, false, false, false);
                }
                add(S_C, A_SURFACE, I_SURFACE, "UP",   false, false, false);
                add(S_D, A_DIVE,    I_DIVE,    "DOWN", false, false, false);
                break;
            case M_SURF :
                add(S_JUMP, A_DIVE, I_DIVE, "DIVE", false, false, false);
                add(S_ACT,  A_OUT,  I_PULLUP, "CLIMB OUT", false, false, false);
                break;
        }

        if (wpnSlot)
            add(S_WPN, A_WEAPON, I_GUN, fight ? "HOLSTER" : (enemyNear ? "DRAW" : NULL),
                (enemyNear && !fight) || (fight && nearWhat != N_NONE), fight, false);

        if (mode != M_HANG && mode != M_BLOCK) {
            add(S_LOOK, A_LOOK, I_EYE, NULL, false, false, false);
        }
        add(S_INV, A_INV, I_MENU, NULL, false, false, false);
    }

    static void refresh() {
        Lara *L = player();
        int old = mode;
        if (inMenu()) mode = M_MENU;
        else if (!L || L->health <= 0.0f) mode = M_DEAD;
        else if (L->stand == Character::STAND_UNDERWATER) mode = M_UNDER;
        else if (L->stand == Character::STAND_ONWATER) mode = M_SURF;
        else if (L->stand == Character::STAND_HANG || L->state == Lara::STATE_HANG || L->state == Lara::STATE_HANG_LEFT || L->state == Lara::STATE_HANG_RIGHT || L->state == Lara::STATE_HANG_UP) mode = M_HANG;
        else if (L->state == Lara::STATE_PUSH_PULL_READY || L->state == Lara::STATE_PUSH_BLOCK || L->state == Lara::STATE_PULL_BLOCK) mode = M_BLOCK;
        else if (L->stand == Character::STAND_AIR) mode = M_AIR;
        else mode = M_GROUND;

        if (mode != old) { // toggles only live in the context that made them
            if (mode != M_UNDER) swimOn = false;
            if (mode != M_GROUND && mode != M_AIR) fireOn = false;
        }

        if ((tick++ % 5u) == 0u || mode != old)
            probe(mode == M_MENU || mode == M_DEAD ? NULL : L);
        if (mode == M_MENU || mode == M_DEAD) { nearWhat = N_NONE; enemyNear = canClimb = false; }
        #ifdef NT_FORCE_MODE   // host_ref previews of the other contexts
            mode = NT_FORCE_MODE; nearWhat = NT_FORCE_NEAR; fight = NT_FORCE_FIGHT; canClimb = NT_FORCE_CLIMB; enemyNear = NT_FORCE_ENEMY;
        #endif
        if (!fight) fireOn = false;
        layout(L);
    }

    // ------------------------------------------------------------------ input
    static bool held(int act) {
        for (int i = 0; i < NT_SLOTS; i++) if (own[i] == 2 && ownA[i] == act) return true;
        return false;
    }
    static bool active(int act) { return until[act] > T || held(act); }

    static int slotAct(int slot) {
        for (int i = 0; i < nb; i++) if (btns[i].slot == slot) return btns[i].act;
        return A_NONE;
    }

    static void press(int act) {
        float dur = 0.0f;
        bool  latch = false;
        switch (act) {
            case A_WALK   : walkOn = !walkOn; return;
            case A_FIRE   : fireOn = !fireOn; dur = 0.3f; break;
            case A_SWIM   : swimOn = !swimOn; latch = true; break;
            case A_DROP   : suppressAuto = T + 0.8f; return;
            case A_JUMP   : dur = 0.25f; latch = true; break;
            case A_ACTION : dur = 0.5f; break;
            case A_CLIMB  :
            case A_PULLUP :
            case A_OUT    : dur = 0.9f; break;
            case A_DIVE   : dur = 0.5f; latch = true; break;
            case A_SURFACE: dur = 0.5f; break;
            case A_ROLL   :
            case A_WEAPON :
            case A_INV    : dur = 0.15f; break;
            case A_LOOK   : return;
            default       : dur = 0.12f;
        }
        if (dur > 0.0f) until[act] = T + dur;
        if (latch && T - lastStickT < 0.35f && lastStick.length() > 0.5f) {
            latchUntil = T + 0.7f;
            latchVec   = lastStick;
        }
    }

    static void update(const Sample *fs, int nfs, uint32 phys) {
        float dt = Core::deltaTime;
        if (dt > 0.1f) dt = 0.1f;
        if (dt < 0.0f) dt = 0.0f;
        T += dt;
        updated = true;
        refresh();
        Lara *L = player();

        for (int k = 0; k < NT_SLOTS && k < nfs; k++) {
            const Sample &s = fs[k];
            if (s.pressed) {
                own[k] = 3;
                int hit = -1;
                float best = 1e9f;
                for (int i = 0; i < nb; i++) {
                    float dx = s.x - btns[i].x, dy = s.y - btns[i].y, d = sqrtf(dx * dx + dy * dy);
                    if (d < btns[i].r + 8.0f && d < best) { best = d; hit = i; }
                }
                if (hit >= 0) {
                    own[k] = 2;
                    ownA[k] = btns[hit].act;
                    press(ownA[k]);
                    if (ownA[k] == A_LOOK && lookSlot < 0) { lookSlot = k; lookPos = vec2((float)s.x, (float)s.y); lookVec = vec2(0.0f); }
                } else if (mode != M_MENU && mode != M_DEAD && s.x < STICK_X && s.y > STICK_TOP && stickSlot < 0) {
                    own[k] = 1;
                    stickSlot = k;
                    float ox = (float)s.x, oy = (float)s.y;
                    if (ox < STICK_R + 6) ox = STICK_R + 6;
                    if (oy > 240 - STICK_R - 6) oy = 240 - STICK_R - 6;
                    origin = vec2(ox, oy);
                }
            }

            if (own[k] == 1 && s.down) {
                vec2 d = vec2((float)s.x, (float)s.y) - origin;
                float len = d.length();
                stickPos = origin + (len > STICK_R ? d * (STICK_R / len) : d);
                vec2 v = len > 0.001f ? d * (1.0f / len) : vec2(0.0f);
                float mag = len / STICK_R;
                if (mag > 1.0f) mag = 1.0f;
                mag = mag < 0.15f ? 0.0f : (mag - 0.15f) / 0.85f;
                if (fabsf(v.x) < 0.30f && fabsf(v.y) > 0.7f) v = vec2(0.0f, v.y > 0 ? 1.0f : -1.0f); // straight lines stay straight
                stick = v * mag;
                if (len > STICK_R * 2.3f)                       // drag the base along instead of losing it
                    origin = vec2((float)s.x, (float)s.y) - v * (STICK_R * 2.3f);
            } else if (own[k] == 2 && ownA[k] == A_LOOK && s.down && lookSlot == k) {
                vec2 d = (vec2((float)s.x, (float)s.y) - lookPos) * (1.0f / 60.0f);
                float l = d.length();
                lookVec = l > 1.0f ? d * (1.0f / l) : d;
            }

            if (s.released || (!s.down && !s.pressed && own[k] != 0)) {
                if (own[k] == 1) { lastStick = stick; lastStickT = T; stick = vec2(0.0f); stickSlot = -1; }
                if (lookSlot == k) { lookSlot = -1; lookVec = vec2(0.0f); }
                own[k] = 0;
            }
        }

        if (phys & NT_PHYS_JUMP) press(slotAct(S_JUMP));
        if (phys & NT_PHYS_ACT)  press(slotAct(S_ACT));

        // ---- compose Lara's keys
        bool kJump = false, kAct = false, kForth = false, kBack = false, kWalk = false;
        bool kWpn = active(A_WEAPON), kRoll = active(A_ROLL), kInv = active(A_INV), kLook = held(A_LOOK);

        kJump = active(A_JUMP) || (swimOn && mode == M_UNDER);
        kAct  = active(A_ACTION);

        if (active(A_CLIMB) || active(A_PULLUP) || active(A_OUT)) { kAct = true; kForth = true; }
        if (active(A_DIVE)) { kForth = true; if (mode == M_SURF) kJump = true; }
        if (active(A_SURFACE)) kBack = true;

        if (active(A_FIRE) || (fireOn && L && L->arms[0].target)) kAct = true;

        if (L && L->emptyHands() && T > suppressAuto) {      // stand-ins for "hold action with the other thumb"
            if (mode == M_HANG || mode == M_BLOCK) kAct = true;
            else if (mode == M_AIR && isJumpState(L->state)) kAct = true;
        }
        kWalk = walkOn && mode == M_GROUND;

        vec2 L2 = vec2(0.0f);
        if (mode != M_MENU) {
            if (stickSlot >= 0) L2 = stick;
            else if (latchUntil > T) L2 = latchVec;
        }

        int ji = Core::settings.controls[0].joyIndex;
        Input::Joystick &J = Input::joy[ji];
        J.L = L2;
        J.R = lookVec;
        if (mode == M_MENU) {
            Input::setJoyDown(ji, jkA,     active(A_OK));
            Input::setJoyDown(ji, jkB,     active(A_BACK));
            Input::setJoyDown(ji, jkUp,    active(A_UP));
            Input::setJoyDown(ji, jkDown,  active(A_DOWN));
            Input::setJoyDown(ji, jkLeft,  active(A_LEFT));
            Input::setJoyDown(ji, jkRight, active(A_RIGHT));
            Input::setJoyDown(ji, jkX, false); Input::setJoyDown(ji, jkRB, false);
            Input::setJoyDown(ji, jkY, false); Input::setJoyDown(ji, jkLB, false);
            Input::setJoyDown(ji, jkSelect, false);
        } else {
            Input::setJoyDown(ji, jkX,  kJump);
            Input::setJoyDown(ji, jkA,  kAct);
            Input::setJoyDown(ji, jkRB, kWalk);
            Input::setJoyDown(ji, jkY,  kWpn);
            Input::setJoyDown(ji, jkB,  kRoll);
            Input::setJoyDown(ji, jkLB, kLook);
            Input::setJoyDown(ji, jkSelect, kInv);
            Input::setJoyDown(ji, jkUp,   kForth);
            Input::setJoyDown(ji, jkDown, kBack);
            Input::setJoyDown(ji, jkLeft, false);
            Input::setJoyDown(ji, jkRight, false);
        }
    }

    // ------------------------------------------------------------------ drawing
    static MeshBuilder *M;
    static float K;

    static uint32 col(int r, int g, int b, float a) {
        int A = int(a * 255.0f);
        return (uint32(A) << 24) | (uint32(b * A / 255) << 16) | (uint32(g * A / 255) << 8) | uint32(r * A / 255);
    }
    static inline int P(float v) { return int(v * K + 0.5f); }

    static void quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, uint32 c) {
        M->addDynQuad(P(x0), P(y0), P(x1), P(y1), P(x2), P(y2), P(x3), P(y3), c);
    }
    static void rect(float x, float y, float w, float h, uint32 c) { quad(x, y, x + w, y, x + w, y + h, x, y + h, c); }
    static void tri(float ax, float ay, float bx, float by, float cx, float cy, uint32 c) { quad(ax, ay, bx, by, cx, cy, cx, cy, c); }
    static void line(float x0, float y0, float x1, float y1, float t, uint32 c) {
        float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
        if (l < 0.001f) return;
        float nx = -dy / l * t * 0.5f, ny = dx / l * t * 0.5f;
        quad(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, c);
    }

    static const float CS[9] = { 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f, 0.0f, 0.7071f, 1.0f };
    static const float SN[9] = { 0.0f, 0.7071f, 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f, 0.0f };

    // 8-gons: the HUD must stay cheap (every quad is 4 vertices through the GL2 vertex path)
    static void disc(float cx, float cy, float r, uint32 c) {
        r *= 1.04f;
        for (int i = 0; i < 8; i += 2)
            quad(cx, cy, cx + r * CS[i], cy + r * SN[i], cx + r * CS[i + 1], cy + r * SN[i + 1], cx + r * CS[i + 2], cy + r * SN[i + 2], c);
    }
    static void ring(float cx, float cy, float r, float t, uint32 c) {
        float ri = r - t;
        r *= 1.04f; ri *= 1.04f;
        for (int i = 0; i < 8; i++)
            quad(cx + r * CS[i], cy + r * SN[i], cx + r * CS[i + 1], cy + r * SN[i + 1], cx + ri * CS[i + 1], cy + ri * SN[i + 1], cx + ri * CS[i], cy + ri * SN[i], c);
    }
    static void wave(float x, float y, float s, float w, uint32 c) {   // zig-zag water line, w units wide
        float px = x - w * 0.5f * s, py = y;
        for (int i = 1; i <= 4; i++) {
            float nx = x + (-w * 0.5f + w * i / 4.0f) * s, ny = y + ((i & 1) ? -3.0f : 0.0f) * s;
            line(px, py, nx, ny, 2.6f * s, c);
            px = nx; py = ny;
        }
    }

    static void icon(int id, float cx, float cy, float s, uint32 c, uint32 c2) {
        #define X(v) (cx + (v) * s)
        #define Y(v) (cy + (v) * s)
        switch (id) {
            case I_JUMP :
                tri(X(0), Y(-17), X(-14), Y(-2), X(14), Y(-2), c);
                rect(X(-5), Y(-2), 10 * s, 16 * s, c);
                break;
            case I_HAND :
                rect(X(-9), Y(0), 18 * s, 13 * s, c);
                rect(X(-9), Y(-13), 4 * s, 13 * s, c);
                rect(X(-4.5f), Y(-16), 4 * s, 16 * s, c);
                rect(X(0), Y(-15), 4 * s, 15 * s, c);
                rect(X(4.5f), Y(-12), 4 * s, 12 * s, c);
                line(X(-9), Y(8), X(-15), Y(0), 4.5f * s, c);
                break;
            case I_LEVER :
                rect(X(-14), Y(8), 28 * s, 6 * s, c);
                line(X(0), Y(8), X(10), Y(-11), 5 * s, c);
                disc(X(10), Y(-13), 5.5f * s, c);
                break;
            case I_TAKE :
                quad(X(0), Y(-15), X(10), Y(-5), X(0), Y(5), X(-10), Y(-5), c);
                rect(X(-14), Y(9), 28 * s, 5 * s, c);
                break;
            case I_KEY :
                disc(X(-8), Y(0), 7.5f * s, c);
                disc(X(-8), Y(0), 3.0f * s, c2);
                rect(X(-2), Y(-2), 18 * s, 4 * s, c);
                rect(X(8), Y(2), 3 * s, 6 * s, c);
                rect(X(13), Y(2), 3 * s, 5 * s, c);
                break;
            case I_PUSH :
                rect(X(-15), Y(-9), 14 * s, 18 * s, c);
                rect(X(2), Y(-2), 10 * s, 4 * s, c);
                tri(X(11), Y(-9), X(11), Y(9), X(20), Y(0), c);
                break;
            case I_ROLL : {
                float pa = 0, pb = 0; bool first = true;
                for (int i = 0; i <= 5; i++) {
                    float a = (35.0f + 270.0f * i / 5.0f) * 0.0174533f, ex = cosf(a) * 12.0f, ey = sinf(a) * 12.0f;
                    if (!first) line(X(pa), Y(pb), X(ex), Y(ey), 4.2f * s, c);
                    pa = ex; pb = ey; first = false;
                }
                tri(X(pa - 7), Y(pb - 4), X(pa + 7), Y(pb - 4), X(pa), Y(pb + 8), c);
                break;
            }
            case I_WALK :
                rect(X(-11), Y(-1), 8 * s, 12 * s, c); disc(X(-7), Y(-4), 4.5f * s, c);
                rect(X(3), Y(-12), 8 * s, 12 * s, c);  disc(X(7), Y(-15), 4.5f * s, c);
                break;
            case I_GUN :
                rect(X(-16), Y(-9), 28 * s, 8 * s, c);
                quad(X(-5), Y(-1), X(5), Y(-1), X(8), Y(13), X(-2), Y(13), c);
                rect(X(12), Y(-8), 4 * s, 4 * s, c);
                break;
            case I_FIRE :
                ring(X(0), Y(0), 11 * s, 3 * s, c);
                line(X(0), Y(-17), X(0), Y(-6), 3 * s, c); line(X(0), Y(6), X(0), Y(17), 3 * s, c);
                line(X(-17), Y(0), X(-6), Y(0), 3 * s, c); line(X(6), Y(0), X(17), Y(0), 3 * s, c);
                disc(X(0), Y(0), 2.5f * s, c);
                break;
            case I_CLIMB :
                rect(X(2), Y(-15), 7 * s, 30 * s, c);
                rect(X(-14), Y(-15), 23 * s, 6 * s, c);
                tri(X(-8), Y(-6), X(-15), Y(6), X(-1), Y(6), c);
                rect(X(-11), Y(6), 6 * s, 9 * s, c);
                break;
            case I_PULLUP :
                tri(X(0), Y(-17), X(-13), Y(-3), X(13), Y(-3), c);
                rect(X(-4), Y(-3), 8 * s, 9 * s, c);
                rect(X(-15), Y(10), 30 * s, 5 * s, c);
                break;
            case I_DROP :
                tri(X(0), Y(17), X(-13), Y(3), X(13), Y(3), c);
                rect(X(-4), Y(-11), 8 * s, 14 * s, c);
                rect(X(-15), Y(-16), 30 * s, 5 * s, c);
                break;
            case I_SWIM :
                tri(X(19), Y(-4), X(7), Y(-14), X(7), Y(6), c);
                rect(X(-12), Y(-8), 20 * s, 8 * s, c);
                wave(X(0), Y(12), s, 30, c);
                break;
            case I_DIVE :
                wave(X(0), Y(-14), s, 30, c);
                tri(X(0), Y(17), X(-11), Y(3), X(11), Y(3), c);
                rect(X(-3.5f), Y(-7), 7 * s, 11 * s, c);
                break;
            case I_SURFACE :
                tri(X(0), Y(-18), X(-11), Y(-4), X(11), Y(-4), c);
                rect(X(-3.5f), Y(-4), 7 * s, 12 * s, c);
                wave(X(0), Y(14), s, 30, c);
                break;
            case I_EYE :
                quad(X(-16), Y(0), X(0), Y(-10), X(16), Y(0), X(0), Y(10), c);
                disc(X(0), Y(0), 5 * s, c2);
                break;
            case I_MENU :
                rect(X(-12), Y(-10), 24 * s, 4 * s, c); rect(X(-12), Y(-2), 24 * s, 4 * s, c); rect(X(-12), Y(6), 24 * s, 4 * s, c);
                break;
            case I_OK :
                line(X(-12), Y(0), X(-4), Y(9), 5 * s, c); line(X(-4), Y(9), X(13), Y(-10), 5 * s, c);
                break;
            case I_BACK :
                line(X(-10), Y(-10), X(10), Y(10), 5 * s, c); line(X(10), Y(-10), X(-10), Y(10), 5 * s, c);
                break;
            case I_UP    : tri(X(0), Y(-13), X(-13), Y(9), X(13), Y(9), c); break;
            case I_DOWN  : tri(X(0), Y(13), X(-13), Y(-9), X(13), Y(-9), c); break;
            case I_LEFT  : tri(X(-13), Y(0), X(9), Y(-13), X(9), Y(13), c); break;
            case I_RIGHT : tri(X(13), Y(0), X(-9), Y(-13), X(-9), Y(13), c); break;
        }
        #undef X
        #undef Y
    }

    static void label(float cx, float y, const char *text) {
        UI::textOut(vec2(cx * K - 120.0f, y * K), text, UI::aCenter, 240.0f, 255, UI::SHADE_ORANGE);
    }

    static void drawButton(const Btn &b) {
        bool pressed = held(b.act) || until[b.act] > T;
        float pulse = 0.5f + 0.5f * sinf(T * 8.0f);
        float a = b.dim ? 0.25f : (pressed ? 0.85f : 0.5f);

        if (b.hot && !pressed) ring(b.x, b.y, b.r + 4.0f, 3.0f, col(255, 190, 40, 0.35f + 0.5f * pulse));
        disc(b.x, b.y, b.r, pressed ? col(90, 100, 118, 0.88f) : col(14, 18, 24, a * 0.85f + 0.1f));
        if (b.on) disc(b.x, b.y, b.r * 0.9f, col(40, 200, 110, 0.35f));
        uint32 ic = b.dim ? col(235, 235, 235, 0.35f) : (b.hot ? col(255, 205, 80, 0.95f) : (b.act == A_FIRE ? col(255, 120, 100, 0.95f) : col(240, 240, 240, 0.9f)));
        icon(b.icon, b.x, b.y, b.r / 30.0f, ic, col(14, 18, 24, 0.9f));
    }

    // inventory = true when called from the inventory pass (menu mode), false from the game pass
    void render(bool inInventory) {
        #ifdef NT_NO_HUD   // A/B frame-time measurements
            return;
        #endif
        if (!updated) { Lara *L = player(); (void)L; refresh(); }   // benches never call update()
        if ((mode == M_MENU) != inInventory) return;
        if (mode == M_DEAD) return;

        M = UI::game->getMesh();
        K = UI::height / float(Core::height);

        if (mode != M_MENU) {   // stick: idle hint or live base + knob
            if (stickSlot >= 0) {
                ring(origin.x, origin.y, STICK_R, 3.0f, col(235, 235, 235, 0.35f));
                disc(stickPos.x, stickPos.y, 17.0f, col(235, 235, 235, 0.6f));
            } else {
                disc(84.0f, 170.0f, STICK_R, col(235, 235, 235, 0.10f));
                disc(84.0f, 170.0f, 14.0f, col(235, 235, 235, 0.18f));
            }
        }

        for (int i = 0; i < nb; i++) drawButton(btns[i]);

        for (int i = 0; i < nb; i++) {
            const Btn &b = btns[i];
            if (b.label && !b.dim) label(b.x, b.y + b.r + 9.0f, b.label);
        }

        const char *hint = NULL;
        if (mode == M_HANG) hint = "STICK LEFT RIGHT TO SHIMMY";
        else if (mode == M_BLOCK) hint = "STICK UP PUSH  DOWN PULL";
        else if (mode == M_UNDER) hint = "STICK STEERS";
        if (hint) label(130.0f, 224.0f, hint);
    }
}

#endif
