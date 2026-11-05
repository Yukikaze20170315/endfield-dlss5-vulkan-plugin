"""Build a separate, reproducible 1i-based MFG diagnostic package."""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261011i-candidate'
OUT = ROOT / 'dist/unified-20261011j-diagnostic'
MFG = ROOT / 'builds/unified-20261011/mfg-diagnostics1j/renodx-mfgunlock.addon64'
NOTE = """# unified1j：MFG 挂钩诊断 / MFG hook diagnostics

本包在1i上仅补充MFG地址挂钩失败日志，用于区分线程访问、模块身份、目标地址与Detours事务失败。它不是已经修复倍率或卡顿的发布版。NR参数、各代兼容策略、Throttle默认1和已有用户配置均不改变；本轮诊断继续保留已设置的Throttle=2。不要恢复来源不明的旧诊断INI。

This package adds MFG address-hook failure logging to 1i to distinguish thread access, module identity, target address and Detours transaction failures. It is not a release with a verified multiplier or pacing fix. NR parameters, GPU compatibility policies, the Throttle default of 1 and existing user settings remain unchanged; keep the already configured Throttle=2 for this diagnostic run. Do not restore an old diagnostic INI with unknown contents.

已有完整1i安装时，正常退出游戏后更新本包即可；无需撤除NR、重新安装ReShade或恢复个人参数。启动后进入可控制角色的场景，切换MFG 2×和6×并记录是否实际生效。若再次失效，退出或重启前保留ReShade.log和dlss5-bridge.log，前者的“address failure”行含具体阶段和错误码。未完成实机诊断前，不把面板保存倍率当作实际倍率。

For an existing complete 1i installation, exit the game normally and update from this package; removing NR, reinstalling ReShade or resetting personal settings is unnecessary. Enter a playable scene, switch MFG between 2× and 6×, and record whether the changes take effect. If control fails again, preserve ReShade.log and dlss5-bridge.log before exiting or restarting; the former's “address failure” line records the failure stage and error code. A saved UI multiplier does not establish the effective multiplier.

回退：退出游戏后将1i的renodx-mfgunlock.addon64恢复到游戏目录；如果使用项目部署脚本安装，则使用本次备份执行Rollback。游戏完全无法启动时，按下方随包恢复说明操作。

Rollback: after exiting the game, restore renodx-mfgunlock.addon64 from 1i to the game directory. If installed with the project's deployment script, use Rollback with that installation's backup. If the game cannot start at all, follow the bundled recovery instructions below.

---

"""


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


assert not OUT.exists(), 'Preserve existing diagnostic packages'
manifest = json.loads((BASE / 'release-manifest.json').read_text(encoding='utf-8'))
for entry in manifest['files']:
    source = BASE / entry['path']
    assert source.resolve().is_relative_to(BASE.resolve())
    assert digest(source) == entry['sha256'], entry['path']
assert MFG.is_file()
entries = []
for entry in manifest['files']:
    name = entry['path']
    target = OUT / name
    assert target.resolve().is_relative_to(OUT.resolve())
    target.parent.mkdir(parents=True, exist_ok=True)
    if name == 'DLSS5-使用与恢复 Usage and Recovery.md':
        target.write_text(NOTE + (BASE / name).read_text(encoding='utf-8'), encoding='utf-8')
    else:
        source = MFG if name == 'renodx-mfgunlock.addon64' else BASE / name
        shutil.copy2(source, target)
        assert digest(target) == digest(source)
    entries.append({'path': name, 'sha256': digest(target), 'size': target.stat().st_size})
(OUT / 'release-manifest.json').write_text(json.dumps({
    'version': 'unified1j-diagnostic-20261011',
    'status': 'diagnostic only; hook failure cause and pacing fix not established; hardware acceptance pending',
    'files': entries,
}, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
(OUT / 'SHA256SUMS.txt').write_text(
    ''.join(f"{e['sha256']}  {e['path']}\n" for e in entries)
    + f"{digest(OUT / 'release-manifest.json')}  release-manifest.json\n", encoding='utf-8')
assert (OUT / 'DLSS5-Clean-Recovery.ps1').read_bytes().startswith(b'\xef\xbb\xbf')
print(json.dumps({'version': 'unified1j-diagnostic', 'payload_files': len(entries),
                  'total_files': sum(p.is_file() for p in OUT.rglob('*')),
                  'mfg_sha256': digest(OUT / 'renodx-mfgunlock.addon64')}, ensure_ascii=False))
