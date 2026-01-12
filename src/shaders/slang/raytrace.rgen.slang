[[vk::binding(0, 0)]]
RaytracingAccelerationStructure tlas;

[[vk::binding(1, 0)]]
RWTexture2D<float4> out_image;

struct UniformBuffer {
    float4x4 inv_view;
    float4x4 inv_proj;
};

[vk::binding(2, 0)]
ConstantBuffer<UniformBuffer> uniform;

float3 viridis_quintic(float x, float2 range) {
	x = saturate((x - range.x) / (range.y - range.x));
	float4 x1 = float4( 1.0, x, x * x, x * x * x ); // 1 x x2 x3
	float4 x2 = x1 * x1.w * x; // x4 x5 x6 x7
	return saturate(float3(
	    dot(x1.xyzw, float4(+0.280268003, -0.143510503, +2.225793877, -14.815088879 )) + dot(x2.xy, float2(+25.212752309, -11.772589584)),
	    dot(x1.xyzw, float4(-0.002117546, +1.617109353, -1.909305070, +2.701152864  )) + dot(x2.xy, float2(-1.685288385,  +0.178738871)),
	    dot(x1.xyzw, float4(+0.300805501, +2.614650302, -12.019139090, +28.933559110)) + dot(x2.xy, float2(-33.491294770, +13.762053843)))
	);
}

float3 inferno_quintic(float x, float2 range) {
	x = saturate((x - range.x) / (range.y - range.x));
	float4 x1 = float4( 1.0, x, x * x, x * x * x ); // 1 x x2 x3
	float4 x2 = x1 * x1.w * x; // x4 x5 x6 x7
	return saturate(float3(
	    dot(x1.xyzw, float4(-0.027780558, +1.228188385, +0.278906882, +3.892783760)) + dot(x2.xy, float2(-8.490712758, +4.069046086)),
	    dot(x1.xyzw, float4(+0.014065206, +0.015360518, +1.605395918, -4.821108251)) + dot(x2.xy, float2(+8.389314011, -4.193858954)),
	    dot(x1.xyzw, float4(-0.019628385, +3.122510347, -5.893222355, +2.798380308)) + dot(x2.xy, float2(-3.608884658, +4.324996022)))
	);
}

[shader("raygeneration")]
void main() {
    uint2 launch_id = DispatchRaysIndex().xy; // Current pixel coords (x,y)
    uint2 launch_size = DispatchRaysDimensions().xy; // Image size (width, height)

    float2 uv = (float2(launch_id) + 0.5) / float2(launch_size) * 2.0 - 1.0; // NDC in range [-1, 1]

    float4 origin = mul(uniform.inv_view, float4(0.0, 0.0, 0.0, 1.0)); // Camera position
    float4 target = mul(uniform.inv_proj, float4(uv, 1.0, 1.0)); // View space target
    float4 direction = mul(uniform.inv_view, float4(normalize(target.xyz), 0.0)); // World space direction

    RayDesc ray;
    ray.Origin = origin.xyz;
    ray.Direction = direction.xyz;
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    float3 color = float3(0.0, 0.0, 0.0);

    uint64_t start = clockARB();
    TraceRay(tlas, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, color);
    uint64_t end = clockARB();

    float diff = 0.0;
    diff = float(end - start);

    //color = inferno_quintic(diff, float2(10000.0, 100000.0));

    out_image[int2(launch_id)] = float4(color, 1.0);
}