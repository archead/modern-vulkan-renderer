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
ConstantBuffer<UniformBuffer> ubo : register(b0, space0);

struct Light {
  float3 pos;
  float intensity;
  float3 color;
  float attenK;
};

struct LightBuffer {
    Light light;
    float3 cameraPos;
    float _pad0; // data padding (everything needs to be aligned to sets of 16 bytes (float3 + float = 16 bytes)
};
ConstantBuffer<LightBuffer> lightUbo : register(b0, space1);

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
    output.normal = normalize(mul((float3x3)ubo.model, input.inNormal));

    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    // point light properties
    float3 diffuseColor = float3(lightUbo.light.color);
    float3 pointLightCoord = float3(lightUbo.light.pos);
    float pointLightIntensity = lightUbo.light.intensity;

    // fragment position
    float3 fragmentCoord = float3(vertIn.fragPos);

    // angle of coincidence
    float3 L = normalize(pointLightCoord - fragmentCoord);
    float3 normal = normalize(vertIn.normal);
    float angleOfCoincidence = saturate(dot(normal, L));

    // attenuation
    float K = lightUbo.light.attenK;
    float3 lightDistance = (pointLightCoord - fragmentCoord);
    float RSq = dot(lightDistance, lightDistance);
    float attenIntensity = pointLightIntensity/(1.0 + K * RSq);

    // Blinn-Phong
    float3 cameraPos = float3(lightUbo.cameraPos);
    float shineFactor = 128;
    float3 specColor = float3(1.0, 1.0, 1.0);

    float3 viewDir = normalize(cameraPos - fragmentCoord);
    float3 halfAngle = normalize(L + viewDir);
    float blinnTerm = saturate(dot(normal, halfAngle));
    blinnTerm = angleOfCoincidence > 0.0 ? blinnTerm : 0.0;
    blinnTerm = pow(blinnTerm, shineFactor);

    // ambient + diffuse Blinn-phong config
    float3 ambient = 0.005 * diffuseColor;
    float3 diffuse = angleOfCoincidence * diffuseColor * attenIntensity;
    float3 specular = specColor * blinnTerm * attenIntensity;

    //return texture.Sample(vertIn.fragTexCoord);
    return float4(ambient + diffuse + specular, 1.0);
}