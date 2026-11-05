// Fixture for the payload NR-runtime table: which file each GPU architecture
// is served, and what happens for architectures the table does not name.
//
// The table decides which of the two shipped runtimes a machine loads. It is
// asserted here rather than only in game because getting it wrong is silent:
// the wrong runtime loads and the game reports an unsupported-architecture
// failure far from this code, or a machine quietly gets a file the package
// never verified.
#include "common/payload.hpp"

#include <cstdio>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
  if (!condition) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}

bool ServesArch(uint32_t architecture, const wchar_t* expected_path) {
  const dlss5::payload::NrRuntime* runtime = dlss5::payload::SelectionForArchitecture(architecture);
  if (!runtime) return expected_path == nullptr;
  if (expected_path == nullptr) return false;
  return std::wstring(runtime->path) == expected_path;
}

}  // namespace

int main() {
  using namespace dlss5::payload;

  // Blackwell alone gets the signed sm_120-only build.
  Check(ServesArch(0x1b0, nr_blackwell.path), "0x1b0 selects the RTX 50 runtime");
  Check(!ServesArch(0x1b0, nr_generic.path), "0x1b0 does not select the multi-generation runtime");

  // Turing, Ampere and Ada all get the multi-generation build: it is the only
  // file in the package that carries their cubins, so a per-generation split
  // would ship the same bytes twice.
  Check(ServesArch(0x160, nr_generic.path), "0x160 (Turing) selects the multi-generation runtime");
  Check(ServesArch(0x170, nr_generic.path), "0x170 (Ampere) selects the multi-generation runtime");
  Check(ServesArch(0x190, nr_generic.path), "0x190 (Ada) selects the multi-generation runtime");
  Check(!ServesArch(0x160, nr_blackwell.path), "Turing does not get the Blackwell-only runtime");
  Check(!ServesArch(0x190, nr_blackwell.path), "Ada does not get the Blackwell-only runtime");

  // Unknown or absent architecture resolves to nothing. Returning a runtime
  // here would load a file whose cubins the GPU may not run, and the failure
  // would surface as an NGX error rather than as this decision.
  Check(ServesArch(0, nullptr), "architecture 0 selects nothing");
  Check(ServesArch(0x1a0, nullptr), "an unnamed architecture selects nothing");
  Check(ServesArch(0x1c0, nullptr), "a future architecture selects nothing");

  // Every shipped runtime declares at least one architecture, so no entry is
  // dead weight in the package.
  for (const NrRuntime* runtime : nr_runtimes) {
    Check(runtime->architectures[0] != 0, "each runtime names at least one architecture");
    Check(runtime->path != nullptr && *runtime->path != L'\0', "each runtime has a path");
    Check(std::wstring(runtime->path).find(L"nvngx_dlssnr.dll") != std::wstring::npos,
          "each runtime keeps the module name the witness and Generic look for");
    Check(runtime->hash != nullptr && std::string(runtime->hash).size() == 64,
          "each runtime carries a full sha256");
  }

  // The two runtimes must not overlap: an architecture served by both would
  // make the choice depend on table order.
  for (uint32_t architecture : {0x160u, 0x170u, 0x190u, 0x1b0u}) {
    int served = 0;
    for (const NrRuntime* runtime : nr_runtimes) {
      if (NrRuntimeServes(*runtime, architecture)) ++served;
    }
    Check(served == 1, "each known architecture is served by exactly one runtime");
  }

  // Paths must differ, so the two files can coexist in one installation.
  Check(std::wstring(nr_blackwell.path) != std::wstring(nr_generic.path),
        "the two runtimes live at different paths");

  // The unavailable path must not collide with a shipped runtime, or a failed
  // selection would resolve to a file the package did verify.
  for (const NrRuntime* runtime : nr_runtimes) {
    Check(std::wstring(runtime->path) != std::wstring(nr_unavailable),
          "the unavailable path is distinct from every shipped runtime");
  }

  if (failures == 0) std::printf("test-nr-selection: all checks passed\n");
  return failures == 0 ? 0 : 1;
}
