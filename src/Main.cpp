#include "Renderer.h"
#include <algorithm>
#include <android/log.h>
#include <android/window.h>
#include <android_native_app_glue.h>
#include <chrono>
#include <cmath>
#include <exception>
namespace {
using Clock = std::chrono::steady_clock;
struct App {
    android_app *android;
    std::unique_ptr<ForestRenderer> renderer;
    bool active = false, failed = false;
    float yaw = .24f, pitch = .12f, distance = 13, x = 0, y = 0, pinch = 0;
    int fingers = 0;
    float fps = 0, seconds = 0;
    unsigned count = 0;
    Clock::time_point previous = Clock::now(), sample = previous;
    void reset() {
        previous = sample = Clock::now();
        count = 0;
        fps = 0;
        fingers = 0;
    }
};
void fail(App &a, const char *message) {
    __android_log_print(ANDROID_LOG_ERROR, "ForestForge", "%s", message);
    a.failed = true;
    ANativeActivity_finish(a.android->activity);
}
void command(android_app *android, int32_t cmd) {
    auto &a = *static_cast<App *>(android->userData);
    if (a.failed)
        return;
    try {
        if (cmd == APP_CMD_INIT_WINDOW && android->window) {
            if (!a.renderer)
                a.renderer = std::make_unique<ForestRenderer>(android->activity);
            a.renderer->open(android->window);
            a.reset();
        } else if (cmd == APP_CMD_TERM_WINDOW) {
            if (a.renderer)
                a.renderer->close();
            a.reset();
        } else if (cmd == APP_CMD_GAINED_FOCUS || cmd == APP_CMD_RESUME) {
            a.active = true;
            a.reset();
        } else if (cmd == APP_CMD_LOST_FOCUS || cmd == APP_CMD_PAUSE) {
            a.active = false;
            a.reset();
        } else if (cmd == APP_CMD_WINDOW_RESIZED && android->window && a.renderer) {
            a.renderer->open(android->window);
            a.reset();
        }
    } catch (const std::exception &e) {
        fail(a, e.what());
    }
}
int32_t input(android_app *android, AInputEvent *e) {
    auto &a = *static_cast<App *>(android->userData);
    if (AInputEvent_getType(e) == AINPUT_EVENT_TYPE_KEY && AKeyEvent_getKeyCode(e) == AKEYCODE_BACK) {
        ANativeActivity_finish(android->activity);
        return 1;
    }
    if (AInputEvent_getType(e) != AINPUT_EVENT_TYPE_MOTION || !android->window)
        return 0;
    int action = AMotionEvent_getAction(e) & AMOTION_EVENT_ACTION_MASK;
    if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL ||
        action == AMOTION_EVENT_ACTION_POINTER_UP) {
        a.fingers = 0;
        return 1;
    }
    int count = AMotionEvent_getPointerCount(e);
    float x = AMotionEvent_getX(e, 0), y = AMotionEvent_getY(e, 0), pinch = 0;
    if (count == 2)
        pinch = std::hypot(x - AMotionEvent_getX(e, 1), y - AMotionEvent_getY(e, 1));
    if (action == AMOTION_EVENT_ACTION_MOVE && a.fingers == count) {
        if (count == 1) {
            a.yaw -= (x - a.x) / ANativeWindow_getWidth(android->window) * 4;
            a.pitch =
                std::clamp(a.pitch + (y - a.y) / ANativeWindow_getHeight(android->window) * 2, .03f, .85f);
        } else if (count == 2 && pinch > 1 && a.pinch > 1)
            a.distance = std::clamp(a.distance * a.pinch / pinch, 4.f, 24.f);
    }
    a.fingers = count;
    a.x = x;
    a.y = y;
    a.pinch = pinch;
    return 1;
}
} // namespace
extern "C" void android_main(android_app *android) {
    App app{android};
    android->userData = &app;
    android->onAppCmd = command;
    android->onInputEvent = input;
    ANativeActivity_setWindowFlags(android->activity, AWINDOW_FLAG_KEEP_SCREEN_ON | AWINDOW_FLAG_FULLSCREEN,
                                   0);
    while (!android->destroyRequested) {
        int events;
        android_poll_source *source;
        bool render = !app.failed && app.active && app.renderer && app.renderer->ready();
        int id = ALooper_pollOnce(render ? 0 : -1, nullptr, &events, reinterpret_cast<void **>(&source));
        if (id >= 0 && source)
            source->process(android, source);
        if (android->destroyRequested)
            break;
        if (!app.failed && app.active && app.renderer && app.renderer->ready())
            try {
                auto now = Clock::now();
                app.seconds += std::chrono::duration<float>(now - app.previous).count();
                app.previous = now;
                app.renderer->frame(app.seconds, app.yaw, app.pitch, app.distance, app.fps);
                auto done = Clock::now();
                app.count++;
                float elapsed = std::chrono::duration<float>(done - app.sample).count();
                if (elapsed >= .5f) {
                    app.fps = app.count / elapsed;
                    app.count = 0;
                    app.sample = done;
                }
            } catch (const std::exception &e) {
                fail(app, e.what());
            }
    }
    app.renderer.reset();
}
