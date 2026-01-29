struct Payload {
    float3 color;
};

struct Vertex {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 texcoord;
};

struct Mesh {
    Vertex* vertices;
    uint32_t* indices;
};