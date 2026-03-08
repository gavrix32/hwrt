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

struct Payload {
    float3 color;
};

struct Vertex {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 texcoord;
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

    uint32_t alpha_mode;
    float alpha_cutoff;
};

struct Geometry {
    uint32_t vertex_offset;
    uint32_t vertex_count;
    uint32_t index_offset;
    uint32_t index_count;
    uint32_t material_index;
};

struct ScenePtrs {
    P(Vertex) vertices;
    P(uint32_t) indices;
    P(Material) materials;
    P(Geometry) geometries;
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

struct RenderSettings {
    DebugChannel debug_channel;
};

struct PushData {
    ScenePtrs scene_ptrs;
    P(RenderSettings) render_settings;
};