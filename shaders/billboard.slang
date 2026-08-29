struct VSInput {
    float3 inPosition : POSITION;
    float3 inColor : COLOR0;
    float2 inTexCoord : TEXCOORD0;
    float3 inNormal : NORMAL;
    float4 inTangent;
};

struct GlobalUBO{
    float4x4 view;
    float4x4 proj;
};

struct ObjectUBO{
      float4x4 model;
      float4x4 normalMatrix; // inverse transpose of the model matrix casted down to 3x3
      uint4 flags; // x = useNormalMap
  };
struct Light {
  float3 pos;
  float intensity;
  float3 color;
  float attenK;
};

struct LightBuffer {
    Light light[2]; // max lights is hardcoded right now, should be changed
    float4 cameraPos; // .w holds the max lights value
};

struct PointLightOutput {
    float3 ambient;
    float3 diffuse;
    float3 specular;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 fragPos;
    float3 color;
    float2 fragTexCoord;
    float3 normal;
    float3x3 TBN;
    bool useNormal;
};

struct GBufferOutput {
    float4 fragPos : SV_Target0;
    float4 normal : SV_Target1;
    float4 albedo : SV_Target2;
};

struct BillboardVSOutput {
    float4 pos : SV_Position;
    float4 color : COLOR0;
};

[[vk::binding(0,0)]] LightBuffer lightBuffer;
[[vk::binding(1,0)]] GlobalUBO globalUbo;

[shader("vertex")]
BillboardVSOutput billboardVertMain(uint vertexID : SV_VertexID) {
    BillboardVSOutput output;

    const float2 corners[4] = {
        {-0.5, 0.5},
        {0.5, 0.5},
        {-0.5, -0.5},
        {0.5, -0.5}
    };

    const uint cornerIndex[6] = {0, 1, 2, 2, 1, 3};

    output.pos = float4(corners[cornerIndex[vertexID]], 0.0, 1.0);
    output.color = float4(1.0, 1.0, 1.0, 1.0); // white

    return output;
}

[shader("fragment")]
float4 billboardFragMain(BillboardVSOutput vertIn) : SV_TARGET {
    return vertIn.color;
}