#pragma once

#ifdef __cplusplus
    #include <glm/glm.hpp>
    using float2 = glm::vec2;
    using float3 = glm::vec3;
    using float4 = glm::vec4;
    using float4x4 = glm::mat4;

    #define P(T) uint64_t
#else
    #define UINT32_MAX 0xFFFFFFFF
    #define P(T) T*
#endif

#define PI 3.14159
#define EPSILON 0.001
#define T_MIN 0.0
#define T_MAX 10000.0

struct Payload {
    float3 normal;
    float3 color;
    float3 emission;
    float depth;
    float light_area;
};

struct Vertex {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 texcoord;
};

enum class AlphaMode : uint32_t {
    Opaque = 0,
    Mask = 1,
    Blend = 2
};

struct Material {
    uint32_t albedo_index = UINT32_MAX;
    float4 base_color_factor;

    uint32_t normal_index = UINT32_MAX;
    float normal_scale;

    uint32_t metallic_roughness_index = UINT32_MAX;
    float metallic_factor;
    float roughness_factor;

    uint32_t emissive_index = UINT32_MAX;
    float3 emissive_factor;

    AlphaMode alpha_mode;
    float alpha_cutoff;
};

struct Geometry {
    uint32_t vertex_offset;
    uint32_t vertex_count;
    uint32_t index_offset;
    uint32_t index_count;
    uint32_t material_index;
};

struct Light {
    float3 emission;
    float3 v0;
    float3 v1;
    float3 v2;
    float area;
};

struct ScenePtrs {
    P(Vertex) vertices;
    P(uint32_t) indices;
    P(Material) materials;
    P(Geometry) geometries;
    P(Light) lights;
};

enum class DebugChannel : uint32_t {
    None = 0,
    Texcoord = 1,
    Depth = 2,
    Hitpos = 3,
    NormalTexture = 4,
    GeometryNormal = 5,
    GeometryTangent = 6,
    GeometryBitangent = 7,
    GeometryTangentW = 8,
    ShadingNormal = 9,
    Alpha = 10,
    Emissive = 11,
    BaseColor = 12,
    Metallic = 13,
    Roughness = 14,
    Heatmap = 15
};

enum class SamplingStrategy : uint32_t {
    UniformSampling = 0,
    ImportanceSampling = 1,
    NextEventEstimation = 2,
    MultipleImportanceSampling = 3
};

enum class EnvironmentType : uint32_t {
    None = 0,
    Solid = 1,
    Hdri = 2,
    Procedural = 3
};

enum class Sun : uint32_t {
    None = 0,
    Enabled = 1
};

struct RenderSettings {
    DebugChannel debug_channel;
    uint32_t samples;
    uint32_t max_depth;
    uint32_t iterations;
    SamplingStrategy sampling_strategy;
    EnvironmentType environment_type;
    float3 sky_color;
    float sky_emission;
    Sun sun;
    float3 sun_color;
    float sun_emission;
    float sun_radius;
};

struct PushData {
    ScenePtrs scene_ptrs;
    P(RenderSettings) render_settings;
    uint32_t frame_count;
    uint32_t num_lights;
    float3 sun_dir;
};