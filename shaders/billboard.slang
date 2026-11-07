import common;

// Billboard shader pipeline --------------------------------------

struct BillboardVSOutput {
    float4 pos : SV_Position;
    float4 color : COLOR0;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUBO> globalUbo;
[[vk::binding(1, 0)]]
ConstantBuffer<LightBuffer> lightUbo;

[shader("vertex")]
BillboardVSOutput billboardVertMain(uint vertexID : SV_VertexID, uint instanceID: SV_InstanceID) {
    BillboardVSOutput output;

    const float2 corners[4] = {
        {-0.5, 0.5},
        {0.5, 0.5},
        {-0.5, -0.5},
        {0.5, -0.5}
    };

    const uint cornerIndex[6] = {0, 1, 2, 2, 1, 3};

    float size = 0.2;
     float2 offset = corners[cornerIndex[vertexID]];
    float3 cameraRight = float3(globalUbo.view[0].xyz);
    float3 cameraUp = float3(globalUbo.view[1].xyz);

    float3 lightPos = lightUbo.light[instanceID].pos;
    float3 worldPos = lightPos + (offset.x * cameraRight + offset.y * cameraUp) * size;
    output.pos = mul(globalUbo.proj, mul(globalUbo.view, float4(worldPos, 1.0)));
    output.color = float4(lightUbo.light[instanceID].color * lightUbo.light[instanceID].intensity, 1.0); // colors the gizmo the same color as the light and uses intensity to control the brightness

    return output;
}

[shader("fragment")]
float4 billboardFragMain(BillboardVSOutput vertIn) : SV_TARGET {
    return vertIn.color;
}
