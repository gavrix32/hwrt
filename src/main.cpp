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
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "vulkan/device.h"

#define WIDTH 1280
#define HEIGHT 720

// TODO: Normal logging without macro
// TODO: Meshoptimizer?

// TODO: Замена для stb image? долго грузит текстуры
// TODO: Перенести scene storage буферы на GPU!!!
// TODO: И хелпер для staging buffers

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

void show_solid_sky_settings(Renderer& renderer) {
    static glm::vec3 sky_color = renderer.get_settings().sky_color;
    static float sky_emission = 1.0f;
    if (ImGui::ColorEdit3("Sky Color", &sky_color[0])) {
        renderer.get_settings().sky_color = sky_color * sky_emission;
        renderer.update_settings();
    }
    if (ImGui::DragFloat("Sky Emission", &sky_emission, 0.01f, 0.0f, 1000.0f)) {
        renderer.get_settings().sky_color = sky_color * sky_emission;
        renderer.update_settings();
    }
}

void show_sun_settings(Renderer& renderer) {
    static glm::vec3 sun_color = renderer.get_settings().sun_color;
    static float sun_emission = renderer.get_settings().sun_emission;
    if (renderer.get_settings().environment_type != EnvironmentType::Procedural &&
        renderer.get_settings().environment_type != EnvironmentType::Hdri) {
        if (ImGui::ColorEdit3("Sun Color", &sun_color[0])) {
            renderer.get_settings().sun_color = sun_color;
            renderer.update_settings();
        }
    }
    if (ImGui::DragFloat("Sun Emission", &sun_emission, 1.0f, 0.0f, 100'000.0f)) {
        renderer.get_settings().sun_emission = sun_emission;
        renderer.update_settings();
    }
}

// r = 0, y = 1 (zenith)
// r = 1, y = 0 (equator)
// r = 2, y = -1 (nadir)
float disk_r_from_sphere_y(const float y) {
    float abs_y = std::abs(y);
    float k = 0.5f * std::sqrt(1.0f - abs_y * abs_y);
    if (k < 1e-10f) return 0.0f;
    return (-abs_y + std::sqrt(abs_y * abs_y + 4.0f * k * k)) / (2.0f * k);
}

glm::vec2 sphere_to_disk(const glm::vec3 pos) {
    float disk_r = disk_r_from_sphere_y(pos.y);
    float dual_disk_r = pos.y < 0.0f ? 2.0f - disk_r : disk_r;
    glm::vec2 xz_plane = {pos.x, pos.z};
    float xz_len = glm::length(xz_plane);
    if (xz_len < 1e-10f) {
        return {dual_disk_r, 0.0f};
    }
    return xz_plane / xz_len * dual_disk_r;
}

glm::vec3 disk_to_sphere(const glm::vec2 pos) {
    float dual_disk_r = glm::length(pos);
    bool is_lower = dual_disk_r > 1.0f;
    float disk_r = is_lower ? 2.0f - dual_disk_r : dual_disk_r;
    glm::vec2 xz = dual_disk_r > 1e-10f ? pos * (disk_r / dual_disk_r) : glm::vec2(0.0f);
    float sign = is_lower ? -1.0f : 1.0f;
    float y = 0.5f * sign * (1.0f - disk_r * disk_r);
    return glm::normalize(glm::vec3(xz.x, y, xz.y));
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
        bool show_gui = false;

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
        scene.build_light_buffer(ctx);
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

            const auto mouse_delta = Input::get_mouse_delta();

            if (mouse_grab) {
                constexpr float sensitivity = 0.005f;
                camera.set_rot(camera.get_rot() - glm::vec2(mouse_delta.y, mouse_delta.x) * sensitivity);

                const float s = speed * delta;

                if (Input::key_down(GLFW_KEY_W)) camera.move_z(-s);
                if (Input::key_down(GLFW_KEY_A)) camera.move_x(-s);
                if (Input::key_down(GLFW_KEY_S)) camera.move_z(s);
                if (Input::key_down(GLFW_KEY_D)) camera.move_x(s);
                if (Input::key_down(GLFW_KEY_SPACE)) camera.move_y(s);
                if (Input::key_down(GLFW_KEY_LEFT_SHIFT)) camera.move_y(-s);
            } else {
                bool mouse_moved = mouse_delta.x != 0.0f || mouse_delta.y != 0.0f;
                if (Input::mouse_button_down(GLFW_MOUSE_BUTTON_LEFT) && !Gui::is_cursor_captured() && mouse_moved) {
                    constexpr float sensitivity = 0.2f;

                    float delta_yaw = mouse_delta.x / static_cast<float>(Window::get_width()) * glm::two_pi<float>();
                    float delta_pitch = mouse_delta.y / static_cast<float>(Window::get_height()) * glm::pi<float>();

                    glm::vec3 world_delta = glm::angleAxis(camera.get_rot().y, glm::vec3(0.0f, 1.0f, 0.0f))
                                            * glm::vec3(-delta_yaw, 0.0f, -delta_pitch);
                    glm::vec2 disk_delta = {world_delta.x, world_delta.z};

                    glm::vec2 disk_pos = sphere_to_disk(renderer.sun_dir);
                    float disk_r = glm::length(disk_pos);

                    glm::vec2 radial_dir = disk_r > 1e-10f ? disk_pos / disk_r : glm::vec2(1.0f, 0.0f);
                    float rotation = (disk_delta.x * radial_dir.y - disk_delta.y * radial_dir.x)
                                     * glm::smoothstep(1.2f, 1.5f, disk_r);

                    disk_pos += disk_delta * sensitivity;

                    float s = std::sin(rotation);
                    float c = std::cos(rotation);
                    disk_pos = {c * disk_pos.x - s * disk_pos.y, s * disk_pos.x + c * disk_pos.y};

                    disk_r = glm::length(disk_pos);
                    if (disk_r > 2.0f) {
                        disk_pos *= -(4.0f - disk_r) / disk_r;
                    }

                    renderer.sun_dir = disk_to_sphere(disk_pos);

                    renderer.reset_frames();
                }
            }

            static float slow_delta = 0.0f;

            if (Input::key_released(GLFW_KEY_TAB)) {
                show_gui = !show_gui;
            }

            if (Input::key_released(GLFW_KEY_R)) {
                renderer.reload_shaders();
            }

            Gui::begin();
            if (show_gui) {
                ImGui::ShowDemoWindow();

                ImGui::Begin("HWRT");

                ImGui::Text("Frametime: %.2f ms (%.0f FPS)", slow_delta * 1000.0f, 1.0f / slow_delta);
                ImGui::Text("Accumulated Frames: %u", std::min(renderer.get_frame_count(), renderer.get_settings().iterations));

                const char* debug_channel_items[] = {
                    "None",
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
                    "Heatmap"
                };
                static int debug_item_idx = static_cast<int>(renderer.get_settings().debug_channel);
                if (ImGui::Combo("Debug Channel", &debug_item_idx, debug_channel_items, IM_ARRAYSIZE(debug_channel_items))) {
                    renderer.get_settings().debug_channel = static_cast<DebugChannel>(debug_item_idx);
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
                const char* sampling_strategy_items[] = {
                    "Uniform Sampling",
                    "Importance Sampling",
                    "Next Event Estimation",
                    "Multiple Importance Sampling"
                };
                static int sampling_strategy_idx = static_cast<int>(renderer.get_settings().sampling_strategy);
                if (ImGui::Combo("Sampling Strategy", &sampling_strategy_idx, sampling_strategy_items,
                                 IM_ARRAYSIZE(sampling_strategy_items))) {
                    renderer.get_settings().sampling_strategy = static_cast<SamplingStrategy>(sampling_strategy_idx);
                    renderer.update_settings();
                }
                const char* environment_type_items[] = {
                    "None",
                    "Solid",
                    "HDRI",
                    "Procedural"
                };
                static int environment_type_idx = static_cast<int>(renderer.get_settings().environment_type);
                if (ImGui::Combo("Environment Type", &environment_type_idx, environment_type_items,
                                 IM_ARRAYSIZE(environment_type_items))) {
                    renderer.get_settings().environment_type = static_cast<EnvironmentType>(environment_type_idx);
                    renderer.update_settings();
                }
                switch (renderer.get_settings().environment_type) {
                    case EnvironmentType::None: break;
                    case EnvironmentType::Solid: show_solid_sky_settings(renderer);
                        break;
                    case EnvironmentType::Hdri: break;
                    case EnvironmentType::Procedural: break;
                }
                static bool enable_sun = true;
                if (ImGui::Checkbox("Enable Sun", &enable_sun)) {
                    renderer.get_settings().sun = static_cast<Sun>(enable_sun);
                    renderer.update_settings();
                }
                if (enable_sun) {
                    show_sun_settings(renderer);
                }
                int fov = static_cast<int>(camera.get_fov());
                if (ImGui::SliderInt("Field Of View", &fov, 30, 110)) {
                    camera.set_fov(static_cast<float>(fov));
                    renderer.update_settings();
                }
                if (ImGui::Button("Reload Shaders (R)")) {
                    renderer.reload_shaders();
                }
                ImGui::End();

                //show_scene_graph(scene);
            }
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