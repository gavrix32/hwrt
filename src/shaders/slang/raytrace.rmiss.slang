import common;

[shader("miss")]
void main(inout Payload payload) {
    payload.color = float3(0.3, 0.6, 1.0);
}