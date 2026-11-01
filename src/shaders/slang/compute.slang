#include "common.h"

[vk::binding(0, 0)]
RWTexture2D images[2];

[vk::binding(1, 0)]
RWStructuredBuffer<ExposureState> exposure_state;

groupshared float group_log_lum[256];
groupshared float group_valid[256];

[[vk::push_constant]]
PushData push_data;

// Khronos PBR Neutral Tone Mapper
// https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/README.md
float3 neutral(float3 color) {
    const float start_compression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < start_compression) return color;

    const float d = 1.0 - start_compression;
    float new_peak = 1.0 - d * d / (peak + d - start_compression);
    color *= new_peak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - new_peak) + 1.0);
    return lerp(color, float3(new_peak), g);
}

float luminance(float3 linear_rgb) {
    // ITU-R Rec. 709 luminance weights
    const float3 weights = float3(0.2126, 0.7152, 0.0722);
    return dot(linear_rgb, weights);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 thread_id : SV_DispatchThreadID, uint group_index : SV_GroupIndex) {
    RWTexture2D rt_image = images[0];
    RWTexture2D out_image = images[1];

    uint width, height;
    out_image.GetDimensions(width, height);

    bool inside = thread_id.x < width && thread_id.y < height;

    float4 rt_color = inside ? rt_image[thread_id.xy] : float4(0.0);

    // Auto exposure measurement (before exposure and tone mapping).
    // log2 luminance is clamped to [-20, 12] and shifted to [0, 32]; NaN/Inf and out-of-bounds threads contribute 0.
    float lum = luminance(rt_color.rgb);
    float log_lum = 0.0;
    float valid = 0.0;

    RenderSettings render_settings = push_data.render_settings[0];
    bool is_debug = render_settings.debug_channel != DebugChannel.None;

    // Debug channels and too low/high numbers are not measured
    if (inside && !is_debug && render_settings.autoexposure != 0 && isfinite(lum) && lum > 1e-4) {
        log_lum = clamp(log2(lum), -20.0, 12.0) + 20.0;
        valid = 1.0;
    }
    group_log_lum[group_index] = log_lum;
    group_valid[group_index] = valid;
    GroupMemoryBarrierWithGroupSync();

    for (uint stride = 128; stride > 0; stride >>= 1) {
        if (group_index < stride) {
            group_log_lum[group_index] += group_log_lum[group_index + stride];
            group_valid[group_index] += group_valid[group_index + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (group_index == 0) {
        // Scale 4x and discard the modulo to keep approximated uint value
        InterlockedAdd(exposure_state[0].sum, uint(group_log_lum[0] * 4.0));
        InterlockedAdd(exposure_state[0].count, uint(group_valid[0]));
    }

    if (!inside) return;

    // Debug channels are displayed without exposure
    // The auto exposure value is the one computed in the previous frame.
    float exposure = render_settings.exposure;
    if (is_debug) {
        exposure = 1.0;
    } else if (render_settings.autoexposure != 0) {
        exposure = exp2(exposure_state[0].ev);
    }
    rt_color.rgb = neutral(rt_color.rgb * exposure);

    if (render_settings.debug_channel == DebugChannel.None ||
        render_settings.debug_channel == DebugChannel.BaseColor) {
        rt_color.rgb = pow(rt_color.rgb, 1.0 / 2.2);
    }

    float4 out_color = rt_color;
    out_image[thread_id.xy] = out_color;
}