#pragma once

#include "editor/Editor.h"

#include <SDL3/SDL.h>

namespace kaya::designer {

class App {
public:
    bool initialize();
    void run();
    void shutdown();

private:
    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    bool running_ = false;
    Editor editor_;
};

}

