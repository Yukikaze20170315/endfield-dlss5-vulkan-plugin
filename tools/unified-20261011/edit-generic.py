from pathlib import Path
root = Path(__file__).resolve().parents[2]
p = root / 'src/unified-20261011/generic/src/addons/dlss5/dlssnr.hpp'
s = p.read_text(encoding='utf-8')
def replace(a,b):
    global s
    assert s.count(a) == 1, (a[:100], s.count(a))
    s = s.replace(a,b)
replace('inline std::filesystem::path FindNrRuntimePath() {', '''inline std::filesystem::path FindNrRuntimePath() {
  // The bridge owns the game-specific GPU/runtime mapping; Generic consumes
  // a versioned export so a full-path LoadLibrary cannot bypass that choice.
  const HMODULE bridge = GetModuleHandleW(L"dlss5-bridge.addon64");
  using RuntimePath = int (__cdecl*)(wchar_t*, uint32_t);
  auto selected = bridge ? reinterpret_cast<RuntimePath>(GetProcAddress(bridge, "DLSS5GetNrRuntimePathV1")) : nullptr;
  if (selected) {
    wchar_t path[32768] = {};
    const int result = selected(path, 32768);
    if (result > 0) return std::filesystem::path(path);
    if (result < 0) return AddonDirectory() / L"dlss5-runtime-unavailable" / L"nvngx_dlssnr.dll";
  }''')
replace('''      uint32_t previous = stages::configuration.load(std::memory_order_acquire);
      while (!stages::configuration.compare_exchange_weak(
          previous, stages::Plan{previous}.With(static_cast<stages::Point>(stage), count).packed,
          std::memory_order_acq_rel, std::memory_order_acquire)) {}''', '''      stages::trial::Request(stages::Snapshot().With(static_cast<stages::Point>(stage), count));''')
replace('  stages::configuration.store(saved_plan.packed, std::memory_order_release);', '  stages::trial::Initialize(saved_plan);')
# Persist the requested values separately from the temporary safety override.
replace('''      const stages::Plan plan = stages::Snapshot();
      reshade::set_config_value''', '''      const stages::Plan plan{stages::configuration.load(std::memory_order_acquire)};
      reshade::set_config_value''')
replace('inline void OnPresent(\n', 'inline void PollLayerTrial();\n\ninline void OnPresent(\n')
replace('''  // Counted before ANY early return below it: the question this answers is''', '''  PollLayerTrial();
  // Counted before ANY early return below it: the question this answers is''')
insert='''inline void PollLayerTrial() {
  if (stages::trial::Poll()) {
    kSetRenderPasses.Persist();
    RecreateFeatures();
  }
}
inline void DrawLayerTrial(reshade::api::effect_runtime*) {
  PollLayerTrial();
  const uint64_t due = stages::trial::deadline.load();
  if (!due) return;
  const uint64_t now = GetTickCount64();
  ImGui::SetNextWindowSize(ImVec2(570, 0), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("DLSS5 层数确认 / Confirm layer count", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
    ImGui::TextWrapped("5 层及以上会显著增加显存、GPU 负载和延迟，可能导致卡顿或设备丢失。 / 5+ layers can exhaust VRAM and stall rendering.");
    ImGui::Text("%u 秒后回退 / seconds until restore", static_cast<unsigned>((due > now ? due-now : 0) + 999) / 1000);
    ImGui::BeginDisabled(now + 13000 < due || now >= due);
    // Mouse-only activation prevents Enter/Space and the initiating click from confirming.
    ImGui::Button("保留 / Keep");
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) stages::trial::Resolve(true);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("立即回退 / Restore now")) stages::trial::Resolve(false);
  }
  ImGui::End();
}
inline void LayerCaption(uint32_t channel, char* text, size_t size) {
  const auto plan = stages::Snapshot();
  const auto point = channel < plan.Count(stages::Point::Render) ? stages::Point::Render :
      channel < plan.Offset(stages::Point::Present) ? stages::Point::Upscaled : stages::Point::Present;
  const char* stage = point == stages::Point::Render ? "渲染 / Render" : point == stages::Point::Upscaled ? "超分后 / Upscaled" : "呈现 / Present";
  std::snprintf(text, size, "%s · %u（全局 / global %u）", stage, channel - plan.Offset(point) + 1, channel + 1);
}
'''
replace('inline void DrawSectionStackingResolution(const ui::PanelText& text) {', insert + '\ninline void DrawSectionStackingResolution(const ui::PanelText& text) {')
replace('''    char caption[64];
    std::snprintf(caption, sizeof(caption), text.Tr(text.Pick("Stack pass %u", "Pass %u")),
                  pass + 1);
    if (text.modern) {
      ui::SubCaption(caption);
    } else {
      ImGui::TextUnformatted(caption);
    }''','''    char caption[160];
    LayerCaption(pass, caption, sizeof(caption));
    ImGui::SeparatorText(caption);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("配置键 / Settings: NRPass%u*", pass + 1);''')
replace('''  char first_channel[64];
  std::snprintf(first_channel, sizeof(first_channel), text.Tr("Pass %u"), 1u);
  ui::SubCaption(first_channel);''','''  char first_channel[160];
  LayerCaption(0, first_channel, sizeof(first_channel));
  ImGui::SeparatorText(first_channel);
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("配置键 / Settings: NR*");''')
replace('''  RefreshUiLanguage();
  ui::reset_buttons_drawn = 0;''','''  RefreshUiLanguage();
  if (!stages::trial::status.empty()) ImGui::TextWrapped("%s", stages::trial::status.c_str());
  ui::reset_buttons_drawn = 0;''')
replace('''      reshade::register_overlay(internal::kOverlayTitle, internal::OnOverlay);''','''      reshade::register_overlay(internal::kOverlayTitle, internal::OnOverlay);
      reshade::register_event<reshade::addon_event::reshade_overlay>(internal::DrawLayerTrial);''')
replace('''      reshade::unregister_overlay(internal::kOverlayTitle, internal::OnOverlay);''','''      reshade::unregister_overlay(internal::kOverlayTitle, internal::OnOverlay);
      reshade::unregister_event<reshade::addon_event::reshade_overlay>(internal::DrawLayerTrial);''')
s=s.replace('8.5.0-rc10-stages1','8.5.0-rc10-unified1')
p.write_text(s, encoding='utf-8')
print('Generic changes applied')
