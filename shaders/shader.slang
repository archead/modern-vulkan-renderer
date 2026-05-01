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
    // point light properties
    float3 diffuseColor = float3(0.96, 0.89, 0.54);
    float3 pointLcoord = float3(0.5, 0.2, 0.4);
    float pointLightIntensity = 0.75;

    // angle of coincidence
    float3 L = normalize(pointLcoord - vertIn.fragPos);
    float3 normal = normalize(vertIn.normal);
    float NdotL = saturate(dot(normal, L));

    // attenuation
    float K = 0.25;
    float3 lightDistance = (pointLcoord - vertIn.fragPos);
    float RSq = dot(lightDistance, lightDistance);
    float attenIntensity = pointLightIntensity/(1.0 + K * RSq);

    // Blinn-Phong
    float3 cameraPos = float3(2.0, 2.0, 2.0);
    float shineFactor = 128;
    float3 specColor = float3(0.25, 0.25, 0.25);

    float3 viewDir = normalize(cameraPos - vertIn.fragPos);
    float3 halfAngle = normalize(L + viewDir);
    float blinnTerm = saturate(dot(normal, halfAngle));
    blinnTerm = NdotL > 0.0 ? blinnTerm : 0.0;
    blinnTerm = pow(blinnTerm, shineFactor);

    // ambient + diffuse Blinn-phong config
    float3 ambient = 0.01 * diffuseColor;
    float3 diffuse = NdotL * diffuseColor * attenIntensity;
    float3 specular = specColor * blinnTerm * attenIntensity;

    //return texture.Sample(vertIn.fragTexCoord);
    return float4(ambient + diffuse + specular, 1.0);
}