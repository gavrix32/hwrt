#include "common.h"
#include "atmosphere.slang"

// TODO: auto exposure
// TODO: light emission control
// TODO: firefly clamping

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

void orthonormal_basis(float3 v, out float3 up, out float3 tangent, out float3 bitangent) {
    up = abs(v.z) < 1.0 - EPSILON ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    tangent = normalize(cross(up, v));
    bitangent = cross(v, tangent);
}

float3 local_to_world(float3 local, float3 target) {
    float3 up, tangent, bitangent;
    orthonormal_basis(target, up, tangent, bitangent);
    return normalize(local.x * tangent + local.y * bitangent + local.z * target);
}

float3 world_to_local(float3 world, float3 target) {
    float3 up, tangent, bitangent;
    orthonormal_basis(target, up, tangent, bitangent);
    return float3(dot(world, tangent), dot(world, bitangent), dot(world, target));
}

float3 sample_cone(float3 dir, float radius) {
    float r1 = random();
    float r2 = random();
    
    float cos_theta = 1.0 - r1 * (1.0 - cos(radius));
    float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
    float phi = 2.0 * PI * r2;
    
    float3 local = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
    return local_to_world(local, dir);
}

float evaluate_cone_pdf(float radius) {
    return 1.0 / (2.0 * PI * (1.0 - cos(radius)));
}

/**
 * Trowbridge-Reitz (GGX) Normal Distribution Function
 * Source: Brent Burley (Disney) 2012, "Physically Based Shading at Disney"
 * Equation (8), Page 25.
 * Web: https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf
 */
float distribution_ggx(float NdotH, float alpha) {
    float alpha2 = pow(alpha, 2.0);
    float NdotH2 = pow(NdotH, 2.0);
    float denom = PI * pow(NdotH2 * (alpha2 - 1.0) + 1.0, 2.0);
    return alpha2 / denom;
}

/**
 * Fresnel-Schlick Approximation
 * Source: Christophe Schlick 1994, "An Inexpensive BRDF Model for Physically-based Rendering"
 * Equation (15), Page 8.
 * Web: https://wiki.jmonkeyengine.org/docs/3.8/tutorials/_attachments/Schlick94.pdf
 */
float3 fresnel_schlick(float3 F0, float HdotV) {
    return F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);
}

/**
 * GGX Distribution
 * Source: Eric Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
 * Equation (72), Page 84.
 * Web: https://jcgt.org/published/0003/02/03/paper.pdf
 */
float smith_lambda_ggx(float NdotV, float alpha) {
    float a2 = alpha * alpha;
    float cos2 = NdotV * NdotV;
    return (-1.0 + sqrt(1.0 + a2 * (1.0 - cos2) / cos2)) * 0.5;
}

/**
 * The Smith Masking Function
 * Source: Eric Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
 * Equation (43), Page 70.
 * Web: https://jcgt.org/published/0003/02/03/paper.pdf
 */
float smith_g1_ggx(float NdotV, float alpha) {
    return 1.0 / (1.0 + smith_lambda_ggx(NdotV, alpha));
}

/**
 * Height-Correlated Masking and Shadowing
 * Source: Eric Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
 * Equation (99), Page 91.
 * Web: https://jcgt.org/published/0003/02/03/paper.pdf
 */
float smith_g2_height_correlated(float NdotV, float NdotL, float alpha) {
    float lambdaV = smith_lambda_ggx(NdotV, alpha);
    float lambdaL = smith_lambda_ggx(NdotL, alpha);
    return 1.0 / (1.0 + lambdaV + lambdaL);
}

/**
 * Complete Implementation of the GGX VNDF Sampling Routine
 * Source: Eric Heitz (Unity Technologies) 2018, "Sampling the GGX Distribution of Visible Normals"
 * Listing (1), Page 10.
 * Web: https://www.jcgt.org/published/0007/04/01/paper.pdf
 */
float3 sample_ggx_vndf(float3 V, float alpha) {
    float3 Vh = normalize(float3(alpha * V.x, alpha * V.y, V.z));
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    float3 T1 = lensq > 0.0 ? float3(-Vh.y, Vh.x, 0.0) * rsqrt(lensq) : float3(1.0, 0.0, 0.0);
    float3 T2 = cross(Vh, T1);

    float r = sqrt(random());
    float phi = 2.0 * PI * random();
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    float3 Ne = normalize(float3(alpha * Nh.x, alpha * Nh.y, max(0.0, Nh.z)));
    return Ne;
}

