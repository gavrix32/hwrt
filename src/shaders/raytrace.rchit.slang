[shader("closesthit")]
void main(inout float3 color, in BuiltInTriangleIntersectionAttributes attr) {
    float3 barycentrics = float3(
        1.0 - attr.barycentrics.x - attr.barycentrics.y,
        attr.barycentrics.x,
        attr.barycentrics.y
    );

    color = barycentrics;
}