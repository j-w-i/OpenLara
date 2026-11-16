#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "nano_app.h"
#include "nano_heap.h"
#include "report.h"
#include "EGL/egl.h"
#include "GLES2/gl2.h"
int openlara_fs_init(void);
}

#ifdef LARA_BENCH
static char g_report[1024];
static unsigned g_rused;
static void rtext(const char *s) { while (*s && g_rused + 1 < sizeof(g_report)) g_report[g_rused++] = *s++; }
static void rword(uint32_t x) {
    if (g_rused + 10 >= sizeof(g_report)) return;
    static const char h[] = "0123456789abcdef";
    for (unsigned i = 0; i < 8; ++i) g_report[g_rused++] = h[(x >> (28 - i * 4)) & 15u];
    g_report[g_rused++] = ' ';
}
static GLuint bench_prog(const char *vs_defs, const char *fs_defs, const char *src, char *vbuf, size_t vcap, char *fbuf, size_t fcap) {
    snprintf(vbuf, vcap, "#define VERTEX\n%s\n%s", vs_defs, src);
    snprintf(fbuf, fcap, "#define fragColor gl_FragColor\n%s\n%s", fs_defs, src);
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    const char *vp = vbuf, *fp = fbuf;
    glShaderSource(vs, 1, &vp, NULL);
    glShaderSource(fs, 1, &fp, NULL);
    glCompileShader(vs);
    glCompileShader(fs);
    GLint vok = 0, fok = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &vok);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &fok);
    if (!vok || !fok) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint lok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &lok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return lok ? p : 0;
}
#endif

#include "game.h"

int osGetTimeMS() {
    return (int)(nano_app_usec() / 1000u);
}

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

    EGLSurface sfc = eglCreateWindowSurface(dpy, cfg, (EGLNativeWindowType)0, NULL);
    if (sfc == EGL_NO_SURFACE) return 4;

    static const EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (ctx == EGL_NO_CONTEXT) return 5;
    if (!eglMakeCurrent(dpy, sfc, sfc, ctx)) return 6;

    Core::width = (int)NANO_SCREEN_W;
    Core::height = (int)NANO_SCREEN_H;

    contentDir[0] = 0;
    cacheDir[0] = 0;
    saveDir[0] = 0;

#ifdef LARA_BENCH
    {
        static const char GUI_SRC[] =
            #include "shaders/gui.glsl"
        ;
        static const char AMB_SRC[] =
            #include "shaders/ambient.glsl"
        ;
        static char vbuf[8192], fbuf[8192];
        GLuint gui_p = bench_prog("", "", GUI_SRC, vbuf, sizeof(vbuf), fbuf, sizeof(fbuf));
        GLuint amb_p = bench_prog("", "", AMB_SRC, vbuf, sizeof(vbuf), fbuf, sizeof(fbuf));
        uint32_t gui_ok = gui_p ? 1u : 0u;
        uint32_t amb_ok = amb_p ? 1u : 0u;
        uint32_t px32 = 0;

        if (gui_p) {
            glUseProgram(gui_p);
            GLuint tex = 0;
            static uint8_t tex_rgba[8 * 8 * 4];
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    int idx = (y * 8 + x) * 4;
                    tex_rgba[idx + 0] = (uint8_t)(64 + x * 24);
                    tex_rgba[idx + 1] = (uint8_t)(64 + y * 24);
                    tex_rgba[idx + 2] = 0xc0u;
                    tex_rgba[idx + 3] = 0xffu;
                }
            }
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_rgba);

            static const float ident[16] = {
                1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
            };
            static const float verts[16] = {
                -0.6f,-0.6f,0,1,  0.6f,-0.6f,0,1,  -0.6f,0.6f,0,1,  0.6f,0.6f,0,1
            };
            static const float uvs[16] = {
                0,0,0,0,  1,0,0,0,  0,1,0,0,  1,1,0,0
            };
            static const float cols[16] = {
                1,1,1,1,  1,1,1,1,  1,1,1,1,  1,1,1,1
            };
            glUniformMatrix4fv(glGetUniformLocation(gui_p, "uViewProj"), 1, GL_FALSE, ident);
            glUniform4f(glGetUniformLocation(gui_p, "uMaterial"), 1.0f, 1.0f, 1.0f, 1.0f);
            glUniform1i(glGetUniformLocation(gui_p, "sDiffuse"), 0);

            GLint a_coord = glGetAttribLocation(gui_p, "aCoord");
            GLint a_uv    = glGetAttribLocation(gui_p, "aTexCoord");
            GLint a_light = glGetAttribLocation(gui_p, "aLight");
            glVertexAttribPointer((GLuint)a_coord, 4, GL_FLOAT, GL_FALSE, 0, verts);
            glEnableVertexAttribArray((GLuint)a_coord);
            glVertexAttribPointer((GLuint)a_uv, 4, GL_FLOAT, GL_FALSE, 0, uvs);
            glEnableVertexAttribArray((GLuint)a_uv);
            glVertexAttribPointer((GLuint)a_light, 4, GL_FLOAT, GL_FALSE, 0, cols);
            glEnableVertexAttribArray((GLuint)a_light);

            glViewport(0, 0, (GLsizei)NANO_SCREEN_W, (GLsizei)NANO_SCREEN_H);
            for (int f = 0; f < 30; ++f) {
                glClearColor(0.05f, 0.08f, 0.14f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                if (f == 29) {
                    uint8_t px[4] = {0, 0, 0, 0};
                    glReadPixels(120, 216, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
                    px32 = ((uint32_t)px[3] << 24) | ((uint32_t)px[0] << 16) | ((uint32_t)px[1] << 8) | px[2];
                }
                eglSwapBuffers(dpy, sfc);
            }
            glDeleteTextures(1, &tex);
        }
        if (gui_p) glDeleteProgram(gui_p);
        if (amb_p) glDeleteProgram(amb_p);
        uint32_t glerr = (uint32_t)glGetError();
        uint32_t frames = 0, sw = 0, fault = 0;
        nano_app_gfx_stats(&frames, &sw, &fault);
        uint32_t hu = 0, hf = 0, hlg = 0, hnb = 0;
        nano_heap_stats(&hu, &hf, &hlg, &hnb);

        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(dpy, ctx);
        eglDestroySurface(dpy, sfc);
        eglTerminate(dpy);

        rtext("NANOOS GL2 GAME build " __DATE__ " " __TIME__ "\nLARA1 status/mount/gui/amb/frames/sw/fault/glerr/px ");
        rword(0u); rword((uint32_t)mount_rc); rword(gui_ok); rword(amb_ok);
        rword(frames); rword(sw); rword(fault); rword(glerr); rword(px32);
        rtext("\nHEAP "); rword(hu); rword(hf); rword(hlg); rword(hnb);
        rtext("\n");
        rtext((mount_rc == 0 && gui_ok && amb_ok && frames == 30u && sw == 0u && fault == 0u && glerr == 0u && (px32 & 0xffffffu) != 0u) ? "LARA PASS\n" : "LARA FAIL\n");
        g_report[g_rused] = 0;
        rpt_mirror_write(g_report, g_rused, rpt_sum(g_report, g_rused));
        nano_app_dcache_clean();
        return 0;
    }
#endif
    (void)mount_rc;

    Game::init((char *)NULL);

    while (!Core::isQuit) {
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

        if (Game::update()) {
            Game::render();
            eglSwapBuffers(dpy, sfc);
        }
    }

    Game::deinit();
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(dpy, ctx);
    eglDestroySurface(dpy, sfc);
    eglTerminate(dpy);
    return 0;
}
