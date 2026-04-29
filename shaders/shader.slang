struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
    float3 inNormal;
};

struct UniformBuffer{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput {
    float4 pos : SV_Position;
    float3 fragPos;
    float3 color;
    float2 fragTexCoord;
    float3 normal;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));


    float4 worldPos = mul(ubo.model, float4(input.inPosition, 1.0));
    output.fragPos = worldPos.xyz;
    output.fragTexCoord = input.inTexCoord;
    output.color = input.inColor;

    float3 N = normalize(mul((float3x3)ubo.model, input.inNormal));
    output.normal = N;

    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    float3 pointLdir = float3(0.5, 0.2, 0.4);
    float3 L = normalize(pointLdir - vertIn.fragPos);
    float3 normal = normalize(vertIn.normal);
    float NdotL = saturate(dot(normal, L));

    float3 ambient = 0.05 * vertIn.color;
    float3 diffuse = NdotL * vertIn.color;

    //return texture.Sample(vertIn.fragTexCoord);
    return float4(ambient + diffuse, 1.0);
}