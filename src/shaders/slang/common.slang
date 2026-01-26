struct Payload {
    float3 color;
};

struct Vertex {
    float3 position;
    float3 normal;
    float3 tangent;
    float3 texcoord;
};

struct Mesh {
    Vertex* vertices;
    uint32_t* indices;
    uint32_t first_index;
    uint32_t material_index;
};

struct PushConstants {
    Mesh* meshes;
};

// uint meshID = InstanceID() (variable from tlas struct)
// uint primID = PrimitiveIndex() (triangle id from blas)