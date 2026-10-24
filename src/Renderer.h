#pragma once
#include <android/native_activity.h>
#include <android/native_window.h>
#include <memory>
class ForestRenderer {
  public:
    explicit ForestRenderer(ANativeActivity *activity);
    ~ForestRenderer();
    void open(ANativeWindow *window);
    void close();
    void frame(float seconds, float yaw, float pitch, float distance, float fps);
    bool ready() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
