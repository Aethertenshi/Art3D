struct VSInput
{
    float3 Position : TEXCOORD0;
    float4 Color    : TEXCOORD1;
    float3 Normal   : TEXCOORD2;
};

struct VSOutput
{
    float4 Position    : SV_Position;
    float4 Color       : COLOR;
    float3 WorldNormal : TEXCOORD0;
    float3 WorldPos    : TEXCOORD1;
};

cbuffer UBO : register(b0, space1)
{
    float4x4 MVP;
    float4x4 Model;          // model matrix for world-space transform
    float4   ColorOverride;  // if w > 0, override vertex color with rgb
};

cbuffer LightUBO : register(b0, space3)
{
    float3 LightPos;
    float  LightIntensity;
    float3 LightColor;
    float  _pad;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = mul(MVP, float4(input.Position, 1.0f));

    if (ColorOverride.a > 0.0f) {
        output.Color = float4(ColorOverride.rgb, input.Color.a);
    } else {
        output.Color = input.Color;
    }

    // Transform vertex to world space using Model matrix
    float4 worldPos4    = mul(Model, float4(input.Position, 1.0f));
    output.WorldPos     = worldPos4.xyz;

    // Transform normal to world space (valid for uniform scale / no shear)
    output.WorldNormal  = normalize(mul((float3x3)Model, input.Normal));

    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float3 N = normalize(input.WorldNormal);
    float3 L = normalize(LightPos - input.WorldPos);  // both in world space now

    float ambient  = 0.15f;
    float diffuse  = max(dot(N, L), 0.0f);
    float lighting = ambient + diffuse * LightIntensity;

    float3 lit = input.Color.rgb * LightColor * lighting;
    return float4(saturate(lit), input.Color.a);
}
