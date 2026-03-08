#include "common.h"

[[vk::push_constant]]
PushData push_data;

[vk::binding(0, 1)]
Sampler2D textures[];

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

    // alpha_mode == AlphaMode::Mask
    if (material.alpha_mode != 1) return;

    float3 bary = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    float2 uv = v0.texcoord * bary.x + v1.texcoord * bary.y + v2.texcoord * bary.z;

    float alpha = material.base_color_factor.a;
    if (material.albedo_index != UINT32_MAX) {
        alpha *= textures[NonUniformResourceIndex(material.albedo_index)].SampleLevel(uv, 0.0).a;
    }

    if (alpha < material.alpha_cutoff) {
        IgnoreHit();
    }
}