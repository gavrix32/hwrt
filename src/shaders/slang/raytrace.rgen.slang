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

float3 heatmap(RayDesc ray, float2 range) {
    Payload payload;
    uint64_t start = clockARB();
    TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
    uint64_t end = clockARB();

    uint64_t diff_raw = (end > start) ? (end - start) : 0;
    float diff = float(diff_raw);
    return inferno_quintic(diff, range);
}

float3 get_accumulated_color(float3 color) {
    if (push_data.frame_count == 1) {
        return color;
    }

    uint2 launch_id = DispatchRaysIndex().xy;

    float3 previous_color = out_image[int2(launch_id)].rgb;
    float3 accumulated_color = color;

    accumulated_color = lerp(previous_color, color, 1.0 / float(push_data.frame_count));

    return accumulated_color;
}

float3 get_debug_color(RayDesc ray) {
    float3 color = float3(0.0);

    if (push_data.render_settings[0].debug_channel == DebugChannel::Heatmap) {
        color = heatmap(ray, float2(10000.0, 100000.0));
    } else {
        Payload payload = {};
        payload.color = float3(0.0);

        TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
        color = payload.color;
    }

    return color;
}

// source: https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
static uint rng_state;

uint rand_pcg() {
    uint state = rng_state;
    rng_state = rng_state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random() {
    uint seed = rand_pcg();
    return float(seed) / 4294967296.0;
}

float3 sample_uniform_hemisphere() {
    float z = random();
    float r = sqrt(1.0 - z * z);
    float phi = 2.0 * PI * random();

    float x = r * cos(phi);
    float y = r * sin(phi);

    return float3(x, y, z);
}

float3 sample_cosine_hemisphere() {
    float r1 = random();
    float r2 = random();

    float phi = 2 * PI * r1;
    float x = cos(phi) * sqrt(r2);
    float y = sin(phi) * sqrt(r2);
    float z = sqrt(1 - r2);

    return float3(x, y, z);
}

float3 local_to_world(float3 local, float3 normal) {
    float3 up = abs(normal.z) < 1.0 - EPSILON ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);

    float3 world = local.x * tangent + local.y * bitangent + local.z * normal;
    return normalize(world);
}

// float3 get_sky_color(float3 dir) {
//     dir = normalize(dir);

//     float transition = pow(max(0.0, dir.y), 0.5); // Плавность перехода

//     float3 horizon_color = float3(0.4, 0.6, 0.8);  // Нежно-голубой у горизонта
//     float3 zenith_color = float3(0.05, 0.15, 0.4); // Глубокий синий в зените
//     float3 ground_color = float3(0.1, 0.1, 0.1);   // Почти черный низ

//     if (dir.y > 0) {
//         return lerp(horizon_color, zenith_color, transition);
//     } else {
//         return lerp(horizon_color, ground_color, pow(max(0.0, -dir.y), 0.2));
//     }
// }

float3 evaluate_brdf(float3 base_color) {
    return base_color / PI;
}

float evaluate_uniform_pdf() {
    return 1.0 / (2.0 * PI);
}

float evaluate_cosine_pdf(float cos_theta) {
    return cos_theta / PI;
}

float evaluate_light_pdf(Light light, float3 to_light) {
    float distance_squared = dot(to_light, to_light);
    float distance = sqrt(distance_squared);
    to_light = normalize(to_light);

    float3 light_normal = normalize(cross(light.v1 - light.v0, light.v2 - light.v0));
    float cos_theta = max(dot(light_normal, -to_light), 0.00001);

    float pdf = distance_squared / (cos_theta * light.area);
    pdf /= push_data.num_lights;

    return pdf;
}

Light random_light() {
    uint light_idx = uint(random() * push_data.num_lights);
    return push_data.scene_ptrs.lights[light_idx];
}

