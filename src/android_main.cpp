#include <jni.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include <string>
#include <vector>
#include <memory>

#include "android_native_sandbox.hpp"

#define LOG_TAG "CyberBoxNative"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

// Shared state for NativeActivity
struct SavedState {
    int current_tab = 0;
    int context_window = 3;
};

struct EngineContext {
    struct android_app* app = nullptr;

    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    int32_t width = 0;
    int32_t height = 0;

    SavedState state;
    bool animating = true;

    // Sandbox with flexible storage path
    std::unique_ptr<sandbox::AndroidNativeSandbox> sandbox;
    std::string current_workspace;
    std::vector<sandbox::FileItem> workspace_files;
};

static int init_display(struct EngineContext* engine) {
    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };
    EGLint w, h, format;
    EGLint numConfigs;
    EGLConfig config = nullptr;
    EGLSurface surface;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

    ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);

    surface = eglCreateWindowSurface(display, config, engine->app->window, nullptr);
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGW("Unable to eglMakeCurrent");
        return -1;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    engine->display = display;
    engine->context = context;
    engine->surface = surface;
    engine->width = w;
    engine->height = h;

    glViewport(0, 0, w, h);
    LOGI("OpenGL ES 3.0 Display initialized successfully: %dx%d", w, h);
    return 0;
}

static void draw_frame(struct EngineContext* engine) {
    if (engine->display == EGL_NO_DISPLAY) {
        return;
    }

    // CyberBox Dark Modern Theme (#0d1117)
    glClearColor(0.051f, 0.067f, 0.090f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // EGL buffer swap
    eglSwapBuffers(engine->display, engine->surface);
}

static void term_display(struct EngineContext* engine) {
    if (engine->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (engine->context != EGL_NO_CONTEXT) {
            eglDestroyContext(engine->display, engine->context);
        }
        if (engine->surface != EGL_NO_SURFACE) {
            eglDestroySurface(engine->display, engine->surface);
        }
        eglTerminate(engine->display);
    }
    engine->animating = false;
    engine->display = EGL_NO_DISPLAY;
    engine->context = EGL_NO_CONTEXT;
    engine->surface = EGL_NO_SURFACE;
}

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    struct EngineContext* engine = (struct EngineContext*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        // Multi-touch gestures and taps
        return 1;
    }
    return 0;
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct EngineContext* engine = (struct EngineContext*)app->userData;
    switch (cmd) {
        case APP_CMD_SAVE_STATE:
            engine->app->savedState = malloc(sizeof(SavedState));
            *((SavedState*)engine->app->savedState) = engine->state;
            engine->app->savedStateSize = sizeof(SavedState);
            break;
        case APP_CMD_INIT_WINDOW:
            if (engine->app->window != nullptr) {
                init_display(engine);
                draw_frame(engine);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            term_display(engine);
            break;
        case APP_CMD_GAINED_FOCUS:
            engine->animating = true;
            break;
        case APP_CMD_LOST_FOCUS:
            engine->animating = false;
            draw_frame(engine);
            break;
    }
}

// Entry Point for 100% NativeActivity C++ Android Application
void android_main(struct android_app* state) {
    struct EngineContext engine;
    memset(&engine, 0, sizeof(engine));
    state->userData = &engine;
    state->onAppCmd = handle_cmd;
    state->onInputEvent = handle_input;
    engine.app = state;

    // Flexible Workspace Initialization:
    // 1. Try external storage if granted: /sdcard/CyberBox
    // 2. Fall back to internal private data directory
    std::string internal_path = state->activity->internalDataPath ? state->activity->internalDataPath : "/data/data/com.cyberbox.ai/files";
    std::string external_path = state->activity->externalDataPath ? state->activity->externalDataPath : "/sdcard/CyberBox";

    // Initialize Sandbox with flexible workspace
    engine.sandbox = std::make_unique<sandbox::AndroidNativeSandbox>(internal_path);
    engine.sandbox->initialize();
    LOGI("CyberBox Native Sandbox initialized at: %s", internal_path.c_str());

    if (state->savedState != nullptr) {
        engine.state = *(SavedState*)state->savedState;
    }

    // Main event loop
    while (true) {
        int ident;
        int events;
        struct android_poll_source* source = nullptr;

        while ((ident = ALooper_pollOnce(engine.animating ? 0 : -1, nullptr, &events, (void**)&source)) >= 0) {
            if (source != nullptr) {
                source->process(state, source);
            }
            if (state->destroyRequested != 0) {
                term_display(&engine);
                return;
            }
        }

        if (engine.animating) {
            draw_frame(&engine);
        }
    }
}
