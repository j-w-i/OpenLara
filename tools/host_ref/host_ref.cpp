// Host reference renderer: the iPod build of OpenLara (same engine defines, same GLES2
// code path, same detail settings) rendered on macOS through ANGLE's GLES 2.0 + EGL.
// Ground truth for NanoOS GL2 graphics comparisons. Frame time is a fixed 33 ms step
// (the Nano bench uses the same with -DLARA_FIXED_DT), so frame N matches frame N.
//
//   tools/host_ref/build.sh && tools/host_ref/run.sh /path/to/dir/containing/DATA out_dir
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern "C" {
#include "EGL/egl.h"
#include "GLES2/gl2.h"
}
uint32_t gPhase[16];
extern "C" uint32_t gl2_prof[8];
uint32_t gl2_prof[8];
static uint32_t g_ms;
extern "C" uint32_t nano_app_usec(void) { return g_ms * 1000u; }
extern "C" void lara_log(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); }
#include "game.h"
int osGetTimeMS() { return (int)g_ms; }
void osJoyVibrate(int, float, float) {}
void *osMutexInit() { return NULL; }
void osMutexFree(void *) {}
void osMutexLock(void *) {}
void osMutexUnlock(void *) {}
void osCacheRead(Stream *stream) { if (stream) stream->callback(NULL, stream->userData); }
void osCacheWrite(Stream *stream) { if (stream) stream->callback(NULL, stream->userData); }
void osReadSlot(Stream *stream) { if (stream) stream->callback(NULL, stream->userData); }
void osWriteSlot(Stream *stream) { if (stream) stream->callback(NULL, stream->userData); }
#include "../../src/walk_script.h"

static void save_ppm(const char *path, int w, int h) {
    static uint8_t px[240 * 432 * 4];
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    FILE *f = fopen(path, "wb"); if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; --y) for (int x = 0; x < w; ++x) fwrite(px + (y * w + x) * 4, 1, 3, f);
    fclose(f);
}

int main(int argc, char **argv) {
    const char *out = argc > 1 ? argv[1] : ".";
    int frames = argc > 2 ? atoi(argv[2]) : 6;
    // HOST_LANDSCAPE=1 renders the 432x240 surface the device uses in landscape mode
    const bool land = getenv("HOST_LANDSCAPE") && atoi(getenv("HOST_LANDSCAPE"));
    const int W = land ? 432 : 240, H = land ? 240 : 432;
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, NULL, NULL)) { fprintf(stderr, "EGL init failed\n"); return 1; }
    const EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE };
    EGLConfig cfg; EGLint n = 0;
    if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || n < 1) { fprintf(stderr, "no EGL config\n"); return 1; }
    const EGLint sattr[] = { EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE };
    EGLSurface sfc = eglCreatePbufferSurface(dpy, cfg, sattr);
    const EGLint cattr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };   // GLES 2.0 only
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, cattr);
    if (sfc == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, sfc, sfc, ctx)) { fprintf(stderr, "EGL context failed\n"); return 1; }
    fprintf(stderr, "GL_VERSION %s\nGL_RENDERER %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

    Core::width = W; Core::height = H;
    contentDir[0] = cacheDir[0] = saveDir[0] = 0;
    // same detail settings as the Nano bench and playable build (src/main.cpp)
    Core::settings.detail.shadows  = Core::Settings::LOW;
    Core::settings.detail.water    = Core::Settings::LOW;
    Core::settings.detail.lighting = Core::Settings::LOW;
    Core::settings.detail.filter   = (Core::Settings::Quality)(getenv("HOST_FILTER") ? atoi(getenv("HOST_FILTER")) : 0);   // as LARA_FILTER
    Core::settings.detail.stereo   = Core::Settings::STEREO_OFF;
    Game::init((char *)"DATA/LEVEL2.PHD");
    // HOST_WALK=1: after the fixed frames, replay src/walk_script.h like the LARA_WALK device
    // bench and save every 30th frame as walkNNN.ppm (plus Lara's room/position) to see the path.
    if (getenv("HOST_WALK") && atoi(getenv("HOST_WALK"))) {
        for (int f = 0; f < frames; ++f) { g_ms += 33; if (Game::update()) { Game::render(); eglSwapBuffers(dpy, sfc); } }
        for (int f = 0; lara_walk_input(f); ++f) {
            g_ms += 33;
            if (Game::update()) {
                Game::render();
                if ((f + 1) % 30 == 0) {
                    Controller *pl = (Controller *)Game::level->player;
                    fprintf(stderr, "W %d dips %d tris %d room %d x %d z %d\n", f + 1, Core::stats.dips, Core::stats.tris, pl->getRoomIndex(), (int)pl->pos.x, (int)pl->pos.z);
                    char path[512]; snprintf(path, sizeof path, "%s/walk%03d.ppm", out, f + 1); save_ppm(path, W, H);
                }
                eglSwapBuffers(dpy, sfc);
            }
        }
        Game::deinit();
        return 0;
    }
    for (int f = 0; f < frames; ++f) {
        g_ms += 33;                                    // fixed step, as LARA_FIXED_DT on the device
        if (Game::update()) {
            Game::render();
            if (getenv("HOST_STATE")) fprintf(stderr, "STATE f%d dt %.4f title %.3f health %.1f dmg %.3f wpn %d empty %d\n", f, Core::deltaTime, inventory->titleTimer, Game::level->player->health, Game::level->player->damageTime, (int)Game::level->player->wpnReady(), (int)Game::level->player->emptyHands());
            char path[512]; snprintf(path, sizeof path, "%s/ref%d.ppm", out, f);
            save_ppm(path, W, H);
            eglSwapBuffers(dpy, sfc);
        }
    }
    Game::deinit();
    fprintf(stderr, "glGetError %x\n", glGetError());
    return 0;
}
