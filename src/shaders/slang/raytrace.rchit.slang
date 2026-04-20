#include "common.h"

[[vk::push_constant]]
PushData push_data;

[vk::binding(0, 1)]
Sampler2D textures[];

struct Surface {
    float3 geometry_normal;
    float3 geometry_tangent;
    float3 geometry_bitangent;
    float tangent_w;
    float2 uv;
    float3 albedo;
    float3 local_normal;
    float3 shading_normal;
    float alpha;
    float metallic;
    float roughness;
    float3 emissive;
    float light_area;
};

Surface get_surface(ScenePtrs scene, float3 bary) {
    Surface s;

    Geometry geometry = scene.geometries[InstanceID() + GeometryIndex()];
    uint32_t base = geometry.index_offset + PrimitiveIndex() * 3;

    Vertex v0 = scene.vertices[geometry.vertex_offset + scene.indices[base + 0]];
    Vertex v1 = scene.vertices[geometry.vertex_offset + scene.indices[base + 1]];
    Vertex v2 = scene.vertices[geometry.vertex_offset + scene.indices[base + 2]];

    Material material = scene.materials[geometry.material_index];

    float3 geometry_normal = v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z;
    geometry_normal = mul(geometry_normal, float3x3(WorldToObject3x4())).xyz;
    geometry_normal = normalize(geometry_normal);
    s.geometry_normal = geometry_normal;

    float3 geometry_tangent = v0.tangent.xyz * bary.x + v1.tangent.xyz * bary.y + v2.tangent.xyz * bary.z;
    geometry_tangent = mul(ObjectToWorld3x4(), float4(geometry_tangent, 0.0)).xyz;
    geometry_tangent = normalize(geometry_tangent);

    // Gram-Schmidt re-orthogonalize
    float3 orthogonalized_tangent = normalize(geometry_tangent - dot(geometry_tangent, s.geometry_normal) * s.geometry_normal);
    float tangent_len = length(orthogonalized_tangent);

    if (tangent_len > 0.00001) {
        s.geometry_tangent = orthogonalized_tangent / tangent_len;
    } else {
        float3 up = abs(s.geometry_normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
        s.geometry_tangent = normalize(cross(up, s.geometry_normal));
    }

    s.tangent_w = v0.tangent.w;
    s.geometry_bitangent = cross(s.geometry_normal, s.geometry_tangent) * s.tangent_w;

    s.uv = v0.texcoord.xy * bary.x + v1.texcoord.xy * bary.y + v2.texcoord.xy * bary.z;

    if (material.albedo_index != UINT32_MAX) {
        float4 albedo = textures[NonUniformResourceIndex(material.albedo_index)].SampleLevel(s.uv, 0.0).rgba;
        s.albedo = albedo.rgb * material.base_color_factor.rgb;
        s.alpha = albedo.a * material.base_color_factor.a;
    } else {
        s.albedo = material.base_color_factor.rgb;
        s.alpha = material.base_color_factor.a;
    }

    if (material.normal_index != UINT32_MAX) {
        float3 raw_normal = textures[NonUniformResourceIndex(material.normal_index)].SampleLevel(s.uv, 0.0).rgb;
        s.local_normal = raw_normal * 2.0 - 1.0;
        s.local_normal.xy *= material.normal_scale;
        s.local_normal = normalize(s.local_normal);
    } else {
        s.local_normal = float3(0.0, 0.0, 1.0);
    }

    if (material.metallic_roughness_index != UINT32_MAX) {
        float3 metallic_roughness = textures[NonUniformResourceIndex(material.metallic_roughness_index)].SampleLevel(s.uv, 0.0).rgb;
        s.metallic = metallic_roughness.b * material.metallic_factor;
        s.roughness = metallic_roughness.g * material.roughness_factor;
    } else {
        s.metallic = material.metallic_factor;
        s.roughness = material.roughness_factor;
    }

    if (material.emissive_index != UINT32_MAX) {
        float3 emissive = textures[NonUniformResourceIndex(material.emissive_index)].SampleLevel(s.uv, 0.0).rgb;
        s.emissive = emissive * material.emissive_factor;
    } else {
        s.emissive = material.emissive_factor;
    }

    if (material.normal_index != UINT32_MAX) {
        s.shading_normal = normalize(
            s.local_normal.x * s.geometry_tangent +
            s.local_normal.y * s.geometry_bitangent +
            s.local_normal.z * s.geometry_normal
        );
    } else {
        s.shading_normal = s.geometry_normal;
    }

    if (any(s.emissive > 0.0)) {
        float3 w0 = mul(ObjectToWorld3x4(), float4(v0.position, 1.0)).xyz;
        float3 w1 = mul(ObjectToWorld3x4(), float4(v1.position, 1.0)).xyz;
        float3 w2 = mul(ObjectToWorld3x4(), float4(v2.position, 1.0)).xyz;

        s.light_area = 0.5 * length(cross(w1 - w0, w2 - w0));
    }

    return s;
}

[shader("closesthit")]
void main(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr) {
    ScenePtrs scene = push_data.scene_ptrs;
    float3 bary = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    Surface s = get_surface(scene, bary);

    float depth = RayTCurrent();
    float3 hitpos = WorldRayOrigin() + WorldRayDirection() * depth;

    RenderSettings render_settings = push_data.render_settings[0];

    switch (render_settings.debug_channel) {
        case DebugChannel.None: payload.color = s.albedo; break;
        case DebugChannel.Texcoord: payload.color = float3(s.uv, 0.0); break;
        case DebugChannel.Depth: payload.color = float3(depth, 0.0, 0.0); break;
        case DebugChannel.Hitpos: payload.color = hitpos; break;
        case DebugChannel.NormalTexture: payload.color = s.local_normal * 0.5 + 0.5; break;
        case DebugChannel.GeometryNormal: payload.color = s.geometry_normal * 0.5 + 0.5; break;
        case DebugChannel.GeometryTangent: payload.color = s.geometry_tangent * 0.5 + 0.5; break;
        case DebugChannel.GeometryBitangent: payload.color = s.geometry_bitangent * 0.5 + 0.5; break;
        case DebugChannel.GeometryTangentW: payload.color = float3(s.tangent_w); break;
        case DebugChannel.ShadingNormal: payload.color = s.shading_normal * 0.5 + 0.5; break;
        case DebugChannel.Alpha: payload.color = float3(s.alpha); break;
        case DebugChannel.Emissive: payload.color = s.emissive; break;
        case DebugChannel.BaseColor: payload.color = s.albedo; break;
        case DebugChannel.Metallic: payload.color = float3(s.metallic); break;
        case DebugChannel.Roughness: payload.color = float3(s.roughness); break;
        case DebugChannel.Heatmap: payload.color = float3(0.0); break;
    }

    payload.normal = s.shading_normal;
    payload.emission = s.emissive;
    payload.depth = RayTCurrent();
    payload.light_area = s.light_area;
}