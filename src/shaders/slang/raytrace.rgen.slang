#include "common.h"

[[vk::push_constant]]
PushData push_data;

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

// Source: https://www.shadertoy.com/view/XtGGzG
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

// Source: https://www.shadertoy.com/view/XtGGzG
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

// source: https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint pcg_hash(uint seed) {
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random(inout uint seed) {
    seed = pcg_hash(seed);
    return float(seed) * (1.0 / 4294967296.0);
}

float3 random_cosine_weighted_hemisphere(float3 normal, inout uint seed) {
    float a = random(seed) * 2.0 * PI;
    float z = random(seed) * 2.0 - 1.0;
    float r = sqrt(1.0 - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return normalize(normal + float3(x, y, z));
}

void update_seed(inout uint seed, uint frame) {
    seed = pcg_hash(uint(DispatchRaysIndex().x));
    seed = pcg_hash(seed + uint(DispatchRaysIndex().y));
    seed = pcg_hash(seed + uint(frame * 1000.0));
}

float3 heatmap(RayDesc ray, float2 range) {
    Payload payload;
    uint64_t start = clockARB();
    TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
    uint64_t end = clockARB();

    uint64_t diff_raw = (end > start) ? (end - start) : 0;
    float diff = float(diff_raw);
    return inferno_quintic(diff, range);
}

[shader("raygeneration")]
void main() {
    uint seed = 0;
    update_seed(seed, uint(push_data.frame_count));

    uint2 launch_id = DispatchRaysIndex().xy; // Current pixel coords (x,y)
    uint2 launch_size = DispatchRaysDimensions().xy; // Image size (width, height)

    float2 uv = (float2(launch_id) + 0.5) / float2(launch_size) * 2.0 - 1.0; // NDC in range [-1, 1]

    uv.x += (random(seed) - 0.5) * 0.002;
    uv.y += (random(seed) - 0.5) * 0.002;

    float4 origin = mul(uniform.inv_view, float4(0.0, 0.0, 0.0, 1.0)); // Camera position
    float4 target = mul(uniform.inv_proj, float4(uv, 1.0, 1.0)); // View space target
    float4 direction = mul(uniform.inv_view, float4(normalize(target.xyz), 0.0)); // World space direction

    RayDesc ray;
    ray.Origin = origin.xyz;
    ray.Direction = direction.xyz;
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    // TODO: чекать это tmax вместо -1

    float3 radiance = float3(0.0);
    float3 energy = float3(1.0);

    RenderSettings render_settings = push_data.render_settings[0];

    if (render_settings.debug_channel == DebugChannel.None) {
        for (uint depth = 0; depth < 4; ++depth) {
            Payload payload;
            payload.depth = -1.0;

            TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

            radiance += energy * payload.emission;

            if (any(payload.emission > 0.0) || payload.depth == -1.0) break;

            energy *= payload.color;

            float3 hitpos = ray.Origin + ray.Direction * payload.depth;

            //if (dot(ray.Direction, payload.normal) > 0.0) {
            //    payload.normal = -payload.normal;
            //}

            ray.Origin = hitpos + (payload.normal * EPSILON);
            ray.Direction = random_cosine_weighted_hemisphere(payload.normal, seed);
        }
    } else if (render_settings.debug_channel == DebugChannel.Heatmap) {
        radiance = heatmap(ray, float2(10000.0, 100000.0));
    } else {
        Payload payload;
        payload.color = float3(0.0);
        TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
        radiance = payload.color;
    }

    float3 previous_radiance = out_image[int2(launch_id)].rgb;

    float3 accumulated_radiance = radiance;
    if (push_data.frame_count != 1) {
        accumulated_radiance = lerp(previous_radiance, radiance, 1.0 / float(push_data.frame_count));
    }

    out_image[int2(launch_id)] = float4(accumulated_radiance, 1.0);
}