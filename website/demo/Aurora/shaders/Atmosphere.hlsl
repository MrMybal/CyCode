cbuffer Camera : register(b0) {
    float4x4 ViewProjection;
    float3 CameraPosition;
    float Time;
};

float4 MainPS(float4 position : SV_POSITION) : SV_TARGET {
    float horizon = saturate(position.y * 0.001);
    float3 sky = lerp(float3(0.04, 0.08, 0.16), float3(0.2, 0.55, 0.9), horizon);
    return float4(sky, 1.0);
}
