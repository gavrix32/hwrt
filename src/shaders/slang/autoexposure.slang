#include "common.h"

[vk::binding(1, 0)]
RWStructuredBuffer<ExposureState> exposure_state;

[[vk::push_constant]]
PushData push_data;

// Turns the luminance measured by compute.slang into the exposure for the next frames.
// Runs as a single thread after the tone mapping pass.
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
    RenderSettings render_settings = push_data.render_settings[0];
    ExposureState state = exposure_state[0];

    // count == 0: nothing was measured (debug channel, or nothing bright enough), keep the current exposure
    if (render_settings.autoexposure != 0 && state.count > 0) {
        float avg_log2_lum = float(state.sum) / 4.0 / float(state.count) - 20.0;

        // Map the average luminance to middle grey value
        // exposure = 2^ev, ev = log2(exposure)
        float middle_grey = 0.18;
        float target_ev = clamp(-avg_log2_lum + log2(middle_grey) + render_settings.exposure_compensation,
                                render_settings.min_ev, render_settings.max_ev);

        state.ev += (target_ev - state.ev) * (1.0 - exp(-render_settings.delta_time * render_settings.adaptation_speed));
    }

    state.sum = 0;
    state.count = 0;
    exposure_state[0] = state;
}