#pragma once

#include "context.h"

class Renderer {
    Context ctx;

public:
    explicit Renderer(Context ctx);
    void update();
    void draw_frame();
};