float3 sample_light(Light light) {
    float u = random();
    float v = random();
    if (u + v > 1.0) { u = 1.0 - u; v = 1.0 - v; }
    float w = 1.0 - u - v;

    return u * light.v0 + v * light.v1 + w * light.v2;
}

float power_heuristic(float pdf_a, float pdf_b) {
    float a = pdf_a * pdf_a;
    float b = pdf_b * pdf_b;
    return a / (a + b);
}

float3 path_trace_uniform(RayDesc ray, int max_depth) {
    float3 radiance = float3(0.0);
    float3 throughput = float3(1.0);

    for (int depth = 0; depth < max_depth; ++depth) {
        Payload payload = {};
        TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

        if (payload.depth == T_MAX) {
            break;
        }

        if (any(payload.emission > 0.0) && dot(payload.normal, -ray.Direction) > 0.0) {
            radiance += throughput * payload.emission;
            break;
        }

        if (dot(payload.normal, ray.Direction) > 0.0) {
            payload.normal = -payload.normal;
        }

        float3 hitpos = ray.Origin + ray.Direction * payload.depth;

        float3 next_dir = local_to_world(sample_uniform_hemisphere(), payload.normal);
        float3 brdf = evaluate_brdf(payload.color);
        float lambert = dot(payload.normal, next_dir);
        float pdf = evaluate_uniform_pdf();

        throughput *= brdf * lambert / pdf;

        ray.Origin = hitpos + (payload.normal * EPSILON);
        ray.Direction = next_dir;
    }
    return radiance;
}

float3 path_trace_importance(RayDesc ray, int max_depth) {
    float3 radiance = float3(0.0);
    float3 throughput = float3(1.0);

    for (int depth = 0; depth < max_depth; ++depth) {
        Payload payload = {};
        TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

        if (payload.depth == T_MAX) {
            break;
        }

        if (any(payload.emission > 0.0) && dot(payload.normal, -ray.Direction) > 0.0) {
            radiance += throughput * payload.emission;
            break;
        }

        if (dot(payload.normal, ray.Direction) > 0.0) {
            payload.normal = -payload.normal;
        }

        float3 hitpos = ray.Origin + ray.Direction * payload.depth;

        float3 next_dir = local_to_world(sample_cosine_hemisphere(), payload.normal);
        float3 brdf = evaluate_brdf(payload.color);
        float lambert = dot(payload.normal, next_dir);
        float pdf = evaluate_cosine_pdf(lambert);

        throughput *= brdf * lambert / pdf;

        ray.Origin = hitpos + (payload.normal * EPSILON);
        ray.Direction = next_dir;
    }
    return radiance;
}

