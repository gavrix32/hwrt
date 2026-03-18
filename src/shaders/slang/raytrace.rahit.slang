#include "common.h"

[[vk::push_constant]]
PushData push_data;

[vk::binding(0, 1)]
Sampler2D textures[];

// source: https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint pcg_hash(uint seed) {
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random(inout uint seed) {
    seed = pcg_hash(seed);
    return float(seed) * (1.0 / 4294967296.0);
}

void update_seed(inout uint seed, uint frame) {
    seed = pcg_hash(uint(DispatchRaysIndex().x));
    seed = pcg_hash(seed + uint(DispatchRaysIndex().y));
    seed = pcg_hash(seed + uint(frame * 1000.0));
}

[shader("anyhit")]
void main(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr) {
    RenderSettings render_settings = push_data.render_settings[0];

    if (render_settings.debug_channel != DebugChannel.None) {
        return;
    }

    ScenePtrs scene = push_data.scene_ptrs;

    Geometry geometry = scene.geometries[InstanceID() + GeometryIndex()];
    uint32_t base = geometry.index_offset + PrimitiveIndex() * 3;

    Vertex v0 = scene.vertices[geometry.vertex_offset + scene.indices[base + 0]];
    Vertex v1 = scene.vertices[geometry.vertex_offset + scene.indices[base + 1]];
    Vertex v2 = scene.vertices[geometry.vertex_offset + scene.indices[base + 2]];

    Material material = scene.materials[geometry.material_index];

    if (material.alpha_mode == AlphaMode.Opaque) return;

    float3 bary = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    float2 uv = v0.texcoord * bary.x + v1.texcoord * bary.y + v2.texcoord * bary.z;

    float alpha = material.base_color_factor.a;
    if (material.albedo_index != UINT32_MAX) {
        alpha *= textures[NonUniformResourceIndex(material.albedo_index)].SampleLevel(uv, 0.0).a;
    }

    // alpha_mode == AlphaMode::Mask
    float cutoff = material.alpha_cutoff;

    uint seed = 0;
    update_seed(seed, uint(push_data.frame_count));

    if (material.alpha_mode == AlphaMode.Blend) {
        cutoff = random(seed);
    }

    if (alpha < cutoff) {
        IgnoreHit();
    }
}