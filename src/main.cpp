#include <spdlog/spdlog.h>

#include <random>

#include "asset.h"
#include "context.h"
#include "gui.h"
#include "imgui_internal.h"
#include "input.h"
#include "renderer.h"
#include "timer.h"
#include "window.h"
#include "vulkan/device.h"

#define WIDTH 1280
#define HEIGHT 720

// TODO: Normal logging without macro
// TODO: Shader hot reloading
// TODO: Meshoptimizer?

// TODO: Замена для stb image? долго грузит текстуры
// TODO: Починить инстансинг
// TODO: Перенести scene storage буферы на GPU!!!
// TODO: И хелпер для staging buffers
// TODO: удалить pipeline cache и узнать за сколько он создаётся
// TODO: alpha_cutoff, alpha_mode

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
        bool mouse_grab = false;

        Timer timer{};
        Context ctx(validation);

        AssetManager asset_manager;

        const auto model = asset_manager.get_model("../assets/models/ABeautifulGame.glb");

        auto camera = Camera();
        camera.set_pos(glm::vec3(0.0f, 0.0f, 1.0f));

        Scene scene;
        scene.set_camera(camera);
        scene.add_instance(model, glm::mat4(1.0f), ctx);

        std::default_random_engine generator;
        std::uniform_real_distribution distribution(0.0f, 360.0f);

        int size = 67;
        // for (int i = 0; i < size; ++i) {
        //     for (int j = 0; j < size; ++j) {
        //         for (int k = 0; k < size; ++k) {
        //             float offset = 2.0f;
        //
        //             glm::mat4 model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(i, j, k) * offset);
        //
        //             float angleX = glm::radians(distribution(generator));
        //             float angleY = glm::radians(distribution(generator));
        //             float angleZ = glm::radians(distribution(generator));
        //
        //             model_matrix = glm::rotate(model_matrix, angleX, glm::vec3(1.0f, 0.0f, 0.0f));
        //             model_matrix = glm::rotate(model_matrix, angleY, glm::vec3(0.0f, 1.0f, 0.0f));
        //             model_matrix = glm::rotate(model_matrix, angleZ, glm::vec3(0.0f, 0.0f, 1.0f));
        //
        //             scene.add_instance(model, model_matrix);
        //         }
        //     }
        // }
        scene.build_blases(ctx);
        scene.build_tlas(ctx);
        scene.build_descriptor_set(ctx);

        spdlog::info("Loaded scene with {} instances", pow(size, 3));

        Renderer renderer(ctx);

        Gui::init(ctx, renderer.get_swapchain());

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

            Gui::begin();

            ImGui::ShowDemoWindow();

            ImGui::Begin("HWRT");

            static float slow_delta = 0.0f;

            ImGui::Text("Frametime: %.2f ms (%.0f FPS)", slow_delta * 1000.0f, 1.0f / slow_delta);

            const char* items[] = {"None",
                                   "Texcoord",
                                   "Depth",
                                   "Hitpos",
                                   "Normal Texture",
                                   "Geometry Normal",
                                   "Geometry Tangent",
                                   "Geometry Bitangent",
                                   "Geometry Tangent W",
                                   "Shading Normal",
                                   "Alpha",
                                   "Emissive",
                                   "Base Color",
                                   "Metallic",
                                   "Roughness",
                                   "Heatmap"};
            static int current_item_index = 0;
            if (ImGui::Combo("Debug Channel", &current_item_index, items, IM_ARRAYSIZE(items))) {
                renderer.get_settings().debug_channel = static_cast<DebugChannel>(current_item_index);
                renderer.update_settings();
            }
            ImGui::End();

            Gui::end();

            scene.set_camera(camera);
            renderer.draw_frame(scene);

            static float log_accumulator = 0.0f;
            if (log_accumulator >= 1.0) {
                spdlog::info("{:.0f} fps ({:.2f} ms)", 1.0f / delta, delta * 1000.0f);
                log_accumulator = 0.0f;
            }
            static float imgui_accumulator = 0.0f;
            if (imgui_accumulator >= 0.1) {
                slow_delta = delta;
                imgui_accumulator = 0.0f;
            }
            delta = timer.get_delta();
            log_accumulator += delta;
            imgui_accumulator += delta;
        }
        ctx.get_device().get().waitIdle();
        Gui::terminate();
    }
    Window::terminate();

    return 0;
}