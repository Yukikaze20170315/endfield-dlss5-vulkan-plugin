"""Repair the captured first unified1 trial's slash-comparison false rollback.

Restricted to the exact captured state and a stopped Endfield process. User
settings, confirmed choices and safe-mode records are not reset.
"""
from pathlib import Path
import hashlib
import os
import shutil
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[2]
GAME=Path('C:/Program Files/Hypergryph Launcher/games/Endfield Game')
CAPTURE=ROOT/'evidence/unified-20261011/run-20261011-114221-061512/dlss5/state/bridge.state'
STATE=GAME/'dlss5/state/bridge.state'
BACKUP=ROOT/'evidence/unified-20261011/first-trial-path-repair'
result=subprocess.run(['C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe','-NoProfile','-Command',
    "@(Get-CimInstance Win32_Process -Filter \"Name='Endfield.exe'\").Count"],capture_output=True,text=True,check=True)
assert result.stdout.strip()=='0', 'Exit Endfield before repairing state'
old=STATE.read_bytes()
assert old==CAPTURE.read_bytes(), 'State changed since the captured failure; do not overwrite'
v=list(struct.unpack('<IIQQQ12I',old))
assert v[0]==0x35504c44 and v[1]==1 and v[3]==2
assert v[5:12]==[0,0,1,0,1,0,0], 'Not the initial false-rollback state'
def checksum(data):
    value=14695981039346656037
    for byte in data: value=((value^byte)*1099511628211)&0xffffffffffffffff
    return value
unsigned=bytearray(old); unsigned[8:16]=bytes(8)
assert checksum(unsigned)==v[2], 'State checksum mismatch'
BACKUP.mkdir(parents=True,exist_ok=False)
shutil.copy2(STATE,BACKUP/'bridge.state.before')
assert (BACKUP/'bridge.state.before').read_bytes()==old
v[2]=0; v[3]+=1; v[5]=1; v[7]=0
new=struct.pack('<IIQQQ12I',*v)
v[2]=checksum(new)
new=struct.pack('<IIQQQ12I',*v)
temp=STATE.with_name('bridge.state.path-repair.tmp')
with temp.open('xb') as stream:
    stream.write(new); stream.flush(); os.fsync(stream.fileno())
os.replace(temp,STATE)
assert STATE.read_bytes()==new
(BACKUP/'bridge.state.after').write_bytes(new)
print('Restored selected SL=1; retained all other choices, cleared stale active-run marker.')
print('Before SHA256:',hashlib.sha256(old).hexdigest())
print('After SHA256: ',hashlib.sha256(new).hexdigest())
