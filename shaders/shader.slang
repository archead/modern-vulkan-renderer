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
    Light light;
    float3 cameraPos;
    float _pad0; // data padding (everything needs to be aligned to sets of 16 bytes (float3 + float = 16 bytes)
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

    float3 normal;

    if (vertIn.useNormal){
        normal = normalize(normalMap.Sample(vertIn.fragTexCoord).xyz * 2.0 - 1.0);
        normal = normalize(mul(vertIn.TBN, normal));
    } else { normal = normalize(vertIn.normal); }

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

    float4 albedo = texture.Sample(vertIn.fragTexCoord);

    float3 color = albedo.rgb * (ambient + diffuse) + specular;

    float gamma = 2.2;
    color = pow(color.rgb, float3(1.0/gamma));
   return float4(color, albedo.a);
}