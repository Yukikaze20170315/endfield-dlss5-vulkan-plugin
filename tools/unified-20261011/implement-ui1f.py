"""Apply the unified1f panel changes once, retaining exact pre-edit sources."""
from pathlib import Path
import hashlib, json, shutil

ROOT = Path(__file__).resolve().parents[2]
BACKUP = ROOT / 'evidence/unified-20261011/source-before-1f'
if BACKUP.exists():
    raise SystemExit('Source backup exists; do not replay edits')
BACKUP.mkdir(parents=True)
records = {}

def read(name):
    p = ROOT / name
    data = p.read_bytes()
    dest = BACKUP / name
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(data)
    records[name] = hashlib.sha256(data).hexdigest()
    return data.decode('utf-8-sig').replace('\r\n', '\n')

def write(name, text):
    (ROOT / name).write_text(text, encoding='utf-8', newline='\r\n')

def replace(s, a, b):
    assert s.count(a) == 1, (a[:100], s.count(a))
    return s.replace(a, b)

name = 'src/unified-20261011/generic/src/addons/dlss5/dlssnr.hpp'
s = read(name)
s = replace(s, 'inline void DrawSectionHookPoint(const ui::PanelText& text) {',
    'inline bool ui_advanced_settings = false;\n\ninline void DrawSectionHookPoint(const ui::PanelText& text) {')
s = replace(s, '  if (text.modern) ui::RowLabel("");\n  {\n    constexpr const char* kPresentGuideModes',
    '  if (text.modern) ui::RowLabel("");\n  if (ui_advanced_settings) {\n    constexpr const char* kPresentGuideModes')
s = s.replace('  DrawSectionLanguage(text);\n', '')
s = replace(s, '  RefreshUiLanguage();\n  DrawLayerTrial();', '''  RefreshUiLanguage();
  DrawSectionLanguage(ui::kModernText);
  RefreshUiLanguage();
  ImGui::Checkbox(ui::Tr("Advanced settings###nr_advanced"), &ui_advanced_settings);
  DrawLayerTrial();''')
s = replace(s, '  const bool classic = ui_classic_layout.load();', '  const bool classic = ui_advanced_settings && ui_classic_layout.load();')
s = s.replace('      if (ui_classic_layout.load()) {\n        DrawPanelBody', '      if (ui_advanced_settings && ui_classic_layout.load()) {\n        DrawPanelBody')
s = replace(s, '  bool reset = false;\n  char hotkeys[128];', '  if (!ui_advanced_settings) return;\n\n  bool reset = false;\n  char hotkeys[128];')
s = replace(s, '      if (card.action == ui::CardAction::kOpenDiagnostics) ui_open_diagnostics = true;',
    '      if (card.action == ui::CardAction::kOpenDiagnostics) { ui_advanced_settings = true; ui_open_diagnostics = true; }')
s = s.replace('"DLSS5 层数确认 / Confirm layer count"', 'ui::Tr("Confirm DLSS5 layer count")')
s = s.replace('"5 层及以上会显著增加显存、GPU 负载和延迟，可能导致卡顿或设备丢失。 / 5+ layers can exhaust VRAM and stall rendering."', '"%s", ui::Tr("5+ layers increase VRAM use, GPU load and latency; rendering may stall or the device may be lost.")')
s = s.replace('"%u 秒后回退 / seconds until restore"', 'ui::Tr("Restore previous layers in %u seconds")')
s = s.replace('"保留 / Keep"', 'ui::Tr("Keep###layer_keep")').replace('"立即回退 / Restore now"', 'ui::Tr("Restore now###layer_restore")')
s = replace(s, '  const char* stage = point == stages::Point::Render ? "渲染 / Render" : point == stages::Point::Upscaled ? "超分后 / Upscaled" : "呈现 / Present";',
    '  const char* stage = ui::Tr(point == stages::Point::Render ? "Render" : point == stages::Point::Upscaled ? "Upscaled" : "Present");')
s = s.replace('"%s · %u（全局 / global %u）"', 'ui::Tr("%s layer %u (global %u)")')
s = s.replace('"配置键 / Settings: NRPass%u*"', 'ui::Tr("Settings: NRPass%u*")').replace('"配置键 / Settings: NR*"', '"%s", ui::Tr("Settings: NR*")')
s = replace(s, 'if (!stages::trial::status.empty()) ImGui::TextWrapped("%s", stages::trial::status.c_str());',
    'if (!stages::trial::status.empty()) ImGui::TextWrapped("%s", dlss5_panel::SelectText(stages::trial::status, ui::CurrentLanguage() == 13 || ui::CurrentLanguage() == 14).c_str());')
