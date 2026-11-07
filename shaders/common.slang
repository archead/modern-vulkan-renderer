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

struct GlobalUBO{
    float4x4 view;
    float4x4 proj;
};
