[[vk::binding(0, 0)]] RaytracingAccelerationStructure tlas;
[[vk::binding(1, 0)]] RWTexture2D<float4> out_image;

[shader("raygeneration")]
void main() {
    uint2 launch_id = DispatchRaysIndex().xy; // Current pixel coords (x,y)
    uint2 launch_size = DispatchRaysDimensions().xy; // Image size (width, height)

    float2 uv = (float2) launch_id / (float2) launch_size * 2.0 - 1.0; // NDC in range [-1, 1]

    RayDesc ray; // Primary ray
    ray.Origin = float3(0.0, 0.0, -1.0);
    ray.Direction = normalize(float3(uv.x, uv.y, 1.0));
    ray.TMin = 0.001;
    ray.TMax = 1000.0;

    float3 color = float3(0.0, 0.0, 0.0);

    TraceRay(tlas, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, color);

    out_image[int2(launch_id)] = float4(color, 1.0);
}