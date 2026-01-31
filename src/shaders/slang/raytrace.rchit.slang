import common;

[[vk::binding(3, 0)]]
StructuredBuffer<Mesh, ScalarDataLayout> meshes;

[shader("closesthit")]
void main(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr) {
    uint meshID = InstanceID(); // (variable from tlas struct)
    uint primID = PrimitiveIndex(); // (triangle id from blas)

    Mesh mesh = meshes[meshID];

    uint32_t i0 = mesh.indices[primID * 3 + 0];
    uint32_t i1 = mesh.indices[primID * 3 + 1];
    uint32_t i2 = mesh.indices[primID * 3 + 2];

    Vertex v0 = mesh.vertices[i0];
    Vertex v1 = mesh.vertices[i1];
    Vertex v2 = mesh.vertices[i2];

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

    payload.color = normal;
}