/**
 * Importance sampling using the VNDF
 * Source: Eric Heitz (Unity Technologies) 2018, "Sampling the GGX Distribution of Visible Normals"
 * Equation (17), Page 11.
 * Web: https://www.jcgt.org/published/0007/04/01/paper.pdf
 */
float evaluate_vndf_pdf(float NdotV, float NdotH, float alpha) {
    float D = distribution_ggx(NdotH, alpha);
    float G1 = smith_g1_ggx(NdotV, alpha);
    return (G1 * D) / (4.0 * NdotV);
}

// Physically Based BRDF (Cook-Torrance)
float3 evaluate_brdf(float3 base_color, float metallic, float roughness, float3 N, float3 V, float3 L, out float pdf) {
    float3 H = normalize(V + L);

    float NdotL = max(dot(N, L), EPSILON);
    float NdotV = max(dot(N, V), EPSILON);
    float NdotH = max(dot(N, H), EPSILON);
    float HdotV = max(dot(H, V), EPSILON);

    float3 F0 = lerp(float3(0.04), base_color, metallic);
    float3 F  = fresnel_schlick(F0, HdotV);

    float p_spec = clamp(max(F.x, max(F.y, F.z)), 0.1, 0.9);

    float3 kd = (1.0 - F) * (1.0 - metallic);
    float3 diffuse  = kd * base_color / PI;

    float alpha = max(roughness * roughness, EPSILON);

    float D = distribution_ggx(NdotH, alpha);
    float G = smith_g2_height_correlated(NdotL, NdotV, alpha);
    float3 specular = D * F * G / (4.0 * NdotV * NdotL);

    float pdf_spec = evaluate_vndf_pdf(NdotV, NdotH, alpha);
    float pdf_diff = evaluate_cosine_pdf(NdotL);
    pdf = p_spec * pdf_spec + (1.0 - p_spec) * pdf_diff;

    return diffuse + specular;
}

float3 sample_brdf(float3 base_color, float metallic, float roughness, float3 N, float3 V) {
    float3 F0 = lerp(float3(0.04), base_color, metallic);
    float3 F = fresnel_schlick(F0, max(dot(N, V), 0.0));
    float p_spec = clamp(max(F.x, max(F.y, F.z)), 0.1, 0.9);

    float alpha = max(roughness * roughness, EPSILON);

    float3 L;
    if (random() < p_spec) {
        float3 view_local = world_to_local(V, N);
        float3 half_local = sample_ggx_vndf(view_local, alpha);
        float3 half_world = local_to_world(half_local, N);
        L = reflect(-V, half_world);
    } else {
        L = local_to_world(sample_cosine_hemisphere(), N);
    }

    if (dot(N, L) <= 0.0) {
        return 0.0.xxx;
    }

    return L;
}

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
    float cos_theta = max(dot(light_normal, -to_light), EPSILON);

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

float3 sky_atmosphere(float3 ray_dir, float3 sun_dir) {
    RenderSettings render_settings = push_data.render_settings[0];
    float3 transmittance;
    float3 scatter;
    switch (render_settings.environment_type) {
        case EnvironmentType.None: scatter = 0.0.xxx; break;
        case EnvironmentType.Solid:
            scatter = render_settings.sky_color * render_settings.sky_emission;
            break;
        case EnvironmentType.Hdri: scatter = 0.0.xxx; break;
        case EnvironmentType.Procedural:
            scatter = IntegrateScattering(0.0.xxx, ray_dir, T_MAX, sun_dir, 1.0.xxx, transmittance);
            break;
    }
    return scatter;
}

float3 sun_transmittance(float3 sun_dir) {
    RenderSettings render_settings = push_data.render_settings[0];
    float3 transmittance;
    if (render_settings.environment_type != EnvironmentType.Procedural) {
        return render_settings.sun_color;
    }
    return Absorb(IntegrateOpticalDepth(0.0.xxx, sun_dir));
}

/**
 * A Fast and Robust Method for Avoiding Self-Intersection
 * Source: Eric Haines (NVIDIA) 2019, "Ray Tracing Gems"
 * Listing (6-1), Page 84.
 * Web: https://link.springer.com/content/pdf/10.1007/978-1-4842-4427-2.pdf
 */
