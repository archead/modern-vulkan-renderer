import common;

struct LightingVSOutput {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

struct PointLightOutput {
    float3 ambient;
    float3 diffuse;
    float3 specular;
};

 [[vk::binding(0, 0)]]
Sampler2D gBufferFragPos;
[[vk::binding(1, 0)]]
Sampler2D gBufferNormal;
[[vk::binding(2, 0)]]
Sampler2D gBufferAlbedo;
[[vk::binding(3, 0)]]
ConstantBuffer<LightBuffer> lightingUBO;

[[vk::push_constant]]
int debugState;

PointLightOutput CalcPointLight(LightBuffer pointLight, int lightIndex, float3 normal, float3 fragPos) {
    // point light properties
    float3 diffuseColor = float3(pointLight.light[lightIndex].color);
    float3 pointLightCoord = float3(pointLight.light[lightIndex].pos);
    float pointLightIntensity = pointLight.light[lightIndex].intensity;
    float K = pointLight.light[lightIndex].attenK;

    // fragment position
    float3 fragmentCoord = fragPos;

    // attenuation
    float3 lightDistance = (pointLightCoord - fragmentCoord);
    float RSq = dot(lightDistance, lightDistance);
    float attenIntensity = pointLightIntensity/(1.0 + K * RSq);

    // angle of coincidence
    float3 L = normalize(pointLightCoord - fragmentCoord);
    float angleOfCoincidence = saturate(dot(normal, L));

    // Blinn-Phong
    float3 cameraPos = float3(pointLight.cameraPos.xyz);
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

    return PointLightOutput(ambient, diffuse, specular);
}

// Lighting shader pipeline --------------------------------------

[shader("vertex")]
LightingVSOutput lightingVertMain(uint vertexID : SV_VertexID) {
    LightingVSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.pos = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 lightingFragMain(LightingVSOutput lightingVertIn) : SV_TARGET {

    if (debugState == 0) {
        float4 color = 0.0;
        float4 albedo = float4(gBufferAlbedo.Sample(lightingVertIn.uv).xyz, 1.0);

        for (int i = 0; i < lightingUBO.cameraPos.w; i++){
            PointLightOutput light = CalcPointLight(lightingUBO, i, gBufferNormal.Sample(lightingVertIn.uv).xyz, gBufferFragPos.Sample(lightingVertIn.uv).xyz);
            color += float4(albedo.rgb * (light.ambient + light.diffuse) + light.specular, 1.0);
        }
        return color;
    }

    if (debugState == 1) return float4(gBufferAlbedo.Sample(lightingVertIn.uv).xyz, 1.0);
    if (debugState == 2) return float4(gBufferNormal.Sample(lightingVertIn.uv).xyz, 1.0) * 0.5 + 0.5; // remap [-1, 1] to [0, 1]
    if (debugState == 3) return float4(gBufferFragPos.Sample(lightingVertIn.uv).xyz, 1.0);

    return float4(1.0, 0.0, 0.0, 1.0);
}
