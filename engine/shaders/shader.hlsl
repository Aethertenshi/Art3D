struct VSInput
{
    float3 Position : TEXCOORD0;
    float4 Color    : TEXCOORD1;
    float3 Normal   : TEXCOORD2;
    float2 TexCoord : TEXCOORD3;
};

struct VSOutput
{
    float4 Position    : SV_Position;
    float4 Color       : COLOR;
    float3 WorldNormal : TEXCOORD0;
    float3 WorldPos    : TEXCOORD1;
    float2 TexCoord    : TEXCOORD2;
};

cbuffer UBO : register(b0, space1)
{
    float4x4 MVP;
    float4x4 Model;
    float4   ColorOverride;
};

struct GPULight
{
    float4 PosAndIntensity;
    float4 DirAndRange;
    float4 ColorAndType;
    float2 SpotAngles;
    float2 _Pad;
};

cbuffer LightUBO : register(b0, space3)
{
    GPULight Lights[8];
    int      LightCount;
    float    AmbientIntensity;
    float2   _ShadowPad0;
    float4x4 LightViewProj;
    int      HasShadow;
    float    _ShadowPad1;
    float    _ShadowPad2;
    float    _ShadowPad3;
};

Texture2D ShadowMap : register(t0, space2);
SamplerComparisonState ShadowSampler : register(s0, space2);

Texture2D DiffuseTexture : register(t1, space2);
SamplerState DiffuseSampler : register(s1, space2);

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = mul(MVP, float4(input.Position, 1.0f));

    if (ColorOverride.a > 0.0f) {
        output.Color = float4(ColorOverride.rgb, input.Color.a);
    } else {
        output.Color = input.Color;
    }

    float4 worldPos4 = mul(Model, float4(input.Position, 1.0f));
    output.WorldPos  = worldPos4.xyz;
    output.WorldNormal = normalize(mul((float3x3)Model, input.Normal));
    output.TexCoord = input.TexCoord;

    return output;
}

struct ShadowVSOutput
{
    float4 Position : SV_Position;
};

ShadowVSOutput VSShadow(VSInput input)
{
    ShadowVSOutput output;
    output.Position = mul(MVP, float4(input.Position, 1.0f));
    return output;
}

void PSShadow()
{
    // No-op: depth is written by the rasterizer, no color target
}

float ComputeShadow(float3 worldPos)
{
    if (HasShadow == 0) return 1.0;

    float4 lightClip = mul(LightViewProj, float4(worldPos, 1.0));
    float3 ndc = lightClip.xyz / lightClip.w;

    float2 uv = ndc.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 1.0;
    if (ndc.z > 1.0 || ndc.z < 0.0) return 1.0;

    float depth = ndc.z - 0.002;

    float2 texelSize = float2(1.0 / 2048.0, 1.0 / 2048.0);
    float shadow = 0.0;
    shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, uv + float2(-texelSize.x, -texelSize.y), depth);
    shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, uv + float2( texelSize.x, -texelSize.y), depth);
    shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, uv + float2(-texelSize.x,  texelSize.y), depth);
    shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, uv + float2( texelSize.x,  texelSize.y), depth);
    shadow *= 0.25;

    return shadow;
}

float3 CalcPointLight(GPULight light, float3 pos, float3 N)
{
    float3 L = light.PosAndIntensity.xyz - pos;
    float dist = length(L);
    L /= dist;
    float atten = 1.0f / (1.0f + 0.09f * dist + 0.032f * dist * dist);
    float NdotL = max(dot(N, L), 0.0f);
    return light.ColorAndType.xyz * NdotL * light.PosAndIntensity.w * atten;
}

float3 CalcSpotLight(GPULight light, float3 pos, float3 N)
{
    float3 L = light.PosAndIntensity.xyz - pos;
    float dist = length(L);
    L /= dist;
    float3 D = normalize(light.DirAndRange.xyz);
    float cosAngle = dot(-L, D);
    float spotFactor = smoothstep(light.SpotAngles.y, light.SpotAngles.x, cosAngle);
    float atten = 1.0f / (1.0f + 0.09f * dist + 0.032f * dist * dist);
    float NdotL = max(dot(N, L), 0.0f);
    return light.ColorAndType.xyz * NdotL * light.PosAndIntensity.w * atten * spotFactor;
}

float3 CalcDirectionalLight(GPULight light, float3 N)
{
    float3 L = -normalize(light.DirAndRange.xyz);
    float NdotL = max(dot(N, L), 0.0f);
    return light.ColorAndType.xyz * NdotL * light.PosAndIntensity.w;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float3 N = normalize(input.WorldNormal);
    float3 lit = 0.0f;

    float shadow = ComputeShadow(input.WorldPos);

    for (int i = 0; i < LightCount; i++)
    {
        int type = (int)(Lights[i].ColorAndType.w + 0.5f);

        float3 toLight = Lights[i].PosAndIntensity.xyz - input.WorldPos;
        float distSq = dot(toLight, toLight);
        float maxDist = Lights[i].DirAndRange.w;
        if (type != 2 && distSq > maxDist * maxDist)
            continue;

        float3 contrib;
        if (type == 0)
            contrib = CalcPointLight(Lights[i], input.WorldPos, N);
        else if (type == 1)
            contrib = CalcSpotLight(Lights[i], input.WorldPos, N);
        else
            contrib = CalcDirectionalLight(Lights[i], N);

        lit += contrib * shadow;
    }

    lit += AmbientIntensity;

    float4 texColor = DiffuseTexture.Sample(DiffuseSampler, input.TexCoord);
    lit *= input.Color.rgb * texColor.rgb;

    return float4(saturate(lit), input.Color.a);
}

// ── Skybox shaders ──
Texture2D SkyTexture : register(t1, space2);
SamplerState SkySampler : register(s1, space2);

struct SkyVSOutput
{
    float4 Position : SV_Position;
    float3 LocalPos : TEXCOORD0;
};

SkyVSOutput VSSky(VSInput input)
{
    SkyVSOutput output;
    output.Position = mul(MVP, float4(input.Position, 1.0f));
    output.LocalPos = input.Position;
    return output;
}

float4 PSSky(SkyVSOutput input) : SV_Target
{
    float3 dir = normalize(input.LocalPos);

    // Spherical mapping: wrap the 2D sky texture around the box
    float u = 0.5 + atan2(dir.z, dir.x) / 6.2831853;
    float v = 0.5 - asin(dir.y) / 3.1415926;

    float4 skyColor = SkyTexture.Sample(SkySampler, float2(u, v));
    return skyColor;
}