float3 offset_ray_origin(float3 hitpos, float3 normal) {
    const float origin = 1.0 / 32.0;
    const float float_scale = 1.0 / 65536.0;
    const float int_scale = 256.0;

    int3 of_i = int3(int_scale * normal);

    float3 p_i = float3(
        asfloat(asint(hitpos.x) + (hitpos.x < 0 ? -of_i.x : of_i.x)),
        asfloat(asint(hitpos.y) + (hitpos.y < 0 ? -of_i.y : of_i.y)),
        asfloat(asint(hitpos.z) + (hitpos.z < 0 ? -of_i.z : of_i.z))
    );

    return float3(
        abs(hitpos.x) < origin ? hitpos.x + float_scale * normal.x : p_i.x,
        abs(hitpos.y) < origin ? hitpos.y + float_scale * normal.y : p_i.y,
        abs(hitpos.z) < origin ? hitpos.z + float_scale * normal.z : p_i.z
    );
}

float3 path_trace_uniform(RayDesc ray, int max_depth) {
    RenderSettings render_settings = push_data.render_settings[0];
    float3 radiance = float3(0.0);
    float3 throughput = float3(1.0);
    float sun_radius = render_settings.sun_radius;

    for (int depth = 0; depth < max_depth; ++depth) {
        Payload payload = {};
        TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

        if (payload.depth == T_MAX) {
            radiance += throughput * sky_atmosphere(ray.Direction, push_data.sun_dir);
            float cos_theta = dot(ray.Direction, push_data.sun_dir);
            float cos_max = cos(sun_radius);
            bool sun_is_visible = cos_theta >= cos_max;

            if (render_settings.sun == Sun.Enabled && sun_is_visible) {
                float3 transmittance = sun_transmittance(push_data.sun_dir);
                float3 sun_contrib = transmittance * 50.0;
                radiance += throughput * sun_contrib;
            }
            break;
        }

        float3 hitpos = ray.Origin + ray.Direction * payload.depth;
        float3 V = -ray.Direction;

        if (any(payload.emission > 0.0) && dot(payload.normal, V) > 0.0) {
            radiance += throughput * payload.emission;
            break;
        }

        if (dot(payload.normal, ray.Direction) > 0.0) {
            payload.normal = -payload.normal;
        }

        float3 next_dir = local_to_world(sample_uniform_hemisphere(), payload.normal);
        float3 brdf = evaluate_brdf(payload.color);
        float lambert = dot(payload.normal, next_dir);
        float pdf = evaluate_uniform_pdf();
        throughput *= brdf * lambert / pdf;

        ray.Origin = offset_ray_origin(hitpos, payload.normal);
        ray.Direction = next_dir;
    }
    return radiance;
}

float3 path_trace_importance(RayDesc ray, int max_depth) {
    RenderSettings render_settings = push_data.render_settings[0];
    float3 radiance = float3(0.0);
    float3 throughput = float3(1.0);
    float sun_radius = render_settings.sun_radius;

    for (int depth = 0; depth < max_depth; ++depth) {
        Payload payload = {};
        TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

        if (payload.depth == T_MAX) {
            radiance += throughput * sky_atmosphere(ray.Direction, push_data.sun_dir);
            float cos_theta = dot(ray.Direction, push_data.sun_dir);
            float cos_max = cos(sun_radius);
            bool sun_is_visible = cos_theta >= cos_max;

            if (render_settings.sun == Sun.Enabled && sun_is_visible) {
                float3 transmittance = sun_transmittance(push_data.sun_dir);
                float3 sun_contrib = transmittance * 50.0;
                radiance += throughput * sun_contrib;
            }
            break;
        }

        float3 hitpos = ray.Origin + ray.Direction * payload.depth;
        float3 V = -ray.Direction;

        if (any(payload.emission > 0.0) && dot(payload.normal, V) > 0.0) {
            radiance += throughput * payload.emission;
            break;
        }

        if (dot(payload.normal, ray.Direction) > 0.0) {
            payload.normal = -payload.normal;
        }

        float3 next_dir = sample_brdf(payload.color, payload.metallic, payload.roughness, payload.normal, V);
        float brdf_pdf;
        float3 brdf = evaluate_brdf(payload.color, payload.metallic, payload.roughness, payload.normal, V, next_dir, brdf_pdf);
        float lambert = dot(payload.normal, next_dir);
        throughput *= brdf * lambert / brdf_pdf;

        ray.Origin = offset_ray_origin(hitpos, payload.normal);
        ray.Direction = next_dir;
    }
    return radiance;
}

