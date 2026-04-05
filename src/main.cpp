#include <spdlog/spdlog.h>

#include <iostream>
#include <random>

#include "asset.h"
#include "context.h"
#include "gui.h"
#include "imgui_internal.h"
#include "input.h"
#include "renderer.h"
#include "timer.h"
#include "window.h"
#include "glm/gtx/matrix_decompose.hpp"
#include "vulkan/device.h"

#define WIDTH 1280
#define HEIGHT 720

// TODO: Normal logging without macro
// TODO: Meshoptimizer?

// TODO: Замена для stb image? долго грузит текстуры
// TODO: Перенести scene storage буферы на GPU!!!
// TODO: И хелпер для staging buffers
// TODO: ABeautifulGame -> CornellBox model in github

void show_scene_graph(const Scene& scene) {
    // Instance -> Node -> Mesh -> Primitive
    ImGui::Begin("Scene Graph");
    auto& instances = scene.get_instances();
    for (size_t i = 0; i < instances.size(); ++i) {
        if (ImGui::TreeNode(reinterpret_cast<void*>(i), "Instance %zu", i)) {
            auto& model = instances[i].model;

            auto& nodes = model->nodes;
            // auto& materials = model->materials;
            for (size_t j = 0; j < nodes.size(); ++j) {
                if (ImGui::TreeNode(reinterpret_cast<void*>(j), "Node %zu", j)) {
                    const auto mesh_index = nodes[j].mesh_index;
                    ImGui::Text("Mesh %u", mesh_index); // TODO: Mesh selector

                    const auto& transform = nodes[j].transform;

                    glm::vec3 scale;
                    glm::quat orientation;
                    glm::vec3 translation;
                    glm::vec3 skew;
                    glm::vec4 perspective;
                    glm::decompose(transform, scale, orientation, translation, skew, perspective);

                    glm::vec3 rotation = glm::eulerAngles(orientation);

                    ImGui::DragFloat3("Translation", &translation[0]);
                    ImGui::DragFloat3("Rotation", &rotation[0]);
                    ImGui::DragFloat3("Scale", &scale[0]);

                    auto& mesh = model->meshes[mesh_index];
                    auto& primitives = mesh.primitives;
                    for (size_t k = 0; k < primitives.size(); ++k) {
                        if (ImGui::TreeNode(reinterpret_cast<void*>(j), "Primitive %zu", k)) {
                            const auto material_index = primitives[k].material_index;
                            // auto& material = materials[material_index];
                            ImGui::Text("Material %u", material_index);
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
    }
    ImGui::End();
}

int main(const int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);

    bool validation = false;

    std::vector<std::string> args(argv, argv + argc);

    std::vector<std::string> arg_model_paths;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-h" || args[i] == "--help") {
            std::cout << "Usage: hwrt [OPTIONS]\n"
                << "Options:\n"
                << "  -h, --help          Display this help message and exit\n"
                << "  -v, --validation    Enable Vulkan validation validation layers\n"
                << "  -m, --model <FILE>  Load .glb model from the specified path\n";
            return 0;
        }
        if (args[i] == "-v" || args[i] == "--validation") {
            validation = true;
        } else if (args[i] == "-m" || args[i] == "--model") {
            if (i + 1 < args.size()) {
                arg_model_paths.push_back(args[i + 1]);
                i++;
            } else {
                std::cerr << "Error: " << args[i] << " requires a file path argument\n"
                    << "Try 'hwrt --help' for more information\n";
                return 1;
            }
        } else {
            std::cerr << "Error: unknown option '" << args[i] << "'\n"
                << "Try 'hwrt --help' for more information.\n";
            return 1;
        }
    }

    Window::init(WIDTH, HEIGHT, "hwrt");
    Window::hide();
    {
        float speed = 3.0f;
        float delta = 0.0f;
        bool mouse_grab = false;

        Timer timer{};
        Context ctx(validation);

        AssetManager asset_manager;

        //const auto model = asset_manager.get_model("../assets/models/sponza.glb");

        auto camera = Camera();
        camera.set_pos(glm::vec3(0.0f, 1.0f, 4.0f));
        camera.set_fov(40.0f);

        Scene scene;
        scene.set_camera(camera);
        //scene.add_instance(model, glm::mat4(1.0f), ctx);

        //glm::scale(glm::mat4(1.0f), glm::vec3(0.01f)

        for (auto& path : arg_model_paths) {
            scene.add_instance(asset_manager.get_model(path), glm::mat4(1.0f), ctx);
        }

        std::default_random_engine generator;
        std::uniform_real_distribution distribution(0.0f, 360.0f);

        // int size = 67;
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
        //             scene.add_instance(model, model_matrix, ctx);
        //         }
        //     }
        // }
        scene.build_blases(ctx);
        scene.build_tlas(ctx);
        scene.build_descriptor_set(ctx);

        //spdlog::info("Loaded scene with {} instances", pow(size, 3));

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
            ImGui::Text("Accumulated Frames: %u", std::min(renderer.get_frame_count(), renderer.get_settings().iterations));

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
            int samples = static_cast<int>(renderer.get_settings().samples);
            if (ImGui::SliderInt("Samples", &samples, 1, 32)) {
                renderer.get_settings().samples = std::clamp(samples, 1, 32);
                renderer.update_settings();
            }
            int max_depth = static_cast<int>(renderer.get_settings().max_depth);
            if (ImGui::SliderInt("Max Depth", &max_depth, 0, 32)) {
                renderer.get_settings().max_depth = std::clamp(max_depth, 0, 32);
                renderer.update_settings();
            }
            int iterations = static_cast<int>(renderer.get_settings().iterations);
            if (ImGui::SliderInt("Iterations", &iterations, -1, 128)) {
                renderer.get_settings().iterations = iterations;
                renderer.update_settings();
            }
            if (ImGui::Button("Reload Shaders (R)") || Input::key_released(GLFW_KEY_R)) {
                renderer.reload_shaders();
            }
            ImGui::End();

            //show_scene_graph(scene);

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