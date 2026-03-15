#include "common.h"

[vk::binding(0, 0)]
RWTexture2D images[2];

[[vk::push_constant]]
PushData push_data;

// TODO
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

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
    RWTexture2D rt_image = images[0];
    RWTexture2D out_image = images[1];

    uint width, height;
    out_image.GetDimensions(width, height);

    if (thread_id.x >= width || thread_id.y >= height) return;

    float4 rt_color = rt_image[thread_id.xy];

    RenderSettings render_settings = push_data.render_settings[0];

    if (render_settings.debug_channel == DebugChannel.None ||
        render_settings.debug_channel == DebugChannel.BaseColor) {
        rt_color.rgb = neutral(rt_color.rgb);
        rt_color.rgb = pow(rt_color.rgb, 1.0 / 2.2);
    }

    float4 out_color = rt_color;

    out_image[thread_id.xy] = out_color;
}