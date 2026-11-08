// 2D UI geometry for the gpu-ui example: pixel-space quads with a colour, a texture, an alpha-only font atlas
// or a blurred copy of the scene behind them, optionally with rounded corners.
//
// Uniform block (set 0, binding 0) and textures (set 1, texture binding 2n, sampler binding 2n + 1 for slot n)
// follow the conduit conventions that the shader pack builder and Perlin expect.

[[vk::binding(0, 0)]] cbuffer Uniforms : register(b0, space0)
{
    float4 u_screen; // xy: size in pixels of the target being drawn to
    float4 u_mode;   // x: 0 colour * texture, 1 colour with alpha from the font atlas (R8), 2 blurred backdrop tinted by the colour
                     // y: corner radius in pixels, 0 for square corners
    float4 u_box;    // xy: centre of the rounded box in pixels, zw: half size (only used when u_mode.y > 0)
};

[[vk::binding(0, 1)]] Texture2D s_tex : register(t0, space1);
[[vk::binding(1, 1)]] SamplerState s_tex_s : register(s1, space1);
[[vk::binding(2, 1)]] Texture2D s_back : register(t2, space1);
[[vk::binding(3, 1)]] SamplerState s_back_s : register(s3, space1);

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
    [[vk::location(1)]] float4 v_color : TEXCOORD1;
    [[vk::location(2)]] float2 v_pixel : TEXCOORD2;
    [[vk::location(3)]] float2 v_back : TEXCOORD3;
};

struct FsIn
{
    float4 fragCoord : SV_Position;
    [[vk::location(0)]] float2 v_uv : TEXCOORD0;
    [[vk::location(1)]] float4 v_color : TEXCOORD1;
    [[vk::location(2)]] float2 v_pixel : TEXCOORD2;
    [[vk::location(3)]] float2 v_back : TEXCOORD3;
};

[shader("vertex")]
VsOut vsMain(VsIn input)
{
    VsOut output;
    float2 ndc = input.a_position / u_screen.xy * 2.0 - 1.0;
    output.clipPosition = float4(ndc.x, -ndc.y, 0.0, 1.0);
    output.v_uv = input.a_uv;
    output.v_color = input.a_color;
    output.v_pixel = input.a_position;
    output.v_back = input.a_position / u_screen.xy;
    return output;
}

[shader("fragment")]
float4 fsMain(FsIn input) : SV_Target
{
    float4 color = input.v_color;
    if (u_mode.x < 0.5)
        color *= s_tex.Sample(s_tex_s, input.v_uv);
    else if (u_mode.x < 1.5)
        color.a *= s_tex.Sample(s_tex_s, input.v_uv).r;
    else
        color = float4(lerp(s_back.Sample(s_back_s, input.v_back).rgb, color.rgb, color.a), 1.0);

    if (u_mode.y > 0.0)
    {
        float2 q = abs(input.v_pixel - u_box.xy) - u_box.zw + u_mode.y;
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - u_mode.y;
        color.a *= saturate(0.5 - d);
    }
    return color;
}