float3 path_trace_nee(RayDesc ray, int max_depth) {
    RenderSettings render_settings = push_data.render_settings[0];
    float3 radiance = float3(0.0);
    float3 throughput = float3(1.0);
    float sun_radius = render_settings.sun_radius;
    float sun_emission = render_settings.sun_emission;

    for (int depth = 0; depth < max_depth; ++depth) {
        Payload payload = {};
        TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

        if (payload.depth == T_MAX) {
            radiance += throughput * sky_atmosphere(ray.Direction, push_data.sun_dir);
            float cos_theta = dot(ray.Direction, push_data.sun_dir);
            float cos_max = cos(sun_radius);
            bool sun_is_visible = cos_theta >= cos_max;

            if (render_settings.sun == Sun.Enabled && sun_is_visible) {
                float3 transmittance = sun_transmittance(push_data.sun_dir);
                float3 sun_contrib = transmittance * 50.0;
                if (depth == 0) {
                    radiance += throughput * sun_contrib;
                }
            }
            break;
        }

        float3 hitpos = ray.Origin + ray.Direction * payload.depth;
        float3 V = -ray.Direction;

        if (any(payload.emission > 0.0) && dot(payload.normal, V) > 0.0) {
            if (depth == 0) {
                radiance += throughput * payload.emission;
            }
            break;
        }

        if (dot(ray.Direction, payload.normal) > 0.0) {
            payload.normal = -payload.normal;
        }

        Light light = random_light();
        float3 light_sample = sample_light(light);
        float3 to_light = light_sample - hitpos;
        float distance_to_light = length(to_light);
        float3 light_dir = normalize(to_light);
        float light_lambert = dot(payload.normal, light_dir);

        if (any(light.emission > 0.0) && light_lambert > 0.0) {
            RayDesc shadow_ray = {};
            shadow_ray.Origin = offset_ray_origin(hitpos, payload.normal);
            shadow_ray.Direction = light_dir;
            shadow_ray.TMin = T_MIN;
            shadow_ray.TMax = distance_to_light - 0.001;

            Payload shadow_payload = {};
            TraceRay(tlas, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                0xFF, 0, 0, 0, shadow_ray, shadow_payload);

            bool is_visible = shadow_payload.depth >= distance_to_light - EPSILON;

            if (is_visible) {
                float brdf_pdf;
                float3 brdf = evaluate_brdf(payload.color, payload.metallic, payload.roughness, payload.normal, V, light_dir, brdf_pdf);
                float light_pdf = evaluate_light_pdf(light, to_light);
                if (light_pdf < EPSILON) continue; // ?
                float3 light_contrib = light.emission * light_lambert / light_pdf;
                radiance += throughput * brdf * light_contrib;
            }
        }

        if (render_settings.sun == Sun.Enabled &&
            render_settings.environment_type != EnvironmentType.None &&
            render_settings.environment_type != EnvironmentType.Hdri) {
            float3 sampled_dir = sample_cone(push_data.sun_dir, sun_radius);
            float sun_lambert = dot(payload.normal, sampled_dir);
            
            if (sun_lambert > 0.0) {
                RayDesc shadow_ray = {};
                shadow_ray.Origin = offset_ray_origin(hitpos, payload.normal);
                shadow_ray.Direction = sampled_dir;
                shadow_ray.TMin = T_MIN;
                shadow_ray.TMax = T_MAX;

                Payload shadow_payload = {};
                TraceRay(tlas, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                    0xFF, 0, 0, 0, shadow_ray, shadow_payload);

                if (shadow_payload.depth == T_MAX) {
                    float brdf_pdf;
                    float3 brdf = evaluate_brdf(payload.color, payload.metallic, payload.roughness, payload.normal, V, sampled_dir, brdf_pdf);
                    float3 transmittance = sun_transmittance(push_data.sun_dir);
                    float sun_pdf = evaluate_cone_pdf(sun_radius);
                    if (sun_pdf < EPSILON) continue; // ?
                    float3 sun_contrib = transmittance * sun_emission * sun_lambert / sun_pdf;
                    radiance += throughput * brdf * sun_contrib;
                }
            }
        }

        float3 next_dir = sample_brdf(payload.color, payload.metallic, payload.roughness, payload.normal, V);
        float brdf_pdf;
        float3 brdf = evaluate_brdf(payload.color, payload.metallic, payload.roughness, payload.normal, V, next_dir, brdf_pdf);
        float lambert = dot(next_dir, payload.normal);
        throughput *= brdf * lambert / brdf_pdf;

        ray.Origin = offset_ray_origin(hitpos, payload.normal);
        ray.Direction = next_dir;
    }

    return radiance;
}