float3 path_trace_nee(RayDesc ray, int max_depth) {
    float3 radiance = float3(0.0);
    float3 throughput = float3(1.0);

    for (int depth = 0; depth < max_depth; ++depth) {
        Payload payload = {};
        TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

        if (payload.depth == T_MAX) {
            break;
        }

        if (any(payload.emission > 0.0) && dot(payload.normal, -ray.Direction) > 0.0) {
            if (depth == 0) {
                radiance += throughput * payload.emission;
            }
            break;
        }

        if (dot(ray.Direction, payload.normal) > 0.0) {
            payload.normal = -payload.normal;
        }

        float3 hitpos = ray.Origin + ray.Direction * payload.depth;

        Light light = random_light();
        float3 light_sample = sample_light(light);
        float3 to_light = light_sample - hitpos;
        float distance_to_light = length(to_light);
        float3 light_dir = normalize(to_light);
        float light_lambert = dot(payload.normal, light_dir);

        if (light_lambert > 0.0) {
            RayDesc shadow_ray = {};
            shadow_ray.Origin = hitpos + payload.normal * 0.00001;
            shadow_ray.Direction = light_dir;
            shadow_ray.TMin = T_MIN;
            shadow_ray.TMax = distance_to_light - EPSILON;

            Payload shadow_payload = {};
            TraceRay(tlas, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                0xFF, 0, 0, 0, shadow_ray, shadow_payload);

            bool is_visible = shadow_payload.depth >= distance_to_light - EPSILON;

            if (is_visible) {
                float3 brdf = evaluate_brdf(payload.color);
                float pdf = evaluate_light_pdf(light, to_light);
                if (pdf < 0.00001) continue;

                radiance += throughput * brdf * light.emission * light_lambert / pdf;
            }
        }

        /// Sun

        // float3 sun_dir = normalize(float3(-1.4, 5.0, -4.9));
        // float3 sun_emission = float3(10.0, 10.0, 7.0);
        // float r_max = 0.05;

        // float sun_lambert = dot(payload.normal, sun_dir);
        // if (sun_lambert > 0.0) {
        //     float2 rand_sample = float2(random(), random());
        //     float phi = 2.0 * PI * rand_sample.y;
        //     float r = r_max * sqrt(rand_sample.x);
        //     float2 cp = float2(cos(phi), sin(phi)) * r;

        //     float3 swup = abs(sun_dir.z) < 0.999 ? float3(0,0,1) : float3(1,0,0);
        //     float3 right = normalize(cross(swup, sun_dir));
        //     float3 up = cross(sun_dir, right);

        //     float3 sampled_dir = normalize(sun_dir + right * cp.x + up * cp.y);

        //     RayDesc shadow_ray = {};
        //     shadow_ray.Origin = hitpos + payload.normal * 0.00001;
        //     shadow_ray.Direction = sampled_dir;
        //     shadow_ray.TMin = T_MIN;
        //     shadow_ray.TMax = T_MAX;

        //     Payload shadow_payload = {};
        //     TraceRay(tlas, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
        //         0xFF, 0, 0, 0, shadow_ray, shadow_payload);

        //     if (shadow_payload.depth == T_MAX) {
        //         float3 brdf = evaluate_brdf(payload.color);
        //         radiance += throughput * brdf * sun_emission * sun_lambert;
        //     }
        // }

        ///

        float3 next_dir = local_to_world(sample_cosine_hemisphere(), payload.normal);
        float3 brdf = evaluate_brdf(payload.color);
        float lambert = dot(next_dir, payload.normal);
        float pdf = evaluate_cosine_pdf(lambert);
        if (pdf < 0.00001) break;

        throughput *= brdf * lambert / pdf;

        ray.Origin = hitpos + (payload.normal * EPSILON);
        ray.Direction = next_dir;
    }

    return radiance;
}

