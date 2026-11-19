//
// Glow around actors for the DirectX 9 hook (example_kf1_overlay_dll), nullcore's Outline with its Classic style
// (nullcore/assets/shaders/d3d9/glow/): the HLSL below is common.hlsl cut down to what classic.hlsl reads, classic.hlsl,
// main.hlsl, blur.hlsl, mask.hlsl and vs.hlsl.
//
// One of these is one colour: the game's own draws of its actors are repeated into a render target of ours in white,
// with no depth surface, which makes the sharp silhouettes (B). Once the level is drawn they are blurred, B -> C along X
// and C -> A along Y times the thickness. A is laid over the game's target in that colour as a separate step, because of
// the game's HUD: HUDKillingFloor::DrawModOverlay draws KFX.SepiaShader over the screen, whose OutputBlending is
// OB_Modulate, 2 * source * frame. What it multiplies by depends on the zone's colour and on the material, which changes
// with the player's health. Anything laid under it comes out as whatever it does to the frame, a white source doubled
// the glow and turned a pale colour white. So dllmain.cpp lays A right after that draw, which leaves the colour exact.
// The first person weapon is drawn before the overlay: it goes into B after the blur, which keeps the glow off it.
//
// nullcore keeps the glow off the models with the stencil of a pass of the engine's. There is no such pass here, so
// laying A does it with B instead. It also adds the light to the frame in linear space, the way the engine it was
// written for does. Here A is the coverage of a plain blend in the frame's own sRGB values instead, so the glow reads as
// the one colour over a bright wall too.
//
// The device is put back exactly as the game left it: the renderer caches device state and never reads it back.

#pragma once
#include <array>
#include <d3d9.h>
#include <d3dcommon.h>
#include <string>
#include <string_view>
#include <utility>

namespace kf1_overlay
{
    constexpr std::string_view k_glow_vertex_shader = R"hlsl(
struct VsOut
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};
VsOut main(float4 pos : POSITION, float2 uv : TEXCOORD0)
{
    VsOut o;
    o.pos = pos;
    o.uv = uv;
    return o;
}
)hlsl";

    constexpr std::string_view k_glow_mask_shader = R"hlsl(
float4 main() : COLOR
{
    return 1;
}
)hlsl";

    constexpr std::string_view k_glow_blur_shader = R"hlsl(
sampler2D s0 : register(s0);
float4 c0 : register(c0); // xy: one texel of the source along the blur's axis
float4 c1 : register(c1); // x: the scale

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float2 d = c0.xy;
    float4 c = tex2D(s0, uv) * 0.2013;
    c += (tex2D(s0, uv + d * 1.3366) + tex2D(s0, uv - d * 1.3366)) * 0.2185;
    c += (tex2D(s0, uv + d * 3.4295) + tex2D(s0, uv - d * 3.4295)) * 0.0821;
    c += (tex2D(s0, uv + d * 5.4264) + tex2D(s0, uv - d * 5.4264)) * 0.0461;
    c += (tex2D(s0, uv + d * 7.4359) + tex2D(s0, uv - d * 7.4359)) * 0.0262;
    c += (tex2D(s0, uv + d * 9.4436) + tex2D(s0, uv - d * 9.4436)) * 0.0162;
    c += (tex2D(s0, uv + d * 11.4401) + tex2D(s0, uv - d * 11.4401)) * 0.0102;
    c.rgb *= c1.x;
    return c;
}
)hlsl";

    constexpr std::string_view k_glow_classic_shader = R"hlsl(
sampler2D s0 : register(s0);
sampler2D s1 : register(s1);
float4 c0 : register(c0);

float glow_at(float2 p)
{
    return tex2Dlod(s0, float4(p, 0, 0)).r;
}

float body_at(float2 p)
{
    return tex2Dlod(s1, float4(p, 0, 0)).r;
}

