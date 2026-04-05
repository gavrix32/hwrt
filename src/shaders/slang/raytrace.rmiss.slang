#include "common.h"

[shader("miss")]
void main(inout Payload payload) {
    payload.depth = T_MAX;
}