float3 path_trace_mis(RayDesc ray, int max_depth) {
    float3 radiance = float3(0.0);
    float3 throughput = float3(1.0);
    float last_pdf = 1.0;

    for (int depth = 0; depth < max_depth; ++depth) {
        Payload payload = {};
        TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

        if (payload.depth == T_MAX) {
            break;
        }

        float3 hitpos = ray.Origin + ray.Direction * payload.depth;

        float dir_lambert = dot(ray.Direction, payload.normal);

        if (any(payload.emission > 0.0) && dot(payload.normal, -ray.Direction) > 0.0) {
            if (depth == 0) {
                radiance += throughput * payload.emission;
            } else {
                float distance_squared = payload.depth * payload.depth;
                float cos_theta = max(dot(payload.normal, -ray.Direction), 0.00001);

                float light_pdf = distance_squared / (cos_theta * payload.light_area);
                light_pdf /= push_data.num_lights;

                float mis_weight = power_heuristic(last_pdf, light_pdf);

                radiance += throughput * payload.emission * mis_weight;
            }
            break;
        }

        if (dir_lambert > 0.0) {
            payload.normal = -payload.normal;
        }

        Light light = random_light();
        float3 light_sample = sample_light(light);
        float3 to_light = light_sample - hitpos;
        float distance_to_light = length(to_light);
        float3 light_dir = normalize(to_light);
        float light_lambert = dot(payload.normal, light_dir);

        if (light_lambert > 0.0) {
            RayDesc shadow_ray = {};
            shadow_ray.Origin = hitpos + payload.normal * 0.00001;
            shadow_ray.Direction = light_dir;
            shadow_ray.TMin = T_MIN;
            shadow_ray.TMax = distance_to_light - EPSILON;

            Payload shadow_payload = {};
            TraceRay(tlas, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                0xFF, 0, 0, 0, shadow_ray, shadow_payload);

            bool is_visible = shadow_payload.depth >= distance_to_light - EPSILON;

            if (is_visible) {
                float3 brdf = evaluate_brdf(payload.color);
                float light_pdf = evaluate_light_pdf(light, to_light);
                float cosine_pdf = evaluate_cosine_pdf(light_lambert);
                float mis_weight = power_heuristic(light_pdf, cosine_pdf);

                radiance += throughput * brdf * light.emission * light_lambert * mis_weight / light_pdf;
            }
        }

        float3 next_dir = local_to_world(sample_cosine_hemisphere(), payload.normal);
        float3 brdf = evaluate_brdf(payload.color);
        float lambert = dot(next_dir, payload.normal);
        float pdf = evaluate_cosine_pdf(lambert);
        if (pdf < 0.00001) break;

        throughput *= brdf * lambert / pdf;
        last_pdf = pdf;

        ray.Origin = hitpos + (payload.normal * EPSILON);
        ray.Direction = next_dir;
    }

    return radiance;
}

[shader("raygeneration")]
void main() {
    uint2 launch_id = DispatchRaysIndex().xy; // Current pixel coords (x,y)
    uint2 launch_size = DispatchRaysDimensions().xy; // Image size (width, height)

    uint pixel_index = launch_id.y * launch_size.x + launch_id.x;
    rng_state = pixel_index + push_data.frame_count * 1_000_000u;

    float2 uv = (float2(launch_id) + 0.5) / float2(launch_size) * 2.0 - 1.0; // NDC in range [-1, 1]

    // Anti-Aliasing
    uv.x += (random() - 0.5) * 0.002;
    uv.y += (random() - 0.5) * 0.002;

    RenderSettings render_settings = push_data.render_settings[0];

    if (push_data.frame_count > render_settings.iterations) {
        return;
    }

    float4 origin = mul(uniform.inv_view, float4(0.0, 0.0, 0.0, 1.0)); // Camera position
    float4 target = mul(uniform.inv_proj, float4(uv, 1.0, 1.0)); // View space target
    float4 direction = mul(uniform.inv_view, float4(normalize(target.xyz), 0.0)); // World space direction

    RayDesc ray = {};
    ray.Origin = origin.xyz;
    ray.Direction = direction.xyz;
    ray.TMin = T_MIN;
    ray.TMax = T_MAX;

    if (render_settings.debug_channel != DebugChannel::None) {
        float3 debug_color = get_debug_color(ray);
        out_image[int2(launch_id)] = float4(get_accumulated_color(debug_color), 1.0);
        return;
    }

    float3 radiance = float3(0.0);

    for (int sample = 0; sample < render_settings.samples; ++sample) {
        uint max_depth = render_settings.max_depth;
        switch (render_settings.sampling_strategy) {
            case SamplingStrategy.UniformSampling: radiance += path_trace_uniform(ray, max_depth); break;
            case SamplingStrategy.ImportanceSampling: radiance += path_trace_importance(ray, max_depth); break;
            case SamplingStrategy.NextEventEstimation: radiance += path_trace_nee(ray, max_depth); break;
            case SamplingStrategy.MultipleImportanceSampling: radiance += path_trace_mis(ray, max_depth); break;
        }
    }
    radiance /= render_settings.samples;

    out_image[int2(launch_id)] = float4(get_accumulated_color(radiance), 1.0);
}