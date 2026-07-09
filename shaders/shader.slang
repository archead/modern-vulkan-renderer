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
}
//set 1
[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUBO> globalUbo;
[[vk::binding(1, 0)]]
ConstantBuffer<LightBuffer> lightUbo;

//set 2
[[vk::binding(0, 1)]]
ConstantBuffer<ObjectUBO> objectUbo;
[[vk::binding(1, 1)]]
Sampler2D texture;
[[vk::binding(2, 1)]]
Sampler2D normalMap;


[[vk::binding(0, 2)]]
Sampler2D gBufferFragPos;
[[vk::binding(1, 2)]]
Sampler2D gBufferNormal;
[[vk::binding(2, 2)]]
Sampler2D gBufferAlbedo;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(globalUbo.proj, mul(globalUbo.view, mul(objectUbo.model, float4(input.inPosition, 1.0))));

    float4 worldPos = mul(objectUbo.model, float4(input.inPosition, 1.0));
    output.fragPos = worldPos.xyz;

    output.fragTexCoord = input.inTexCoord;
    output.color = input.inColor;

    output.normal = normalize(mul((float3x3)objectUbo.normalMatrix, input.inNormal));

    float3 N = output.normal;
    float3 T = normalize(mul((float3x3)objectUbo.normalMatrix, input.inTangent.xyz));
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(input.inTangent.w * cross(N, T));
    float3x3 TBN = float3x3(float3(T.x, B.x, N.x),
                            float3(T.y, B.y, N.y),
                            float3(T.z, B.z, N.z));
    output.TBN = TBN;

    output.useNormal = (bool)objectUbo.flags.x;

    return output;
}

struct LightingVSOutput {
    float4 pos : SV_Position;
    float2 uv;
}

[shader("vertex")]
LightingVSOutput lightingVertMain(uint vertexID : SV_VertexID) {
    LightingVSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.pos = (output.uv * 2.0 - 1.0, 0.0, 1.0);
    return output;

}

[shader("fragment")]
GBufferOutput geometryFragMain(VSOutput vertIn) {
    GBufferOutput output = {};

    output.fragPos = float4(vertIn.fragPos, 1.0);

    // set the normal vector, if object has a normalMap, use that, if not, use vertex normal
    float3 normal;
    if (vertIn.useNormal){
        normal = normalize(normalMap.Sample(vertIn.fragTexCoord).xyz * 2.0 - 1.0);
        normal = normalize(mul(vertIn.TBN, normal));
    } else { normal = normalize(vertIn.normal); }

    output.normal = float4(normal, 0.0);
    output.albedo = texture.Sample(vertIn.fragTexCoord);

    return output;
}

PointLightOutput CalcPointLight(LightBuffer pointLight, int lightIndex, float3 normal, VSOutput vertIn) {
    // point light properties
    float3 diffuseColor = float3(pointLight.light[lightIndex].color);
    float3 pointLightCoord = float3(pointLight.light[lightIndex].pos);
    float pointLightIntensity = pointLight.light[lightIndex].intensity;
    float K = pointLight.light[0].attenK;

    // fragment position
    float3 fragmentCoord = float3(vertIn.fragPos);

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

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {

    // set the normal vector, if object has a normalMap, use that, if not, use vertex normal
    float3 normal;
    if (vertIn.useNormal){
        normal = normalize(normalMap.Sample(vertIn.fragTexCoord).xyz * 2.0 - 1.0);
        normal = normalize(mul(vertIn.TBN, normal));
    } else { normal = normalize(vertIn.normal); }

    float4 albedo = texture.Sample(vertIn.fragTexCoord);

    float3 color = float3(0);

    for (int i = 0; i < lightUbo.cameraPos.w; i++){ // .w stores the light count, variable needs to be renamed
        PointLightOutput lightOut = CalcPointLight(lightUbo, i, normal, vertIn);
        color += albedo.rgb * (lightOut.ambient + lightOut.diffuse) + lightOut.specular;
    }

    return float4(color, albedo.a);
}

[shader("fragment")]
float4 lightingFragMain(VSOutput lightingVertIn) : SV_TARGET {
    return float4(0, 0, 0, 1);
}