from pathlib import Path
import re,json,hashlib
ROOT=Path(__file__).resolve().parents[2]
BACK=ROOT/'evidence/unified-20261011/source-before-1f'
records=json.loads((BACK/'sha256.json').read_text(encoding='utf-8'))
def update(name,fn):
    p=ROOT/name; data=p.read_bytes(); dest=BACK/name
    if not dest.exists():
        dest.parent.mkdir(parents=True,exist_ok=True); dest.write_bytes(data)
        records[name]=hashlib.sha256(data).hexdigest()
    p.write_text(fn(data.decode('utf-8-sig').replace('\r\n','\n')),encoding='utf-8-sig' if data.startswith(b'\xef\xbb\xbf') else 'utf-8',newline='\r\n')
def mfg(s):
    marker='std::atomic_bool g_force_flip_meter_off{false};'
    assert s.count(marker)==1
    s=s.replace(marker,marker+'''
std::atomic_bool g_legacy_configured{false};
static bool LegacyRequested() {
  if(g_legacy_configured.load(std::memory_order_acquire)) return g_force_flip_meter_off.load();
  const auto* profile=mfgunlock::architecture::SelectedProfile();
  return profile && profile->architecture!=mfgunlock::Architecture::kBlackwell;
}
''')
    start=s.index('std::atomic_bool g_temporal_fix')
    s=s[:start]+re.sub(r'g_force_flip_meter_off\.load\([^)]*\)', 'LegacyRequested()',s[start:])
    s=re.sub(r'(g_force_flip_meter_off\.store\([^;]+;)',r'\1\n    g_legacy_configured.store(true,std::memory_order_release);',s)
    # Do not expose a second language-independent debug checkbox in the basic UI.
    s=s.replace('Blackwell native multiplier - unified1a','Blackwell native multiplier - unified1f')
    return s
update('src/unified-20261011/mfg/addon/addon.cpp',mfg)
for name in ['src/unified-20261011/bridge/src/dlss5-bridge.cpp','src/unified-20261011/bridge/src/version.rc',
             'src/unified-20261011/generic/src/addons/dlss5/dlssnr.hpp','src/unified-20261011/generic/generated/resource.rc',
             'src/unified-20261011/mfg/version.rc']:
    update(name,lambda s:re.sub(r'unified1[a-e]','unified1f',s))
update('tools/unified-20261011/DLSS5-Clean-Recovery.ps1',lambda s:s.replace('drs-journal\\.state$', 'drs-(journal|fg)\\.state$'))
(BACK/'sha256.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
print('Versions, default compatibility policy and external recovery updated')
