# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

HWRT is a progressive, physically based path tracer in C++20 using the Vulkan 1.4 hardware ray tracing pipeline (vulkan-hpp RAII). Shaders are written in **Slang**. You need a GPU with RT support (RTX 20+/RDNA 2+). Dependencies are git submodules under `external/` (glfw, glm, spdlog, VMA, fastgltf, imgui, stb, mikktspace). Clone with `--recursive`, or run `git submodule update --init --recursive`.

## Build & Run

```bash
./src/shaders/compile.sh                 # Slang -> SPIR-V; needs `slangc` on PATH. Run after editing any shader.
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/hwrt -m assets/models/sponza.glb   # -m can be repeated; -v enables validation layers
```

- Run the binary from a build directory that sits directly under the repo root (`build/`, `cmake-build-*/`). At runtime it finds `src/shaders/spirv/*.spv` and `assets/fonts/` as `<exe dir>/../...` (see `utils::get_exec_path()` usages in `renderer.cpp` and `gui.cpp`).
- The compiled SPIR-V in `src/shaders/spirv/` is committed. CI does not compile shaders, so after a shader change, recompile and commit the `.spv` files along with the `.slang` sources.
- There are no tests. CI (`.github/workflows/cmake-multi-platform.yml`) only builds on Ubuntu (gcc/clang) and Windows (MSVC).
- Code style is in `.clang-format` (Google-based, 4-space indent, 130 column limit).

In-app controls: right-click toggles mouse-look (WASD/Space/Shift to move, scroll changes speed), Tab toggles the ImGui panel, R hot-reloads shaders, and left-drag (without mouse grab) moves the sun.

## Architecture

**Vulkan abstraction layer (`src/vulkan/`)**: thin RAII wrappers (Instance, Adapter, Device, Allocator/VMA, Buffer, Image, Encoder for command buffers, pipeline builders, AccelerationStructure, ShaderBindingTable, Swapchain). `Context` (`src/context.h`) bundles instance, adapter, device, allocator, samplers, and the **bindless texture descriptor set layout** (`MAX_TEXTURES` = 1024). Most APIs take a `Context&`.

**Shared CPU/GPU types (`src/shaders/slang/common.h`)**: this header is included by both the C++ code (CMake adds it to the include path) and the Slang shaders. It defines `Vertex`, `Material`, `Geometry`, `Light`, `RenderSettings`, `PushData`, and the debug/sampling/environment enums. The `P(T)` macro becomes `uint64_t` in C++ and `T*` in Slang, because the GPU reads scene data through **buffer device addresses** (`ScenePtrs`), not descriptors. Shaders compile with `-fvk-use-scalar-layout`, so struct layouts must match C++ layout exactly. When you add a field or enum value, update both sides. That includes the ImGui combo string arrays in `main.cpp`, which index the enums directly.

**Scene pipeline (`scene.cpp`)**: `AssetManager` loads and caches glTF `Model`s through fastgltf (`model.cpp`). Tangents are generated with MikkTSpace in `tangent.cpp`. `Scene::add_instance` appends each model's vertices, indices, materials, and geometries into global flat arrays; a model used by several instances is only appended once (`model_cache`). After that, `main.cpp` calls these in order: `build_blases` (one BLAS per mesh) → `build_tlas` → `build_light_buffer` (collects emissive triangles for NEE) → `build_descriptor_set` (bindless textures). The scene is static once it's built.

**Rendering (`renderer.cpp`, `frame.cpp`)**: each frame, the ray tracing pipeline (`raytrace.rgen/rmiss/rchit/rahit`) writes HDR radiance to `rt_image`, accumulating progressively while `frame_count` grows. Then a compute pass (`compute.slang`) applies exposure and Khronos PBR Neutral tone mapping into `out_image`, which is blitted to the swapchain, and ImGui is drawn on top. The `FrameManager` manages frames in flight and owns every object that is indexed by frame in flight (fences, semaphores, per-frame uniform and `RenderSettings` buffers). Put new per-frame-in-flight resources there instead of in `Resources`/`Renderer`. Any change that affects the image (camera, settings, sun) must call `renderer.reset_frames()` to restart accumulation.

**Shaders**: the path tracing loop lives in `raytrace.rgen.slang` (sampling strategies Uniform/BRDF importance/NEE/MIS, Cook-Torrance BRDF, debug channels). The closest-hit shader fetches material and geometry data and fills `Payload`. The any-hit shader handles alpha masking. The SBT has two hit groups: opaque, and rchit+rahit. `atmosphere.slang` is the procedural sky. Descriptor set 0 holds the per-renderer resources (TLAS, output image, uniform); set 1 holds the bindless textures.

**Auto exposure**: computed entirely on the GPU, no CPU readback. `compute.slang` (tone mapping) measures the log2 luminance of each pixel and accumulates it into the persistent `ExposureState` buffer (`Resources::exposure_buffer`). Right after it, `autoexposure.slang` (a single thread) turns that into a smoothed EV and clears the accumulators, and the next frame's tone mapping applies `exp2(ev)`. Debug channels are never measured or exposed. Settings (`autoexposure`, compensation, speed, EV limits, `delta_time`) live in `RenderSettings`.

**Conventions**: use the project's own `Timer` (`src/timer.h`: `tick()` / `get_delta()`) for frame deltas instead of `glfwGetTime`/`std::chrono`.
