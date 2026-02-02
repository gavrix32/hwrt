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
    uint32_t albedo_index;
    float4 base_color_factor;

    uint32_t normal_index;
    float normal_scale;

    uint32_t metallic_roughness_index;
    float metallic_factor;
    float roughness_factor;

    uint32_t emissive_index;
    float3 emissive_factor;
};

struct Mesh {
    Vertex* vertices;
    uint32_t* indices;
    Material* material;
};