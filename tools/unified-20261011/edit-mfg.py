from pathlib import Path
root=Path(__file__).resolve().parents[2]/'src/unified-20261011/mfg'
p=root/'addon/addon.cpp'; s=p.read_text(encoding='utf-8')
s=s.replace('''  if (mfgunlock::architecture::NativeMultiplierOnly()) return false;
  unsigned char* base = nullptr;''','''  // An explicit Legacy selection changes only the SL presentation policy.
  // Blackwell's NGX architecture, PTX, kernels and quality paths stay native.
  if (mfgunlock::architecture::NativeMultiplierOnly() && !g_force_flip_meter_off.load()) return false;
  unsigned char* base = nullptr;''',1)
s=s.replace('''  if (!g_enabled.load() || !mfgunlock::architecture::ActiveProfile()) return;
  if (g_flip_meter_patched.load''','''  if (!g_enabled.load() || !g_force_flip_meter_off.load() || !mfgunlock::architecture::SelectedProfile()) return;
  if (g_flip_meter_patched.load''')
start=s.index('void OnRegisterOverlay(')
insert='''  bool legacy = g_force_flip_meter_off.load();
  if (ImGui::Checkbox("Legacy software flip pacing (restart required)", &legacy)) {
    g_force_flip_meter_off.store(legacy);
    reshade::set_config_value(nullptr, kConfigSection, "ForceFlipMeteringOff", legacy ? 1 : 0);
  }
  ImGui::TextWrapped("Legacy controls Streamline presentation pacing. RTX 40 compatibility may require it. RTX 50 defaults to native pacing. Restart after every change.");
  ImGui::Text("Legacy: %s", g_flip_meter_patched.load() ? "active in this process" : g_flip_meter_failed.load() ? "unavailable; see log" : "not applied in this process");
  ImGui::Separator();
'''
brace=s.index('{',start)+1
s=s[:brace]+'\n'+insert+s[brace:]
s=s.replace('Blackwell native multiplier - experimental bw3','Blackwell native multiplier - unified1')
s=s.replace('Native kernels and pacing preserved. Streamline selection belongs to the NR bridge.','Native NGX kernels preserved. Legacy pacing is a separate opt-in. Streamline selection belongs to the NR bridge.')
s=s.replace('''    if (const char* action = mfgunlock::framecount::LiveOptionsActionText()) ImGui::TextWrapped("%s", action);
    return;''','''    if (const char* action = mfgunlock::framecount::LiveOptionsActionText()) ImGui::TextWrapped("%s", action);
    bool advanced=g_show_debug_options.load();
    if(ImGui::Checkbox("Show advanced/debug options",&advanced)) {
      g_show_debug_options=advanced;
      mfgunlock::diagnostic::g_verbose=advanced;
      reshade::set_config_value(nullptr,kConfigSection,"ShowDebugOptions",advanced?1:0);
    }
    if(advanced) {
      DrawMfgRuntimeDiagnostics();
      ImGui::TextWrapped("Compatibility provider/kernel, HDR/UI composition and Reflex overrides apply to Turing/Ampere/Ada only. Blackwell uses its native implementation; saved compatibility options are preserved.");
    }
    return;''')
s=s.replace('Experimental bw3: native Blackwell multiplier; Endfield startup compatibility','Unified1: native Blackwell multiplier, optional Legacy pacing, Endfield startup compatibility')
p.write_text(s,encoding='utf-8')
p=root/'build.cmd';s=p.read_text(encoding='utf-8');s=s.replace('..\\..\\builds\\mfg-blackwell-codex-20261011-bw3','..\\..\\..\\builds\\unified-20261011\\mfg');p.write_text(s,encoding='utf-8')
print('MFG Legacy and diagnostics applied')
