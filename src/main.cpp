#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "nano_app.h"
#include "nano_heap.h"
#include "report.h"
#include "nano_flight.h"
extern "C" {
#include "gl2_egl.h"
}
#include "EGL/egl.h"
unsigned gl2std_error_site(void);
unsigned gl2std_error_aux(void);
extern uint32_t (*gl2std_clock)(void);
extern uint32_t gl2std_prof[8];
extern uint32_t gl2_prof[8];
extern "C" uint32_t gl2_delta_headers;
void gl2std_set_clock(uint32_t (*f)(void));
#include "GLES2/gl2.h"
int openlara_fs_init(void);
}
uint32_t gPhase[16];
// Build tag the NanoOS bundler/launcher keys on (also present in non-bench builds).
__attribute__((used)) static const char kBuildTag[] = "NANOOS GL2 GAME build " __DATE__ " " __TIME__;

#ifdef LARA_BENCH
static char g_report[12288];
static unsigned g_rused;
static void rtext(const char *s) { while (*s && g_rused + 1 < sizeof(g_report)) g_report[g_rused++] = *s++; }
static void rword(uint32_t x) {
    if (g_rused + 10 >= sizeof(g_report)) return;
    static const char h[] = "0123456789abcdef";
    for (unsigned i = 0; i < 8; ++i) g_report[g_rused++] = h[(x >> (28 - i * 4)) & 15u];
    g_report[g_rused++] = ' ';
}
static void rflush(void) {
    g_report[g_rused] = 0;
    rpt_mirror_write(g_report, g_rused, rpt_sum(g_report, g_rused));
    nano_app_dcache_clean();
}
extern "C" void lara_log(const char *fmt, ...) {
    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    rtext(tmp);
}
#else
extern "C" void lara_log(const char *fmt, ...) { (void)fmt; }
#endif

#include "game.h"
#ifdef LARA_WALK
#include "walk_script.h"
#endif

#ifdef LARA_FIXED_DT
// Deterministic frame time for graphics comparisons with tools/host_ref (33 ms per frame).
static uint32_t g_fixed_ms;
int osGetTimeMS() { return (int)g_fixed_ms; }
#else
int osGetTimeMS() {
    return (int)(nano_app_usec() / 1000u);
}
#endif

void osJoyVibrate(int index, float L, float R) {
    (void)index;
    (void)L;
    (void)R;
}

void *osMutexInit() { return NULL; }
void osMutexFree(void *obj) { (void)obj; }
void osMutexLock(void *obj) { (void)obj; }
void osMutexUnlock(void *obj) { (void)obj; }
void osCacheRead(Stream *stream) { if (stream) stream->callback(NULL, stream->userData); }
void osCacheWrite(Stream *stream) { if (stream) stream->callback(NULL, stream->userData); }
void osReadSlot(Stream *stream) { if (stream) stream->callback(NULL, stream->userData); }
void osWriteSlot(Stream *stream) { if (stream) stream->callback(NULL, stream->userData); }

extern "C" int nano_example_main(const struct nanoos_api *api) {
    if (nano_app_init(api) != 0) return 1;
    int mount_rc = openlara_fs_init();

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, NULL, NULL)) return 2;

    static const EGLint cfg_attr[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };
    EGLConfig cfg;
    EGLint ncfg = 0;
    if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &ncfg) || ncfg < 1) return 3;

    // Landscape by default for the playable build (the SDK rotates rendering and touch);
    // the bench stays portrait so it matches the existing reference frames.
#ifndef LARA_LANDSCAPE
#ifdef LARA_BENCH
#define LARA_LANDSCAPE 0
#else
#define LARA_LANDSCAPE 1
#endif
#endif
    nano_app_set_orientation(LARA_LANDSCAPE ? NANO_ORIENT_LANDSCAPE : NANO_ORIENT_PORTRAIT);
    EGLSurface sfc = eglCreateWindowSurface(dpy, cfg, (EGLNativeWindowType)0, NULL);
    if (sfc == EGL_NO_SURFACE) return 4;

    static const EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (ctx == EGL_NO_CONTEXT) return 5;
    if (!eglMakeCurrent(dpy, sfc, sfc, ctx)) return 6;

    Core::width = nano_app_screen_w();
    Core::height = nano_app_screen_h();

    contentDir[0] = 0;
    cacheDir[0] = 0;
    saveDir[0] = 0;

