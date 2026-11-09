import common;

struct VSOutput {
    float4 pos : SV_Position;
    float3 fragPos;
    float2 fragTexCoord;
    float3 normal;
    float3x3 TBN;
};

struct GBufferOutput {
    float4 fragPos : SV_Target0;
    float4 normal : SV_Target1;
    float4 albedo : SV_Target2;
};

struct VSInput {
    float3 inPosition : POSITION;
    float3 inColor : COLOR0;
    float2 inTexCoord : TEXCOORD0;
    float3 inNormal : NORMAL;
    float4 inTangent;
};

struct PushConstants{
    float4x4 model;
    float4x4 normalMatrix; // inverse transpose of the model matrix casted down to 3x3
    float4 materialFactors;  // x = metallic, y = roughness, z = normalScale, w = occlusionStrength
    float4 baseColorFactor;
};

 [[vk::binding(0, 0)]]
ConstantBuffer<GlobalUBO> globalUbo;

[[vk::binding(0, 1)]]
Sampler2D baseColorMap;
[[vk::binding(1, 1)]]
Sampler2D normalMap;
[[vk::binding(2, 1)]]
Sampler2D metalRoughMap;
[[vk::binding(3, 1)]]
Sampler2D occlusionMap;
[[vk::binding(4, 1)]]
Sampler2D emissiveMap;

[[vk::push_constant]]
ConstantBuffer<PushConstants> objectPush;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(globalUbo.proj, mul(globalUbo.view, mul(objectPush.model, float4(input.inPosition, 1.0))));

    float4 worldPos = mul(objectPush.model, float4(input.inPosition, 1.0));
    output.fragPos = worldPos.xyz;

    output.fragTexCoord = input.inTexCoord;

    output.normal = normalize(mul((float3x3)objectPush.normalMatrix, input.inNormal));

    float3 N = output.normal;
    float3 T = normalize(mul((float3x3)objectPush.normalMatrix, input.inTangent.xyz));
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(input.inTangent.w * cross(N, T));
    float3x3 TBN = float3x3(float3(T.x, B.x, N.x),
                            float3(T.y, B.y, N.y),
                            float3(T.z, B.z, N.z));
    output.TBN = TBN;

    return output;
}

[shader("fragment")]
GBufferOutput geometryFragMain(VSOutput vertIn) {
    GBufferOutput output = {};

    output.fragPos = float4(vertIn.fragPos, 1.0);

    // set the normal vector, if object has a normalMap, use that, if not, use vertex normal
    float3 normal;
    normal = normalize(normalMap.Sample(vertIn.fragTexCoord).xyz * 2.0 - 1.0);
    normal = normalize(mul(vertIn.TBN, normal));

    float4 metalRoughSample = metalRoughMap.Sample(vertIn.fragTexCoord);

    output.normal = float4(normal, metalRoughSample.b * objectPush.materialFactors.x);
    output.albedo = baseColorMap.Sample(vertIn.fragTexCoord) * objectPush.baseColorFactor;
    output.albedo.a = metalRoughSample.g * objectPush.materialFactors.y;

    return output;
}