s = '#include "../../../../common/panel-locale.hpp"\n' + s
write(name, s)

name = 'src/unified-20261011/generic/src/addons/dlss5/ui/lang/zh-Hans.hpp'
s = read(name)
translations = {
    'Advanced settings###nr_advanced': '高级设置###nr_advanced',
    'Confirm DLSS5 layer count': '确认 DLSS5 层数',
    '5+ layers increase VRAM use, GPU load and latency; rendering may stall or the device may be lost.': '5 层及以上会增加显存占用、GPU 负载和延迟，可能导致卡顿或设备丢失。',
    'Restore previous layers in %u seconds': '%u 秒后恢复之前的层数',
    'Keep###layer_keep': '保留###layer_keep',
    'Restore now###layer_restore': '立即回退###layer_restore',
    'Render': '渲染', 'Upscaled': '超分后', 'Present': '呈现',
    '%s layer %u (global %u)': '%s第 %u 层（全局第 %u 层）',
    'Settings: NRPass%u*': '配置键：NRPass%u*', 'Settings: NR*': '配置键：NR*',
}
lines = ''.join('    {' + json.dumps(k, ensure_ascii=False) + ', ' + json.dumps(v, ensure_ascii=False) + '},\n' for k,v in translations.items())
s = replace(s, 'inline constexpr Translation kZhHans[] = {\n', 'inline constexpr Translation kZhHans[] = {\n' + lines)
write(name, s)

name = 'src/unified-20261011/bridge/src/synth.inc'
s = read(name)
s = replace(s, 'static void DeploymentDraw();', 'static void DeploymentDraw();\nstatic bool DeploymentPanelHeader();')
s = replace(s, '    DeploymentDraw();\n    const LONG src', '    const bool advanced = DeploymentPanelHeader();\n    DeploymentDraw();\n    if (!advanced) return;\n    const LONG src')
write(name, s)

name = 'src/unified-20261011/bridge/src/deployment-panel.inc'
s = read(name)
s = '#include "../../common/panel-locale.hpp"\n' + s
marker = 'static HANDLE g_driver_maintenance=nullptr;'
s = replace(s, marker, marker + '''
static dlss5_panel::Preference g_panel_language;
static bool g_panel_advanced=false;
static bool DeploymentPanelHeader() {
    g_panel_language.Load(deployment::dir);
    const auto* ui=g_panel_gui;
    int selected=g_panel_language.chinese ? 1:0;
    bool changed=ui->RadioButton2("English##bridge_en",&selected,0);
    ui->SameLine(0.f,-1.f);
    changed=ui->RadioButton2("简体中文##bridge_zh",&selected,1) || changed;
    if(changed) g_panel_language.Store(deployment::dir,selected==1);
    ui->Checkbox(g_panel_language.chinese ? "高级设置###bridge_advanced":"Advanced settings###bridge_advanced",&g_panel_advanced);
    return g_panel_advanced;
}
static std::string DeploymentText(const std::string& value) {
    return dlss5_panel::SelectText(value,g_panel_language.chinese);
}
''')
start=s.index('static void DeploymentDraw()')
end=s.index('\n#include <tlhelp32.h>',start)
part=s[start:end]
part=part.replace('ui->TextUnformatted(value.c_str(),nullptr)', 'ui->TextUnformatted(DeploymentText(value).c_str(),nullptr)')
# Literal labels use the same language selection as dynamically reported status.
import re
part=re.sub(r'(ui->(?:SeparatorText|RadioButton2|Checkbox)\()((?:"[^"\\]*(?:\\.[^"\\]*)*"))', r'\1DeploymentText(\2).c_str()',part)
part=replace(part,'    int early=static_cast<int>(state.early);','    if(g_panel_advanced) {\n    int early=static_cast<int>(state.early);')
part=replace(part,'    ui->SeparatorText(DeploymentText("故障与恢复 / Recovery").c_str());','    }\n    ui->SeparatorText(DeploymentText("故障与恢复 / Recovery").c_str());')
part=replace(part,'    bool safe_next=state.safe!=0;', '    if(g_panel_advanced || safe.load() || state.safe) {\n    bool safe_next=state.safe!=0;')
part=replace(part,'    if(ui->Checkbox(DeploymentText("导出诊断 / Export diagnostics").c_str(),&export_data) && export_data) DeploymentExportDiagnostics(game,dir);', '    if(ui->Checkbox(DeploymentText("导出诊断 / Export diagnostics").c_str(),&export_data) && export_data) DeploymentExportDiagnostics(game,dir);\n    }')
s=s[:start]+part+s[end:]
write(name,s)

