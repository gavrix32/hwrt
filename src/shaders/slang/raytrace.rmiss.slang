#include "common.h"

[shader("miss")]
void main(inout Payload payload) {
    payload.emission = float3(0.3, 0.6, 1.0) * 10.0;
    // payload.emission = float3(0.0);
}