"""Offline checks of production NR sizing/debounce and Vulkan wiring.

Compiles the actual shared resolution functions and the Vulkan call-site block
with CPU stubs. This does not execute Vulkan, the NR runtime, or the game.
"""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
DLSS5 = ROOT / 'src/unified-20261011/generic/src/addons/dlss5'
OUT = ROOT / 'builds/unified-20261011/nr-resolution-tests'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def main():
    shared = (DLSS5 / 'dlssnr.hpp').read_text(encoding='utf-8')
    vk = (DLSS5 / 'vulkan_path.hpp').read_text(encoding='utf-8')
    # Integration checks protect the split between NR and game image grids.
    contracts = {
        'model_create_is_1_to_1_at_NR_grid': 'SetNrCreateParameters(slot.parameters, nr_width, nr_height, nr_width, nr_height',
        'proxy_allocation': 'case kProxy:\n      case kNeural:\n      case kLookOutput:\n        image_width = nr_width;',
        'full_crop_copy': 'const subresource_box original_box = {0, 0, 0, width, height, 1};',
        'depth_keeps_game_grid': '{"DLSSNR.DepthSubrectWidth", render_width}',
        'motion_keeps_game_grid': '{"DLSSNR.MVecSubrectWidth", motion_width}',
        'motion_keeps_game_scale': 'motion_scale_x * motion_scale_x_multiplier.load());',
        'NR_view_extent': 'ImageResource(views[kProxy], images[kProxy], nr_width, nr_height, false)',
        'model_evaluate_extent': '{"DLSSNR.OutputSubrectWidth", nr_width}',
        'v6_encode_reduces_grid': '*workset, nr_width, nr_height, width, height, 0, 0, nr_width, nr_height, nr_width, nr_height,',
        'v6_resolve_preserves_grid': '*workset, width, height, width, height, 0, 0, resolve_width, resolve_height,',
        'commit_preserves_output_offset': '*workset, width, height, width, height, output_x, output_y, width, height, width, height,',
        'legacy_decode_preserves_output_offset': 'constants.source_base[0] = to_output ? output_x : 0;',
        'legacy_encode_clears_previous_decode_offset': 'constants.source_base[0] = constants.source_base[1] = 0;',
        'workset_key_includes_NR_grid': 'w.nr_width == nr_width && w.nr_height == nr_height',
        'workset_reuse_includes_NR_grid': 'candidate.nr_width == nr_width && candidate.nr_height == nr_height',
        'history_allocation_tracks_NR_grid': 'resource_desc(nr_width, nr_height, 1, 1, format::r32g32b32a32_uint',
        'geometry_invalidates_history': 'for (auto& history : feature.look_history) history.valid = false;',
        'look_motion_maps_to_NR_grid': '* static_cast<float>(nr_width) / static_cast<float>(motion_width);',
        'look_transport_wanted': 'look::TransportWanted(look_settings, nr_width, nr_height, width, height)',
        'look_transport_dispatches_full_grid': 'look_dispatch(look_transport, groups_x, groups_y);',
        'look_compose_dispatches_NR_grid': 'look_dispatch(look_compose, nr_groups_x, nr_groups_y);',
        'retirement_retains_parameters_and_history': 'RetireStagePass(&feature, pass, true);',
        'HDR_zero_strength_copies_full_image': 'cmd->copy_texture_region(images[ref], 0, nullptr, images[write], 0, nullptr);',
    }
    for name, code in contracts.items():
        assert code in vk, f'Vulkan contract missing: {name}'
    shader = (DLSS5 / 'shaders/v6_common.hlsli').read_text(encoding='utf-8')
    assert 'const float l = clamp(luma + correction, -2.0, 2.0);' in shader
    assert 'const float floor_trust = ProxyFloorTrust(proxy);' in shader
    assert 'max(neural, 1.0 / 1024.0) / max(proxy, 1.0 / 1024.0)' in shader
    codec_host = function(shared, 'inline V6CodecConstants MakeV6Constants(')
    assert 'ActiveTransferMode()' in codec_host and 'scene_transfer_mode.load' in function(shared, 'inline uint32_t ActiveTransferMode('), 'Scene bounded-ratio mode must survive NR scaling'
    selection = vk[vk.index('  uint32_t nr_width = width;'):vk.index('  LogNrWorkingResolutionReduced(width, height, nr_width, nr_height);')]
    fixture = r'''
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
namespace reshade::log { enum class level { info }; }
void Log(reshade::log::level, const std::string&) {}
std::atomic_bool nr_follow_input_res{false};
std::atomic_uint32_t nr_resolution_commit_generation{0};
std::atomic<float> nr_resolution_scale{1.f};
constexpr uint32_t kContractDebounceFrames = 3;
struct FeatureState {
  uint32_t input_width=0, input_height=0, output_width=0, output_height=0;
  uint32_t debounce_width=0, debounce_height=0, debounce_frames=0;
  bool working_resolution_committed=false;
  uint32_t commit_generation=0;
};
struct StageFeature { FeatureState ngx; };
'''
    fixture += function(shared, 'inline void ComputeNrWorkingResolution(') + '\n'
    fixture += function(shared, 'inline bool CommitNrResolution(') + '\n'
    fixture += 'std::pair<uint32_t,uint32_t> Apply(StageFeature& feature, uint32_t width, uint32_t height, uint32_t render_width, uint32_t render_height) {\n' + selection + '\nreturn {nr_width,nr_height};\n}\n'
    fixture += r'''
int checks=0, failures=0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while(0)
void Size(uint32_t w,uint32_t h,uint32_t gw,uint32_t gh,bool follow,float scale,uint32_t ew,uint32_t eh) {
  nr_follow_input_res=follow; nr_resolution_scale=scale;
  StageFeature stream; const auto actual=Apply(stream,w,h,gw,gh);
  CHECK(actual.first==ew); CHECK(actual.second==eh);
  CHECK(stream.ngx.output_width==w); CHECK(stream.ngx.output_height==h);
}
int main() {
  // User's 5120x2160, DLSS Quality render grid, at 67%.
  Size(5120,2160,3414,1440,false,.67f,3430,1447); // Upscaled and Present
  Size(3414,1440,3414,1440,false,.67f,2287,965);  // Render
  Size(5120,2160,3414,1440,true,.33f,3414,1440); // Follow ignores percentage
  Size(3414,1440,3414,1440,true,.33f,3414,1440);
  Size(1920,1080,2560,1440,true,.5f,1920,1080);  // Guides cannot enlarge NR
  Size(5120,2160,0,0,true,.67f,3430,1447);       // Missing guides use scale
  Size(5120,2160,3414,1440,false,2.f,5120,2160); // Config >100% clamps
  Size(5120,2160,3414,1440,false,-1.f,1690,713); // Config <33% clamps
  Size(1,1,0,0,false,.33f,1,1);
  Size(1919,1079,1279,719,false,.67f,1286,723); // Odd rounded dimensions
  for (const float scale : {.33f,.5f,.67f,1.f}) {
    nr_resolution_scale=scale; nr_follow_input_res=false;
    for (uint32_t w : {1u,7u,31u,853u,1920u,5120u}) {
      StageFeature s;
      const auto a=Apply(s,w,1079,1,1);
      CHECK(a.first>=1 && a.first<=w); CHECK(a.second>=1 && a.second<=1079);
      for (int pass=0;pass<4;++pass) CHECK(Apply(s,w,1079,1,1)==a);
    }
  }
  StageFeature stable;
  nr_resolution_scale=1.f;
  CHECK(Apply(stable,5120,2160,3414,1440).first==5120);
  CHECK(Apply(stable,5120,2160,3414,1440).first==5120);
  nr_resolution_scale=.67f;
  for (int i=0;i<3;++i) CHECK(Apply(stable,5120,2160,3414,1440).first==5120);
  CHECK(Apply(stable,5120,2160,3414,1440).first==3430); // Commit after settle
  nr_resolution_scale=.5f; ++nr_resolution_commit_generation;
  CHECK(Apply(stable,5120,2160,3414,1440).first==2560); // Explicit UI commit
  nr_resolution_scale=1.f; ++nr_resolution_commit_generation;
  CHECK(Apply(stable,5120,2160,3414,1440).first==5120);
  // A later game-driven change after a consumed UI commit settles again.
  nr_resolution_scale=.67f;
  CHECK(Apply(stable,5120,2160,3414,1440).first==5120);
  for (int i=0;i<2;++i) CHECK(Apply(stable,5120,2160,3414,1440).first==5120);
  CHECK(Apply(stable,5120,2160,3414,1440).first==3430);
  // Two NR layers (Upscaled and Present streams): one UI commit applies to
  // both on their next evaluation, not to the first stream alone.
  {
    StageFeature first, second;
    nr_follow_input_res=false; nr_resolution_scale=.67f;
    for (int i=0;i<2;++i) {  // the second evaluation marks each stream applied
      CHECK(Apply(first,2560,1600,1707,1067).first==1715);
      CHECK(Apply(second,2560,1600,1707,1067).first==1715);
    }
    CHECK(first.ngx.working_resolution_committed && second.ngx.working_resolution_committed);
    nr_resolution_scale=.5f; ++nr_resolution_commit_generation;
    CHECK(Apply(first,2560,1600,1707,1067).first==1280);
    CHECK(Apply(second,2560,1600,1707,1067).first==1280); // was 30 frames late
    CHECK(Apply(first,2560,1600,1707,1067).first==1280);
    CHECK(Apply(second,2560,1600,1707,1067).first==1280);
    // A stream already at the committed dims consumes the commit for itself
    // only: the others still apply it at once.
    StageFeature third;
    nr_resolution_scale=.67f;
    for (int i=0;i<2;++i) CHECK(Apply(third,2560,1600,1707,1067).first==1715);
    ++nr_resolution_commit_generation;
    CHECK(Apply(third,2560,1600,1707,1067).first==1715);
    CHECK(Apply(first,2560,1600,1707,1067).first==1715);
    CHECK(Apply(second,2560,1600,1707,1067).first==1715);
  }
  nr_resolution_scale=1.f;
  // Reference shrinks with DRS: no frame may retain a larger NR grid.
  CHECK(Apply(stable,1280,720,853,480).first==1280);
  CHECK(stable.ngx.input_height==720);
  nr_follow_input_res=true;
  CHECK(Apply(stable,1920,1080,1280,720).first==1280);
  // Alternating guides on a fixed output retain the applied contract.
  for(int i=0;i<20;++i) CHECK(Apply(stable,1920,1080,i%2?1280:960,i%2?720:540).first==1280);
  StageFeature render, upscaled;
  nr_follow_input_res=false; nr_resolution_scale=.67f;
  CHECK(Apply(render,3414,1440,3414,1440).first==2287);
  CHECK(Apply(upscaled,5120,2160,3414,1440).first==3430);
  CHECK(render.ngx.input_width!=upscaled.ngx.input_width);
  std::printf("Production sizing/debounce: %d checks, %d failures\n",checks,failures);
  return failures ? 1 : 0;
}
'''
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'nr-resolution-fixture.cpp').write_text(fixture, encoding='utf-8')
    runner = OUT / 'run-resolution-fixture.cmd'
    runner.write_text('@echo off\nsetlocal\nset "VSCMD_SKIP_SENDTELEMETRY=1"\n'
                      'call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat" x64 >nul\n'
                      'if errorlevel 1 exit /b %errorlevel%\n'
                      'cl /nologo /W4 /WX /O2 /MT /EHsc /std:c++20 /utf-8 nr-resolution-fixture.cpp /Fe:nr-resolution-fixture.exe\n'
                      'if errorlevel 1 exit /b %errorlevel%\n'
                      'nr-resolution-fixture.exe\nexit /b %errorlevel%\n', encoding='ascii')
    env = os.environ.copy()
    env.update(TEMP=str(OUT), TMP=str(OUT))
    result = subprocess.run(['cmd.exe', '/d', '/c', str(runner)], cwd=OUT,
                            env=env, capture_output=True)
    print(result.stdout.decode('utf-8', errors='replace'), end='')
    print(result.stderr.decode('utf-8', errors='replace'), end='')
    if result.returncode:
        raise SystemExit(result.returncode)
    print(f'Vulkan/codec integration: {len(contracts) + 4} contracts passed')
    print('Limit: CPU and source-contract checks only; no Vulkan/NR/game rendering executed.')


if __name__ == '__main__':
    main()
