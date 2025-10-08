[[vk::binding(0, 0)]] RaytracingAccelerationStructure TLAS;
[[vk::binding(1, 0)]] RWTexture2D<float4> out_image;

[shader("raygeneration")]
void main() {
    float2 launch_id = (float2) DispatchRaysIndex().xy; // Current pixel coords (x,y)
    float2 launch_size = (float2) DispatchRaysDimensions().xy; // Image size (width, height)

    float2 uv = launch_id / launch_size * 2.0 - 1.0; // Normalized device coordinates (NDC) in range [-1, 1]

    RayDesc ray; // Primary ray
    ray.Origin = float3(0.0, 0.0, -3.0);
    ray.Direction = normalize(float3(uv.x, uv.y, 1.0));
    ray.TMin = 0.001;
    ray.TMax = 1000.0;

    float3 color = float3(0.0, 0.0, 0.0);

    TraceRay(TLAS, 0, 0xFF, 0, 0, 0, ray, color);
}