// the blurred silhouettes over the frame in the group's colour, premultiplied, and none of it on the models
float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float g = glow_at(uv) * (1.0 - body_at(uv));
    return float4(c0.rgb * g, g);
}
)hlsl";

    class ActorGlow final
    {
    public:
        // Makes what the glow draws with on first use, again after a release. False once anything was refused, which
        // error() explains, the glow then stays off.
        [[nodiscard]]
        bool prepare(IDirect3DDevice9* device, const UINT width, const UINT height)
        {
            if (!m_error.empty())
                return false;

            if (m_state_block)
                return true;

            m_width = width;
            m_height = height;
            m_vertex_shader = create_vertex_shader(device);
            m_mask_shader = create_pixel_shader(device, k_glow_mask_shader, "ps_2_0");
            m_blur_shader = create_pixel_shader(device, k_glow_blur_shader, "ps_3_0");
            m_classic_shader = create_pixel_shader(device, k_glow_classic_shader, "ps_3_0");

            for (auto& target : m_targets)
                create_target(device, target);

            if (m_error.empty() && FAILED(device->CreateStateBlock(D3DSBT_ALL, &m_state_block)))
                m_error = "no state block";

            if (m_error.empty())
                return true;

            release();

            return false;
        }

        [[nodiscard]]
        const std::string& error() const
        {
            return m_error;
        }

        // Has to happen before the device is reset or thrown away: the targets are D3DPOOL_DEFAULT, and all of it keeps
        // the device alive
        void release()
        {
            for (auto& target : m_targets)
            {
                release_com(target.surface);
                release_com(target.texture);
            }

            release_com(m_state_block);
            release_com(m_classic_shader);
            release_com(m_blur_shader);
            release_com(m_mask_shader);
            release_com(m_vertex_shader);
            discard();
        }

        // Forgets the silhouettes drawn since the last glow was laid
        void discard()
        {
            m_mask_drawn = false;
            m_blurred = false;
        }

        // Repeats one of the game's draws into B, draw_again being that draw. A draw into a target of another size is
        // not the view: a shadow being rendered, for one.
        template<class DrawAgain>
        void draw_silhouette(IDirect3DDevice9* device, const DrawAgain& draw_again)
        {
            IDirect3DSurface9* game_target{};

            if (!m_state_block || FAILED(device->GetRenderTarget(0, &game_target)))
                return;

            D3DSURFACE_DESC description{};

            if (SUCCEEDED(game_target->GetDesc(&description)) && description.Width == m_width
                && description.Height == m_height)
                redirect_draw(device, game_target, draw_again);

            game_target->Release();
        }

        // Blurs the silhouettes drawn since the last glow was laid, lay() then puts them on screen. True when there were
        // any.
        bool blur(IDirect3DDevice9* device, const float thickness)
        {
            if (!m_mask_drawn || m_blurred)
                return false;

            m_blurred = with_composite_states(device,
                                              [this, device, thickness](IDirect3DSurface9*)
                                              {
                                                  return run_blur(device, thickness);
                                              });

            if (!m_blurred)
                m_mask_drawn = false;

            return m_blurred;
        }

        // Keeps the glow off what one of the game's draws covers, draw_again being that draw. It goes between blur() and
        // lay(), the glow around it is not affected.
        template<class DrawAgain>
        void draw_cutout(IDirect3DDevice9* device, const DrawAgain& draw_again)
        {
            if (m_blurred)
                draw_silhouette(device, draw_again);
        }

        // Lays what blur() made over the game's target, the colour being sRGB. True when it was drawn.
        bool lay(IDirect3DDevice9* device, const std::array<float, 3>& color)
        {
            if (!m_blurred)
                return false;

            discard();

            return with_composite_states(device,
                                         [this, device, &color](IDirect3DSurface9* game_target)
                                         {
                                             const std::array<float, 4> constant{color[0], color[1], color[2], 1.f};
                                             const auto quad = clip_quad();

                                             device->SetRenderTarget(0, game_target);
                                             set_blend_states(device);
                                             device->SetTexture(0, m_targets[k_a].texture);
                                             device->SetTexture(1, m_targets[k_b].texture);
                                             device->SetPixelShader(m_classic_shader);
                                             device->SetPixelShaderConstantF(0, constant.data(), 1);

                                             return SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2,
                                                                                      quad.data(), sizeof(ClipVertex)));
                                         });
        }

    private:
        struct Target final
        {
            IDirect3DTexture9* texture{};
            IDirect3DSurface9* surface{};
        };

        struct ClipVertex final
        {
            float x, y, z, u, v;
        };

        static constexpr DWORD k_clip_vertex_fvf = D3DFVF_XYZ | D3DFVF_TEX1;

        // A: the blurred silhouettes, B: the sharp ones, C: the blur in between
        static constexpr std::size_t k_a = 0;
        static constexpr std::size_t k_b = 1;
        static constexpr std::size_t k_c = 2;

        static constexpr std::array<std::pair<D3DRENDERSTATETYPE, DWORD>, 7> k_silhouette_states{
                {{D3DRS_ZENABLE, FALSE},
                 {D3DRS_STENCILENABLE, FALSE},
                 {D3DRS_ALPHABLENDENABLE, FALSE},
                 {D3DRS_ALPHATESTENABLE, FALSE},
                 {D3DRS_FOGENABLE, FALSE},
                 {D3DRS_SRGBWRITEENABLE, FALSE},
                 {D3DRS_COLORWRITEENABLE, 0xF}}};

        template<class Com>
        static void release_com(Com*& object)
        {
            if (!object)
                return;

            object->Release();
            object = nullptr;
        }

        // d3dcompiler_47.dll comes with Windows and the game never loads it itself. <d3dcompiler.h> does not parse after
        // the headers this one has to come after, so its D3DCompile is spelled out.
        [[nodiscard]]
        ID3DBlob* compile(const std::string_view source, const char* profile)
        {
            using D3DCompile = HRESULT(WINAPI*)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR,
                                                LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

            static const auto d3d_compile = []() -> D3DCompile
            {
                const auto library = LoadLibraryW(L"d3dcompiler_47.dll");

                return library ? reinterpret_cast<D3DCompile>(GetProcAddress(library, "D3DCompile")) : nullptr;
            }();

            if (!d3d_compile)
            {
                m_error = "d3dcompiler_47.dll is not available";
                return nullptr;
            }

            ID3DBlob* code{};
            ID3DBlob* messages{};

            if (FAILED(d3d_compile(source.data(), source.size(), nullptr, nullptr, nullptr, "main", profile, 0, 0, &code,
                                   &messages)))
                m_error = messages ? static_cast<const char*>(messages->GetBufferPointer()) : "a shader did not compile";

            release_com(messages);

            return code;
        }

        [[nodiscard]]
        IDirect3DVertexShader9* create_vertex_shader(IDirect3DDevice9* device)
        {
            auto* code = compile(k_glow_vertex_shader, "vs_3_0");

            if (!code)
                return nullptr;

            IDirect3DVertexShader9* shader{};

            if (FAILED(device->CreateVertexShader(static_cast<const DWORD*>(code->GetBufferPointer()), &shader)))
                m_error = "the vertex shader was refused";

            code->Release();

            return shader;
        }

        [[nodiscard]]
        IDirect3DPixelShader9* create_pixel_shader(IDirect3DDevice9* device, const std::string_view source,
                                                   const char* profile)
        {
            auto* code = compile(source, profile);

            if (!code)
                return nullptr;

            IDirect3DPixelShader9* shader{};

            if (FAILED(device->CreatePixelShader(static_cast<const DWORD*>(code->GetBufferPointer()), &shader)))
                m_error = "a pixel shader was refused";

            code->Release();

            return shader;
        }

        void create_target(IDirect3DDevice9* device, Target& target)
        {
            for (const auto format : {D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8})
            {
                if (SUCCEEDED(device->CreateTexture(m_width, m_height, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT,
                                                    &target.texture, nullptr))
                    && SUCCEEDED(target.texture->GetSurfaceLevel(0, &target.surface)))
                    return;

                release_com(target.texture);
            }

            m_error = "the render targets were refused";
        }

        // The device is not a pure one, so what is changed here can be read back from it
        template<class DrawAgain>
        void redirect_draw(IDirect3DDevice9* device, IDirect3DSurface9* game_target, const DrawAgain& draw_again)
        {
            if (!m_mask_drawn)
                device->ColorFill(m_targets[k_b].surface, nullptr, D3DCOLOR_XRGB(0, 0, 0));

            m_mask_drawn = true;

            IDirect3DSurface9* game_depth{};
            IDirect3DPixelShader9* game_shader{};
            D3DVIEWPORT9 game_viewport{};
            std::array<DWORD, k_silhouette_states.size()> game_states{};

            device->GetDepthStencilSurface(&game_depth);
            device->GetPixelShader(&game_shader);
            device->GetViewport(&game_viewport);

            for (auto state = 0uz; state < k_silhouette_states.size(); ++state)
            {
                device->GetRenderState(k_silhouette_states[state].first, &game_states[state]);
                device->SetRenderState(k_silhouette_states[state].first, k_silhouette_states[state].second);
            }

            device->SetRenderTarget(0, m_targets[k_b].surface);
            device->SetDepthStencilSurface(nullptr);
            device->SetViewport(&game_viewport);
            device->SetPixelShader(m_mask_shader);

            draw_again();

            device->SetPixelShader(game_shader);
            device->SetRenderTarget(0, game_target);
            device->SetDepthStencilSurface(game_depth);
            device->SetViewport(&game_viewport);

            for (auto state = 0uz; state < k_silhouette_states.size(); ++state)
                device->SetRenderState(k_silhouette_states[state].first, game_states[state]);

            release_com(game_shader);
            release_com(game_depth);
        }

        // Runs `pass`, handing it the game's target, and puts everything back the way the game left it. Returns what
        // `pass` did.
        template<class Pass>
        bool with_composite_states(IDirect3DDevice9* device, const Pass& pass)
        {
            IDirect3DSurface9* game_target{};
            IDirect3DSurface9* game_depth{};

            if (FAILED(device->GetRenderTarget(0, &game_target)))
                return false;

            device->GetDepthStencilSurface(&game_depth);
            m_state_block->Capture();
            set_composite_states(device);

            const auto done = pass(game_target);

            // The block's Apply puts the old textures back. A render target resets the viewport, so it goes first.
            device->SetTexture(0, nullptr);
            device->SetTexture(1, nullptr);
            device->SetRenderTarget(0, game_target);
            device->SetDepthStencilSurface(game_depth);
            m_state_block->Apply();

            game_target->Release();
            release_com(game_depth);

            return done;
        }

        [[nodiscard]]
        bool run_blur(IDirect3DDevice9* device, const float thickness)
        {
            const auto width = static_cast<float>(m_width);
            const auto quad = clip_quad();

            // One blur pass: `from` -> `to`, a step of one texel along the axis, the colour times `scale`
            const auto pass = [&](const Target& from, const Target& to, const float step_x, const float step_y,
                                  const float scale)
            {
                device->SetTexture(0, nullptr);
                device->SetRenderTarget(0, to.surface);
                device->SetTexture(0, from.texture);

                const std::array<float, 8> constants{step_x, step_y, 0.f, 0.f, scale, 0.f, 0.f, 0.f};
                device->SetPixelShaderConstantF(0, constants.data(), 2);

                return SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad.data(), sizeof(ClipVertex)));
            };

            device->SetPixelShader(m_blur_shader);

            // The Y pass steps by 1 / width too, as the engine's BlurFilterY does: it is the look
            return pass(m_targets[k_b], m_targets[k_c], 1.f / width, 0.f, 1.f)
                   && pass(m_targets[k_c], m_targets[k_a], 0.f, 1.f / width, thickness);
        }

        // Half a texel off, so the quad's texels land on the pixels
        [[nodiscard]]
        std::array<ClipVertex, 4> clip_quad() const
        {
            const auto half_x = 1.f / static_cast<float>(m_width);
            const auto half_y = 1.f / static_cast<float>(m_height);

            return {{{-1.f - half_x, 1.f + half_y, 0.f, 0.f, 0.f},
                     {1.f - half_x, 1.f + half_y, 0.f, 1.f, 0.f},
                     {-1.f - half_x, -1.f + half_y, 0.f, 0.f, 1.f},
                     {1.f - half_x, -1.f + half_y, 0.f, 1.f, 1.f}}};
        }

        void set_composite_states(IDirect3DDevice9* device) const
        {
            device->SetRenderState(D3DRS_ZENABLE, FALSE);
            device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
            device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
            device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
            device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
            device->SetRenderState(D3DRS_FOGENABLE, FALSE);
            device->SetRenderState(D3DRS_CLIPPING, TRUE);
            device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
            device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);

            for (DWORD stage = 0; stage < 2; ++stage)
            {
                device->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                device->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                device->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                device->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                device->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                device->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
                device->SetTexture(stage, nullptr);
            }

            device->SetVertexShader(m_vertex_shader);
            device->SetFVF(k_clip_vertex_fvf);
            device->SetDepthStencilSurface(nullptr);
        }

        // Premultiplied over the game's target, rgb + frame * (1 - a)
        static void set_blend_states(IDirect3DDevice9* device)
        {
            device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
            device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
        }

        std::string m_error;
        UINT m_width{};
        UINT m_height{};
        IDirect3DVertexShader9* m_vertex_shader{};
        IDirect3DPixelShader9* m_mask_shader{};
        IDirect3DPixelShader9* m_blur_shader{};
        IDirect3DPixelShader9* m_classic_shader{};
        // Made last, so it also says that everything else is there
        IDirect3DStateBlock9* m_state_block{};
        std::array<Target, 3> m_targets{};
        bool m_mask_drawn{};
        bool m_blurred{};
    };
} // namespace kf1_overlay
