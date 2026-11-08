// One direction of a separable gaussian blur (9 taps folded into 5 bilinear fetches).
// Draw it twice, horizontally then vertically, into two textures at a fraction of the screen size.

[[vk::binding(0, 0)]] cbuffer Uniforms : register(b0, space0)
{
    float4 u_screen; // xy: size in pixels of the target being drawn to
    float4 u_blur;   // xy: one texel step in uv units along the blur direction, z: spread multiplier
};

[[vk::binding(0, 1)]] Texture2D s_tex : register(t0, space1);
[[vk::binding(1, 1)]] SamplerState s_tex_s : register(s1, space1);

struct VsIn
{
    [[vk::location(0)]] float2 a_position : TEXCOORD0;
    [[vk::location(1)]] float2 a_uv : TEXCOORD1;
    [[vk::location(2)]] float4 a_color : TEXCOORD2;
};

struct VsOut
{
    float4 clipPosition : SV_Position;
    [[vk::location(0)]] float2 v_uv : TEXCOORD0;
};

struct FsIn
{
    float4 fragCoord : SV_Position;
    [[vk::location(0)]] float2 v_uv : TEXCOORD0;
};

[shader("vertex")]
VsOut vsMain(VsIn input)
{
    VsOut output;
    float2 ndc = input.a_position / u_screen.xy * 2.0 - 1.0;
    output.clipPosition = float4(ndc.x, -ndc.y, 0.0, 1.0);
    output.v_uv = input.a_uv;
    return output;
}

[shader("fragment")]
float4 fsMain(FsIn input) : SV_Target
{
    float2 step = u_blur.xy * u_blur.z;
    float4 sum = s_tex.Sample(s_tex_s, input.v_uv) * 0.2270270270;
    sum += s_tex.Sample(s_tex_s, input.v_uv + step * 1.3846153846) * 0.3162162162;
    sum += s_tex.Sample(s_tex_s, input.v_uv - step * 1.3846153846) * 0.3162162162;
    sum += s_tex.Sample(s_tex_s, input.v_uv + step * 3.2307692308) * 0.0702702703;
    sum += s_tex.Sample(s_tex_s, input.v_uv - step * 3.2307692308) * 0.0702702703;
    return float4(sum.rgb, 1.0);
}
