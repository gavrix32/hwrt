import common;

[[vk::push_constant]]
SceneAddresses scene;

[shader("closesthit")]
void main(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr) {
    Geometry geometry = scene.geometries[InstanceID() + GeometryIndex()];

    uint32_t base = geometry.index_offset + PrimitiveIndex() * 3;

    Vertex v0 = scene.vertices[geometry.vertex_offset + scene.indices[base + 0]];
    Vertex v1 = scene.vertices[geometry.vertex_offset + scene.indices[base + 1]];
    Vertex v2 = scene.vertices[geometry.vertex_offset + scene.indices[base + 2]];

    Material material = scene.material[geometry.material_index];

    float3 bary = float3(
        1.0 - attr.barycentrics.x - attr.barycentrics.y,
        attr.barycentrics.x,
        attr.barycentrics.y
    );

    float3 normal = v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z;
    normal = mul(ObjectToWorld3x4(), float4(normal, 0.0)).xyz;
    normal = normalize(normal);

    float3 tangent = v0.tangent.xyz * bary.x + v1.tangent.xyz * bary.y + v2.tangent.xyz * bary.z;
    tangent = mul(ObjectToWorld3x4(), float4(tangent, 0.0)).xyz;
    tangent = normalize(tangent);

    float2 uv = v0.texcoord.xy * bary.x + v1.texcoord.xy * bary.y + v2.texcoord.xy * bary.z;

    payload.color = normal * 0.5 + 0.5;
}