#ifdef LARA_BENCH
    {
        uint32_t hu0 = 0, hf0 = 0, hlg0 = 0, hnb0 = 0;
        nano_heap_stats(&hu0, &hf0, &hlg0, &hnb0);

        Core::settings.detail.shadows   = Core::Settings::LOW;
        Core::settings.detail.water     = Core::Settings::LOW;
        Core::settings.detail.lighting  = Core::Settings::LOW;
        Core::settings.detail.filter    = Core::Settings::LOW;
        Core::settings.detail.stereo    = Core::Settings::STEREO_OFF;

        gl2std_set_clock(nano_app_usec);
        uint32_t t0 = nano_app_usec();
        rtext("NANOOS GL2 GAME build " __DATE__ " " __TIME__ "\n");
        rflush();
        Game::init((char *)"DATA/LEVEL2.PHD");
        uint32_t t_init = nano_app_usec() - t0;
        uint32_t err_init = (uint32_t)glGetError();
        uint32_t site_init = gl2std_error_site();
        rflush();

        uint32_t hu1 = 0, hf1 = 0, hlg1 = 0, hnb1 = 0;
        nano_heap_stats(&hu1, &hf1, &hlg1, &hnb1);

        // Frame 0 carries every glReadPixels check (a read ends the frame synchronously,
        // which bypasses frame queuing), and is not timed. Frames 1..5 are timed with no
        // readbacks, so US/FR/UPD/REN/SWP/PH/PV/GP describe the queued steady state.
        uint32_t t_rnd0 = nano_app_usec();
        uint32_t fr[5] = {0,0,0,0,0}, cellDiff[5] = {0,0,0,0,0};
        uint32_t t_upd = 0, t_ren = 0, t_swp = 0;
        uint32_t px32 = 0, nonblack = 0, err_rnd = 0, site_rnd = 0, aux_rnd = 0;
#ifndef LARA_BENCH_FRAMES
#define LARA_BENCH_FRAMES 6
#endif
        for (int f = 0; f < LARA_BENCH_FRAMES && !Core::isQuit; ++f) {
            if (f == 1) { for (int i = 0; i < 8; ++i) gl2std_prof[i] = gl2_prof[i] = 0; for (int i = 0; i < 16; ++i) gPhase[i] = 0; t_upd = t_ren = t_swp = 0; t_rnd0 = nano_app_usec(); }
            if (nano_app_home_pressed()) break;
#ifdef LARA_FIXED_DT
            g_fixed_ms += 33;
#endif
            uint32_t ta = nano_app_usec();
            bool upd = Game::update();
            t_upd += nano_app_usec() - ta;
            if (upd) {
                (void)glGetError();
                                ta = nano_app_usec();
                Game::render();
                t_ren += nano_app_usec() - ta; if (f && f <= 5) fr[f - 1] = nano_app_usec() - ta;
#ifdef LARA_DEBUG_STATE
                { char st[128]; snprintf(st, sizeof st, "\nSTATE f%d dt %d title %d health %d dmg %d wpn %d empty %d", f, (int)(Core::deltaTime * 1000), (int)(inventory->titleTimer * 1000), (int)Game::level->player->health, (int)(Game::level->player->damageTime * 1000), (int)Game::level->player->wpnReady(), (int)Game::level->player->emptyHands()); rtext(st); }
#endif
                err_rnd = (uint32_t)glGetError();
                if (err_rnd) { site_rnd = gl2std_error_site(); aux_rnd = gl2std_error_aux(); }
                if (f == 0) {
                    uint32_t ref = 0, diff = 0; bool have = false;
                    for (int gy = 0; gy < 27; ++gy) for (int gx = 0; gx < 30; ++gx) {
                        uint8_t q[4] = {0,0,0,0}; glReadPixels(gx * 8 + 4, gy * 16 + 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, q);
                        uint32_t v = ((uint32_t)q[0] << 16) | ((uint32_t)q[1] << 8) | q[2];
                        if (!have) { ref = v; have = true; } else if (v != ref) ++diff;
                    }
                    cellDiff[f] = diff;
                }
                if (f == 0) {
                    uint8_t px[4] = {0, 0, 0, 0};
                    glReadPixels(120, 216, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
                    px32 = ((uint32_t)px[3] << 24) | ((uint32_t)px[0] << 16) | ((uint32_t)px[1] << 8) | px[2];
                    { GLint vp[4] = {0,0,0,0}, sc[4] = {0,0,0,0}; glGetIntegerv(GL_VIEWPORT, vp); glGetIntegerv(GL_SCISSOR_BOX, sc);
                      rtext("\nVP "); for (int i = 0; i < 4; ++i) rword((uint32_t)vp[i]); rtext("SC "); for (int i = 0; i < 4; ++i) rword((uint32_t)sc[i]); rtext("W/H "); rword((uint32_t)Core::width); rword((uint32_t)Core::height); }
                    // low-res luminance grid for visual verification (30 x 54, one hex digit each)
                    rtext("\nIMG\n");
                    for (int gy = 0; gy < 27; ++gy) {
                        for (int gx = 0; gx < 30; ++gx) {
                            uint8_t q[4] = {0, 0, 0, 0};
                            glReadPixels(gx * 8 + 4, gy * 16 + 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, q);
                            uint32_t l = (q[0] * 3 + q[1] * 6 + q[2]) / 10;
                            char c = "0123456789abcdef"[l >> 4];
                            if (g_rused + 2 < sizeof(g_report)) g_report[g_rused++] = c;
                        }
                        if (g_rused + 2 < sizeof(g_report)) g_report[g_rused++] = '\n';
                    }
                    for (int sy = 40; sy < 400; sy += 45) {
                        for (int sx = 20; sx < 220; sx += 50) {
                            glReadPixels(sx, sy, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
                            if (px[0] || px[1] || px[2]) ++nonblack;
                        }
                    }
                }
                ta = nano_app_usec();
                eglSwapBuffers(dpy, sfc);
                t_swp += nano_app_usec() - ta;
            }
        }
        uint32_t t_rnd = nano_app_usec() - t_rnd0;
        uint32_t glerr = err_rnd ? err_rnd : (uint32_t)glGetError();
#ifdef LARA_WALK
        // Movement benchmark: replay src/walk_script.h (fixed 33 ms game step) and time every
        // frame swap-to-swap. Per 30-frame segment: avg/max frame us, draws, tris, Lara's room.
        {
            static uint32_t ft[LARA_WALK_FRAMES];
            uint32_t n = 0, tprev = nano_app_usec(), segmax = 0, segsum = 0, segdips = 0, segtris = 0;
            uint32_t walk_err = 0, rec0 = gl2_egl_recoveries();
            rtext("\nWALKSEG frame avg_us max_us dips tris room x z");
            // profile: whole walk, and the heavy window (frames 450..480, ~288 draws)
            for (int i = 0; i < 8; ++i) gl2std_prof[i] = gl2_prof[i] = 0; for (int i = 0; i < 16; ++i) gPhase[i] = 0;
            { extern uint32_t gl2_prof2[8]; for (int i = 0; i < 8; ++i) gl2_prof2[i] = 0; }
            uint32_t w_upd = 0, w_ren = 0, w_swp = 0, snap[3][35];
            #define WSNAP(k) do { for (int i = 0; i < 8; ++i) { snap[k][i] = gl2std_prof[i]; snap[k][8 + i] = gl2_prof[i]; } for (int i = 0; i < 16; ++i) snap[k][16 + i] = gPhase[i]; snap[k][32] = w_upd; snap[k][33] = w_ren; snap[k][34] = w_swp; } while (0)
#ifdef LARA_WALK_SHOTS
            // Emulator side-by-side: every 60th frame read back (synchronous, so timings in this
            // build are not valid) into one heap block; its address is reported, the block is
            // never freed so it can be read from RAM after the run.
            uint8_t *shots = (uint8_t *)malloc((size_t)Core::width * Core::height * 4u * (LARA_WALK_FRAMES / 60));
            uint32_t nshots = 0;
#endif
            for (int f = 0; lara_walk_input(f) && !Core::isQuit; ++f) {
#ifdef LARA_FIXED_DT
                g_fixed_ms += 33;
#endif
                if (f == 450) WSNAP(0);
                if (f == 480) WSNAP(1);
                uint32_t tq = nano_app_usec(); bool upd = Game::update(); w_upd += nano_app_usec() - tq;
                if (upd) {
                    tq = nano_app_usec(); Game::render(); w_ren += nano_app_usec() - tq;
#ifdef LARA_WALK_SHOTS
                    if (shots && (f + 1) % 60 == 0) glReadPixels(0, 0, Core::width, Core::height, GL_RGBA, GL_UNSIGNED_BYTE, shots + (size_t)(nshots++) * Core::width * Core::height * 4u);
#endif
                    tq = nano_app_usec(); eglSwapBuffers(dpy, sfc); w_swp += nano_app_usec() - tq;
                }
                uint32_t now = nano_app_usec(), dt = now - tprev; tprev = now;
                ft[n++] = dt; segsum += dt; if (dt > segmax) segmax = dt;
                segdips += (uint32_t)Core::stats.dips; segtris += (uint32_t)Core::stats.tris;
                uint32_t e = (uint32_t)glGetError(); if (e && !walk_err) walk_err = e;
                if (n % 30u == 0) {
                    Controller *pl = (Controller *)Game::level->player;
                    rtext("\nW "); rword(n); rword(segsum / 30u); rword(segmax); rword(segdips / 30u); rword(segtris / 30u);
                    rword((uint32_t)pl->getRoomIndex()); rword((uint32_t)(int32_t)pl->pos.x); rword((uint32_t)(int32_t)pl->pos.z);
                    segsum = segmax = segdips = segtris = 0;
                }
            }
            WSNAP(2);
            // PROF: gl2std 0 ensure 1 drawElements 2 useProgram 3 bindTexture 4 uniformMatrix 5 uniform4fv 6 clear | GP: gl2 0 submit 1 texunits 2 header 3 draw 4 #draws 5 #verts 6 convert 7 #packets
            // PH: 0 visible rooms 1 setup 2 prepareRooms 3 opaque 4 transparent 5 additive 7 rooms-opaque | PV: 8 room verts 9 room draws 10 ent verts 11 ent draws 14 room submit 15 room draw | UPD REN SWP
            rtext("\nWPROF all  "); for (int i = 0; i < 35; ++i) rword(snap[2][i]);
            rtext("\nWPROF heavy "); for (int i = 0; i < 35; ++i) rword(snap[1][i] - snap[0][i]);
            { extern uint32_t gl2_prof2[8]; rtext("\nWPROF2 hdr_us delta_us sig_us hdr_built hdr_sent vtx_sent full delta "); for (int i = 0; i < 8; ++i) rword(gl2_prof2[i]); }
            // sorted percentiles
            for (uint32_t i = 1; i < n; ++i) { uint32_t v = ft[i], j = i; while (j && ft[j - 1] > v) { ft[j] = ft[j - 1]; --j; } ft[j] = v; }
            uint64_t sum = 0; for (uint32_t i = 0; i < n; ++i) sum += ft[i];
            rtext("\nWALK frames avg p50 p90 p99 max err recov ");
            rword(n); rword(n ? (uint32_t)(sum / n) : 0u); rword(n ? ft[n / 2] : 0u); rword(n ? ft[n * 9 / 10] : 0u); rword(n ? ft[n * 99 / 100] : 0u); rword(n ? ft[n - 1] : 0u);
            rword(walk_err); rword(gl2_egl_recoveries() - rec0);
            { extern uint32_t gl2_spec_builds, gl2_spec_hits; rtext("\nSPEC builds hits "); rword(gl2_spec_builds); rword(gl2_spec_hits); }
            uint32_t b[5] = {0,0,0,0,0}; for (uint32_t i = 0; i < n; ++i) b[ft[i] < 16700u ? 0 : ft[i] < 25000u ? 1 : ft[i] < 33400u ? 2 : ft[i] < 50000u ? 3 : 4]++;
#ifdef LARA_WALK_SHOTS
            rtext("\nSHOTS addr count w h "); rword((uint32_t)(uintptr_t)shots); rword(nshots); rword((uint32_t)Core::width); rword((uint32_t)Core::height);
#endif
            rtext("\nWALKHIST <16.7 <25 <33.4 <50 >=50ms "); for (int i = 0; i < 5; ++i) rword(b[i]);
            rflush();
        }
#endif
        rtext("\nERR "); rword(err_init); rword(err_rnd); rtext(" SITE "); rword(site_init); rword(site_rnd); rword(aux_rnd);

        Game::deinit();
        uint32_t frames = 0, sw = 0, fault = 0;
        nano_app_gfx_stats(&frames, &sw, &fault);
        uint32_t hu2 = 0, hf2 = 0, hlg2 = 0, hnb2 = 0;
        nano_heap_stats(&hu2, &hf2, &hlg2, &hnb2);

        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(dpy, ctx);
        eglDestroySurface(dpy, sfc);
        eglTerminate(dpy);

        rtext("\nLARA1 status/mount/frames/sw/fault/glerr/nonblack/px ");
        rword(0u); rword((uint32_t)mount_rc); rword(frames); rword(sw); rword(fault); rword(glerr); rword(nonblack); rword(px32);
        { volatile uint32_t *clk = (volatile uint32_t *)0x3c500000u; rtext("\nCLK "); rword(clk[0]); rword(clk[1]); rword(clk[7]); rword(clk[8]); }
        rtext("\nUS "); rword(t_init); rword(t_rnd); rtext("\nPROF "); for (int i = 0; i < 8; ++i) rword(gl2std_prof[i]); rtext("\nPH "); for (int i = 0; i < 8; ++i) rword(gPhase[i]); rtext("\nPV "); for (int i = 8; i < 16; ++i) rword(gPhase[i]);
#ifdef FIFOPROF_ADDR
        rtext("\nFP "); for (int i = 0; i < 6; ++i) rword(((volatile uint32_t *)FIFOPROF_ADDR)[i]);
#endif
#ifdef SCENETIM_ADDR
        rtext("\nST "); for (int i = 0; i < 12; ++i) rword(((volatile uint32_t *)SCENETIM_ADDR)[i]);
        rtext("\nPW "); for (int i = 0; i < 4; ++i) rword(((volatile uint32_t *)MBXPW_ADDR)[i]);
#endif
        rtext("\nGP "); for (int i = 0; i < 8; ++i) rword(gl2_prof[i]); rtext("DR "); rword((uint32_t)Core::stats.dips);

 rtext("DH "); rword(gl2_delta_headers); rtext("\nCD "); for (int i = 0; i < 5; ++i) rword(cellDiff[i]); rtext("\nFR "); for (int i = 0; i < 5; ++i) rword(fr[i]); rtext("\nUPD/REN/SWP "); rword(t_upd); rword(t_ren); rword(t_swp);
        rtext("\nHEAP0 "); rword(hu0); rword(hf0); rword(hlg0);
        rtext("\nHEAP1 "); rword(hu1); rword(hf1); rword(hlg1);
        rtext("\nHEAP2 "); rword(hu2); rword(hf2); rword(hlg2);
        rtext("\n");
        rtext((mount_rc == 0 && frames > 0u && fault == 0u && nonblack > 0u) ? "LARA PASS\n" : "LARA FAIL\n");
        g_report[g_rused] = 0;
        rpt_mirror_write(g_report, g_rused, rpt_sum(g_report, g_rused));
        nano_app_dcache_clean();
        return 0;
    }
#endif
    (void)mount_rc;

    // Playable build: same detail settings as the measured bench (LARA_BENCH), straight into
    // the level, in landscape (LARA_LANDSCAPE, home button on the right). Touch uses OpenLara's own zones (left third: move stick, middle: look, right:
    // jump/action/walk/weapon buttons). Side buttons (edges only): LEFT = jump, RIGHT = action.
    Core::settings.detail.shadows   = Core::Settings::LOW;
    Core::settings.detail.water     = Core::Settings::LOW;
    Core::settings.detail.lighting  = Core::Settings::LOW;
    Core::settings.detail.filter    = Core::Settings::LOW;
    Core::settings.detail.stereo    = Core::Settings::STEREO_OFF;
#ifndef LARA_DEMO_LEVEL
#define LARA_DEMO_LEVEL "DATA/LEVEL2.PHD"
#endif
    __asm__ volatile("" :: "r"(kBuildTag));   // keep the build tag through --gc-sections
    nano_flight_log("init %s orient %u screen %dx%d\n", LARA_DEMO_LEVEL, (unsigned)LARA_LANDSCAPE, Core::width, Core::height);
    nano_flight_commit();
    Game::init((char *)LARA_DEMO_LEVEL);

    uint32_t jumpUntil = 0, actionUntil = 0;
    // Flight recorder (NanoOS SDK): status every frame, a summary every 60 frames and a
    // screenshot every 120, all readable after a freeze + reboot (hwbridge/pull_flight.py).
    uint32_t fl_frame = 0, fl_t0 = nano_app_usec(), fl_worst = 0, fl_prev = nano_app_usec();
    nano_flight_log("play start level %s\n", LARA_DEMO_LEVEL);
    nano_flight_commit();
    while (!Core::isQuit) {
        uint32_t fl_user[4] = { fl_worst, (uint32_t)glGetError(), (uint32_t)(int32_t)Core::deltaTime, gl2_egl_recoveries() };
        nano_flight_status(fl_frame, 1, fl_user);
        nano_input_t in;
        nano_app_poll_input(&in);
        if (in.home_pressed) {
            Core::quit();
            break;
        }
        if (in.pressed || in.moved || in.down) {
            Input::setPos(ikTouchA, vec2((float)in.x, (float)in.y));
            Input::setDown(ikTouchA, true);
        } else if (in.released) {
            Input::setDown(ikTouchA, false);
        }
        uint32_t now = nano_app_usec();
        if (in.buttons & NGAME_LEFT)  jumpUntil   = now + 200000u;   // button reports are edges: hold briefly
        if (in.buttons & NGAME_RIGHT) actionUntil = now + 400000u;
        Input::setJoyDown(0, jkX, (int32_t)(jumpUntil - now) > 0);
        Input::setJoyDown(0, jkA, (int32_t)(actionUntil - now) > 0);

        nano_flight_status(fl_frame, 2, fl_user);
        if (Game::update()) {
            nano_flight_status(fl_frame, 3, fl_user);
            Game::render();
            nano_flight_status(fl_frame, 4, fl_user);
            eglSwapBuffers(dpy, sfc);
        }
        uint32_t now2 = nano_app_usec(), dt = now2 - fl_prev; fl_prev = now2;
        if (dt > fl_worst) fl_worst = dt;
        if (++fl_frame % 60u == 0) {
            uint32_t span = now2 - fl_t0; fl_t0 = now2;
            { uint32_t gf = 0, gs = 0, gx = 0; nano_app_gfx_stats(&gf, &gs, &gx);
              nano_flight_log("f%u avg %u us worst %u us glerr %x dips %d tris %d gpu frames %u sw %u fault %u recovered %u\n", (unsigned)fl_frame, (unsigned)(span / 60u), (unsigned)fl_worst,
                            (unsigned)fl_user[1], Core::stats.dips, Core::stats.tris, (unsigned)gf, (unsigned)gs, (unsigned)gx, (unsigned)gl2_egl_recoveries()); }
            fl_worst = 0;
            nano_flight_commit();
            if (fl_frame % 120u == 0) nano_flight_screenshot();
        }
    }
    nano_flight_log("quit at frame %u\n", (unsigned)fl_frame);
    nano_flight_commit();

    Game::deinit();
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(dpy, ctx);
    eglDestroySurface(dpy, sfc);
    eglTerminate(dpy);
    return 0;
}