name='src/unified-20261011/mfg/addon/addon.cpp'
s=read(name)
s=replace(s,'void OnRegisterOverlay(reshade::api::effect_runtime* /*runtime*/) {','void DrawAdvancedOverlay(reshade::api::effect_runtime* /*runtime*/) {')
pos=s.index('void DrawAdvancedOverlay(')
wrapper='''// Basic controls do not change hidden compatibility settings.
static bool g_ui_chinese=true;
static bool g_ui_language_loaded=false;
static bool g_ui_advanced=false;
static const char* MfgText(const char* english,const char* chinese) { return g_ui_chinese ? chinese:english; }
void OnRegisterOverlay(reshade::api::effect_runtime* runtime) {
  if(!g_ui_language_loaded) {
    int language=1;
    reshade::get_config_value(nullptr,kConfigSection,"UiChinese",language);
    g_ui_chinese=language!=0; g_ui_language_loaded=true;
  }
  int language=g_ui_chinese ? 1:0;
  bool changed=ImGui::RadioButton("English##mfg_en",&language,0);
  ImGui::SameLine();
  changed=ImGui::RadioButton("简体中文##mfg_zh",&language,1) || changed;
  if(changed) { g_ui_chinese=language!=0; reshade::set_config_value(nullptr,kConfigSection,"UiChinese",language); }
  ImGui::Checkbox(MfgText("Advanced settings###mfg_advanced","高级设置###mfg_advanced"),&g_ui_advanced);
  bool enabled=g_enabled.load();
  if(ImGui::Checkbox(MfgText("Enable MFG Unlock###mfg_enable","启用 MFG Unlock###mfg_enable"),&enabled)) {
    g_enabled.store(enabled); mfgunlock::framecount::NotifyLiveOptionsChanged();
    reshade::set_config_value(nullptr,kConfigSection,"Enabled",enabled?1:0);
    if(enabled && !mfgunlock::architecture::NativeMultiplierOnly()) InstallPresetForLoadedProviders();
  }
  const char* labels[]={MfgText("Game default","由游戏决定"),"2x","3x","4x","5x","6x"};
  const unsigned multiplier=mfgunlock::framecount::g_force_multiplier.load();
  int selection=multiplier>=2 && multiplier<=6 ? static_cast<int>(multiplier)-1:0;
  if(ImGui::Combo(MfgText("Frame generation multiplier###mfg_multiplier","帧生成倍率###mfg_multiplier"),&selection,labels,6)) {
    const unsigned value=selection ? static_cast<unsigned>(selection+1):0u;
    mfgunlock::framecount::g_force_multiplier.store(value);
    mfgunlock::framecount::NotifyFixedMultiplierChanged();
    reshade::set_config_value(nullptr,kConfigSection,"ForceMultiplier",value);
  }
  ImGui::TextWrapped("%s",MfgText("Enable frame generation in the game. Select Streamline in DLSS 5 Bridge; changing its version requires a restart.","请在游戏设置中开启帧生成。Streamline 版本在 DLSS 5 Bridge 中选择，切换版本后需重启。"));
  bool legacy=g_force_flip_meter_off.load();
  if(ImGui::Checkbox(MfgText("Legacy pacing (restart required)###mfg_legacy","Legacy 兼容节奏（需重启）###mfg_legacy"),&legacy)) {
    g_force_flip_meter_off.store(legacy);
    reshade::set_config_value(nullptr,kConfigSection,"ForceFlipMeteringOff",legacy?1:0);
  }
  ImGui::TextWrapped("%s",MfgText("RTX 40 compatibility may require Legacy. RTX 50 normally uses native pacing. Restart after changing this option.","40 系兼容性可能需要开启 Legacy；50 系通常使用原生节奏。此项每次更改后都需重启。"));
  using Status=mfgunlock::framecount::FixedOverrideStatus;
  const auto status=mfgunlock::framecount::g_fixed_override_status.load();
  if(enabled && multiplier && status!=Status::kApplied && status!=Status::kAppliedAfterRetry && status!=Status::kAlreadyMatched)
    ImGui::TextWrapped("%s",MfgText("The requested multiplier is pending or unavailable. Open advanced settings for details; game frame generation remains the fallback.","所选倍率尚未生效或不可用。详情见高级设置；插件会尝试保留游戏原生帧生成。"));
  if(g_ui_advanced) { ImGui::Separator(); DrawAdvancedOverlay(runtime); }
}

'''
# Forward declaration preserves the old renderer as the advanced view.
s=s[:pos]+'void DrawAdvancedOverlay(reshade::api::effect_runtime*);\n'+wrapper+s[pos:]
write(name,s)
(BACKUP/'sha256.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
print('Updated',len(records),'source files; originals:',BACKUP)