float3 path_trace_mis(RayDesc ray, int max_depth) {
    RenderSettings render_settings = push_data.render_settings[0];
    float3 radiance = float3(0.0);
    float3 throughput = float3(1.0);
    float sun_radius = render_settings.sun_radius;
    float sun_emission = render_settings.sun_emission;
    float last_pdf = 1.0;

    for (int depth = 0; depth < max_depth; ++depth) {
        Payload payload = {};
        TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

        if (payload.depth == T_MAX) {
            radiance += throughput * sky_atmosphere(ray.Direction, push_data.sun_dir);
            float cos_theta = dot(ray.Direction, push_data.sun_dir);
            float cos_max = cos(sun_radius);
            bool sun_is_visible = cos_theta >= cos_max;

            if (render_settings.sun == Sun.Enabled && sun_is_visible) {
                float3 transmittance = sun_transmittance(push_data.sun_dir);
                float3 sun_contrib = transmittance * 50.0;
                if (depth == 0) {
                    radiance += throughput * sun_contrib;
                } else {
                    float sun_pdf = evaluate_cone_pdf(sun_radius);
                    float mis_weight = power_heuristic(last_pdf, sun_pdf);
                    radiance += throughput * sun_contrib * mis_weight;
                }
            }
            break;
        }

        float3 hitpos = ray.Origin + ray.Direction * payload.depth;
        float3 V = -ray.Direction;

        float dir_lambert = dot(ray.Direction, payload.normal);

        if (any(payload.emission > 0.0) && dot(payload.normal, V) > 0.0) {
            if (depth == 0) {
                radiance += throughput * payload.emission;
            } else {
                float distance_squared = payload.depth * payload.depth;
                float cos_theta = max(dot(payload.normal, V), EPSILON);
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

        if (any(light.emission > 0.0) && light_lambert > 0.0) {
            RayDesc shadow_ray = {};
            shadow_ray.Origin = offset_ray_origin(hitpos, payload.normal);
            shadow_ray.Direction = light_dir;
            shadow_ray.TMin = T_MIN;
            shadow_ray.TMax = distance_to_light - 0.001;

            Payload shadow_payload = {};
            TraceRay(tlas, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                0xFF, 0, 0, 0, shadow_ray, shadow_payload);

            bool is_visible = shadow_payload.depth >= distance_to_light - EPSILON;

            if (is_visible) {
                float brdf_pdf;
                float3 brdf = evaluate_brdf(payload.color, payload.metallic, payload.roughness, payload.normal, V, light_dir, brdf_pdf);
                float light_pdf = evaluate_light_pdf(light, to_light);
                float mis_weight = power_heuristic(light_pdf, brdf_pdf);
                float3 light_contrib = light.emission * light_lambert * mis_weight / light_pdf;
                radiance += throughput * brdf * light_contrib;
            }
        }

        if (render_settings.sun == Sun.Enabled &&
            render_settings.environment_type != EnvironmentType.None &&
            render_settings.environment_type != EnvironmentType.Hdri) {
            float3 sampled_dir = sample_cone(push_data.sun_dir, sun_radius);
            float sun_lambert = dot(payload.normal, sampled_dir);
            
            if (sun_lambert > 0.0) {
                RayDesc shadow_ray = {};
                shadow_ray.Origin = offset_ray_origin(hitpos, payload.normal);
                shadow_ray.Direction = sampled_dir;
                shadow_ray.TMin = T_MIN;
                shadow_ray.TMax = T_MAX;

                Payload shadow_payload = {};
                TraceRay(tlas, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                    0xFF, 0, 0, 0, shadow_ray, shadow_payload);

                if (shadow_payload.depth == T_MAX) {
                    float brdf_pdf;
                    float3 brdf = evaluate_brdf(payload.color, payload.metallic, payload.roughness, payload.normal, V, sampled_dir, brdf_pdf);
                    float3 transmittance = sun_transmittance(push_data.sun_dir);
                    float sun_pdf = evaluate_cone_pdf(sun_radius);
                    float mis_weight = power_heuristic(sun_pdf, brdf_pdf);
                    float3 sun_contrib = transmittance * sun_emission * sun_lambert * mis_weight / sun_pdf;
                    radiance += throughput * brdf * sun_contrib;
                }
            }
        }

        float3 next_dir = sample_brdf(payload.color, payload.metallic, payload.roughness, payload.normal, V);
        float brdf_pdf;
        float3 brdf = evaluate_brdf(payload.color, payload.metallic, payload.roughness, payload.normal, V, next_dir, brdf_pdf);
        float lambert = dot(next_dir, payload.normal);
        throughput *= brdf * lambert / brdf_pdf;
        last_pdf = brdf_pdf;

        ray.Origin = offset_ray_origin(hitpos, payload.normal);
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