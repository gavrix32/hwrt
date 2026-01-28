#include <spdlog/spdlog.h>

#include "asset.h"
#include "context.h"
#include "input.h"
#include "renderer.h"
#include "timer.h"
#include "window.h"
#include "vulkan/device.h"

#define WIDTH 1280
#define HEIGHT 720

// TODO: Abstractions (sbt, image, view, imgui)
// TODO: Model instancing
// TODO: Normal logging without macro
// TODO: Shader hot reloading
// TODO: Meshoptimizer?

#ifdef NDEBUG
constexpr bool validation = false;
#else
constexpr bool validation = true;
#endif

int main() {
#ifdef NDEBUG
    spdlog::set_level(spdlog::level::info);
#else
    spdlog::set_level(spdlog::level::trace);
#endif

    Window::init(WIDTH, HEIGHT, "hwrt");
    Window::hide();
    {
        float speed = 3.0f;
        float delta = 0.0f;
        float accumulator = 0.0f;
        bool mouse_grab = false;

        Timer timer{};
        Context ctx(validation);

        auto model = AssetLoader::load_model("../assets/models/ABeautifulGame.glb");

        auto camera = Camera();
        camera.set_pos(glm::vec3(0.0f, 0.0f, 1.0f));

        Scene scene;
        scene.set_camera(camera);
        scene.add_instance(model, glm::mat4(1.0f));
        scene.build_blases(ctx);
        scene.build_tlas(ctx);

        Renderer renderer(ctx);

        Window::show();
        while (!Window::should_close()) {
            timer.tick();

            Input::update();

            if (Input::key_released(GLFW_KEY_ESCAPE)) {
                Window::close();
            }

            if (Input::mouse_button_released(GLFW_MOUSE_BUTTON_RIGHT)) {
                mouse_grab = !mouse_grab;
                Input::set_cursor_grab(mouse_grab);
            }

            if (const auto scroll = static_cast<float>(Input::get_mouse_scroll());
                scroll != 0.0f && mouse_grab
            ) {
                speed += speed * scroll * 0.1f;
                speed = std::max(speed, 0.001f);
            }

            if (mouse_grab) {
                constexpr float sensitivity = 0.005f;
                const auto mouse_delta = Input::get_mouse_delta() * sensitivity;

                camera.set_rot(camera.get_rot() - glm::vec2(mouse_delta.y, mouse_delta.x));

                const float s = speed * delta;

                if (Input::key_down(GLFW_KEY_W)) camera.move_z(-s);
                if (Input::key_down(GLFW_KEY_A)) camera.move_x(-s);
                if (Input::key_down(GLFW_KEY_S)) camera.move_z(s);
                if (Input::key_down(GLFW_KEY_D)) camera.move_x(s);
                if (Input::key_down(GLFW_KEY_SPACE)) camera.move_y(s);
                if (Input::key_down(GLFW_KEY_LEFT_SHIFT)) camera.move_y(-s);
            }

            scene.set_camera(camera);
            renderer.draw_frame(scene);

            if (accumulator >= 1.0) {
                spdlog::info("{:.0f} fps ({:.2f} ms)", 1.0f / delta, delta * 1000.0f);
                accumulator = 0.0f;
            }
            delta = timer.get_delta();
            accumulator += delta;
        }
        ctx.get_device().get().waitIdle();
    }
    Window::terminate();

    return 0;
}