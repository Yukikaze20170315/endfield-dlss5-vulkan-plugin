"""Snapshot Endfield logs and decode deployment state without changing the game."""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import re
import shutil
import struct

ROOT=Path(__file__).resolve().parents[2]
GAME=Path('C:/Program Files/Hypergryph Launcher/games/Endfield Game')
OUT=ROOT/'evidence/unified-20261011'/('run-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
OUT.mkdir(parents=True)
files=['dlss5-bridge.log','dlss5-observe.log','dlss5-sl.log','ReShade.log','RenoDX-DLSS5-crash.log',
       'ReShade.ini','dlss5-bridge.cfg','vk-present-adapter.ini','dlss5/state/bridge.state',
       'dlss5/state/generic-layers.state','dlss5/state/startup-crash.marker',
       'dlss5/state/drs-result.txt','dlss5/state/drs-journal.state','dlss5/state/drs-fg.state']
inventory=[]
for name in files:
    src=GAME/name
    if not src.is_file(): continue
    dst=OUT/name
    dst.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(src,dst)
    inventory.append({'path':name,'size':dst.stat().st_size,'sha256':hashlib.sha256(dst.read_bytes()).hexdigest()})
def read(name):
    p=OUT/name
    return p.read_text(encoding='utf-8-sig',errors='replace') if p.is_file() else ''
bridge=read('dlss5-bridge.log')
reshade=read('ReShade.log')
observer=read('dlss5-observe.log')
groups={}
for frame,index,total in re.findall(r'\[fg-transition\].*?frame=([0-9A-Fa-f]+) idx=(\d+)/(\d+)',bridge):
    if total=='5': groups.setdefault(frame,set()).add(int(index))
state_path=OUT/'dlss5/state/bridge.state'
state={}
if state_path.is_file() and state_path.stat().st_size==80:
    names=['magic','version','checksum','generation','fingerprint','selected','confirmed','active','healthy',
           'trial','failures','safe','nr','early','confirmed_nr','confirmed_early','reserved']
    state=dict(zip(names,struct.unpack('<IIQQQ12I',state_path.read_bytes())))
summary={
    'complete_6x_groups_in_logged_excerpt':sum(indices==set(range(1,6)) for indices in groups.values()),
    'redirection_succeeded':'[sl-dir] slInit returned 0' in observer and '[sl-dir] REDIRECTED:' in observer,
    'nr_engaged':'state=ENGAGED' in reshade,
    'legacy_patch_applied':'forced flip-metering off in ' in reshade,
    'multiplier_accept_messages':[line for line in reshade.splitlines() if 'Blackwell native multiplier' in line][-15:],
    'saved_config':re.findall(r'^(?:ForceMultiplier|ForceFlipMeteringOff|NRRenderPasses|NRUpscaledPasses|NRPresentPasses)=.*$',read('ReShade.ini'),re.M),
    'deployment_state':state,
    'driver_maintenance_result':read('dlss5/state/drs-result.txt').strip(),
    'device_runtime_selections':[line for line in bridge.splitlines() if '[deployment] NR selected for device LUID=' in line][-8:],
    'startup_driver_sync':[line for line in bridge.splitlines() if '[deployment] startup DRS:' in line][-4:],
    'scope':'Log evidence only; does not establish visual quality or display pacing.',
    'files':inventory,
}
(OUT/'analysis.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(OUT)
print(json.dumps({k:v for k,v in summary.items() if k!='files'},ensure_ascii=False,indent=2))
