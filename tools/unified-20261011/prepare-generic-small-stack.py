"""Extract unmodified runtime helpers for the native 64 KiB stack fixture."""
from pathlib import Path
import hashlib
import sys

root = Path(__file__).resolve().parents[2]
source = root / "src/unified-20261011/generic/src/addons/dlss5/dlssnr.hpp"
text = source.read_text(encoding="utf-8-sig")
functions = []
for signature in (
    "struct NrRuntimeLookup {",
    "inline NrRuntimeLookup FindNrRuntimePath(ID3D12Device* device, uint64_t adapter_luid) {",
    "inline std::string ComputeRuntimeSha256(const std::filesystem::path& path) {",
):
    start = text.index(signature)
    end = text.index("\n};\n" if signature.startswith("struct ") else "\n}\n", start) + 3
    functions.append(text[start:end])
out = Path(sys.argv[1])
out.write_text("\n".join(functions), encoding="utf-8")
print("Production helper source SHA256:", hashlib.sha256(source.read_bytes()).hexdigest())

