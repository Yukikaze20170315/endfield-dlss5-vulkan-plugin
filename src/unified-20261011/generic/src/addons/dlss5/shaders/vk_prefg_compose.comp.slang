// Vulkan pre-FG HUD composite (vulkan_path.hpp RunPreFg): the real frame
// recomposed from NR's HUD-less frame with the game's HUD kept.
//
// The same difference-mask blend the dlss5-bridge FG-input feed ran on its
// private D3D12 device (fg-composite-shader.h): NR's change to the HUD-less
// frame is applied in full where the back buffer equals the HUD-less frame
// (no UI touched the pixel) and fades out with the UI's contribution, so a
// translucent panel keeps a soft scene under it and opaque UI stays as drawn.
// No DLSSG.UI / DLSSG.UIAlpha is read: the titles this serves tag neither.
//
// Codec layout (EnsureCodec): t0 the HUD-less frame as the game handed it,
// t1 NR's HUD-less frame, t2 the back buffer (the real frame), t3 unused,
// u0 the composite (a separate image, so no storage read is needed).
#define RENODX_VULKAN 1
#include "vk_binding.hlsli"
VK_BINDING(0) Texture2D<float4> HudLess : register(t0);
VK_BINDING(1) Texture2D<float4> Enhanced : register(t1);
VK_BINDING(2) Texture2D<float4> Back : register(t2);
VK_BINDING(3) Texture2D<float4> Unused : register(t3);
VK_BINDING(4) RWTexture2D<float4> Composite : register(u0);

PUSH_CONSTANTS(ComposeConstants) {
  uint2 Size;
  float UiFade;  // encoded-domain difference at which a pixel counts as all UI
  uint Pad;
};

[numthreads(16, 16, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID) {
  const uint2 pixel = dispatch_id.xy;
  if (any(pixel >= Size)) return;
  const float4 back = Back.Load(int3(pixel, 0));
  const float3 scene = HudLess.Load(int3(pixel, 0)).rgb;
  const float3 enhanced = Enhanced.Load(int3(pixel, 0)).rgb;
  const float3 difference = abs(back.rgb - scene);
  const float ui = max(difference.r, max(difference.g, difference.b));
  const float weight = saturate(1.0 - ui / max(UiFade, 1.0 / 1024.0));
  Composite[pixel] = float4(saturate(back.rgb + (enhanced - scene) * weight), back.a);
}
