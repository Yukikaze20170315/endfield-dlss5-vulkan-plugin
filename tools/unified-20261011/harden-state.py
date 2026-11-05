from pathlib import Path
root=Path(__file__).resolve().parents[2]/'src/unified-20261011'
p=root/'bridge/src/deployment.inc';s=p.read_text(encoding='utf-8')
s=s.replace('#include "../../common/payload.hpp"','#include "../../common/payload.hpp"\n#include "../../common/deployment-state.hpp"')
a=s.index('struct State {');b=s.index('static State state;',a)
s=s[:a]+'using State=dlss5::deployment_policy::State;\n'+s[b:]
s=s.replace('static uint32_t architecture=0,current=0;','static uint32_t architecture=0,current=0,current_early=0,current_nr=0;')
s=s.replace('''            s.append(reinterpret_cast<const char*>(&data),sizeof(data));''','''        {
            s.append(reinterpret_cast<const char*>(&data.ftLastWriteTime),sizeof(data.ftLastWriteTime));
            s+=std::to_string(data.nFileSizeHigh)+":"+std::to_string(data.nFileSizeLow);
        }''')
s=s.replace('''    s+=std::to_string(architecture);''','''    s+=std::to_string(architecture);
    wchar_t build[64]{}; DWORD bytes=sizeof(build),ubr=0,ubr_bytes=sizeof(ubr);
    RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\\\Microsoft\\\\Windows NT\\\\CurrentVersion",L"CurrentBuildNumber",RRF_RT_REG_SZ,nullptr,build,&bytes);
    RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\\\Microsoft\\\\Windows NT\\\\CurrentVersion",L"UBR",RRF_RT_REG_DWORD,nullptr,&ubr,&ubr_bytes);
    s.append(reinterpret_cast<const char*>(build),wcslen(build)*sizeof(wchar_t)); s+=std::to_string(ubr);''')
s=s.replace('result==durable::ReadResult::Invalid || state.selected>1 || state.confirmed>1 || state.nr>2 || state.early>2','result==durable::ReadResult::Invalid || !deployment_policy::Valid(state)')
s=s.replace('''        state={}; state.safe=1;
        status="状态损坏''','''        if(writable && fs::exists(dir/L"bridge.state")) {
            std::error_code ec;
            fs::copy_file(dir/L"bridge.state",dir/(L"bridge.state.bad-"+std::to_wstring(GetTickCount64())),fs::copy_options::none,ec);
            if(ec) writable=false;
        }
        state={}; state.safe=1;
        status="状态损坏''')
a=s.index('    if(state.active && !state.healthy) {');b=s.index('    if(durable::Plain(game/L"dlss5/safe-mode.flag")',a)
s=s[:a]+'''    WIN32_FILE_ATTRIBUTE_DATA mark{};
    const bool crashed=writable && GetFileAttributesExW((dir/L"startup-crash.marker").c_str(),GetFileExInfoStandard,&mark) && mark.nFileSizeLow!=0;
    if(deployment_policy::Begin(state,fingerprint,crashed))
        status="启动未通过健康点，已回退 / Startup did not reach health confirmation; restored.";
'''+s[b:]
s=s.replace('''    state.trial=state.selected!=state.confirmed || (state.fingerprint && fingerprint!=state.fingerprint);
    state.fingerprint=fingerprint; state.active=1; state.healthy=0;
''','')
s=s.replace('''    current=safe.load() ? 0u:state.selected;''','''    current=safe.load() ? 0u:state.selected;
    current_early=state.early; current_nr=state.nr;
    if(safe.load()) { g_cfg.stage=0; g_vk_mirror=0; }''')
s=s.replace('''    State next=state; next.healthy=1; next.trial=0; next.failures=0; next.confirmed=current;''','''    State next=state; deployment_policy::Healthy(next,current);
    next.confirmed_early=current_early; next.confirmed_nr=current_nr;''')
p.write_text(s,encoding='utf-8')
p=root/'generic/src/addons/dlss5/layer_trial.hpp';s=p.read_text(encoding='utf-8')
s=s.replace('''  auto dir = ::dlss5::durable::StateDirectory''','''  deadline=0; safe=false; changed=false; record={};
  auto dir = ::dlss5::durable::StateDirectory''')
s=s.replace('''    record = {}; safe = true;''','''    if(writable && std::filesystem::exists(file)) {
      std::error_code ec;
      std::filesystem::copy_file(file,std::filesystem::path(file.wstring()+L".bad-"+std::to_wstring(GetTickCount64())),std::filesystem::copy_options::none,ec);
      if(ec) writable=false;
    }
    record = {}; safe = true;''')
p.write_text(s,encoding='utf-8')
print('State handling hardened')
