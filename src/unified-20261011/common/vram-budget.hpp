#pragma once
// Windows video-memory budget policy, shared by the Bridge panel and the
// recovery menu (rundll32 "Maintenance apply-vram-budget/restore-vram-budget").
//
// VidMm reads two REG_DWORD values (MiB) when the adapter starts:
//   per-process budget = VRAM - min(10% VRAM, MaxVideoMemoryFragmentationBuffer)
//                             - min(5% VRAM, MaxProcessBudgetCapBuffer)
// The defaults 512/256 hold back 768 MiB on cards with 5 GB or more; 64/64
// holds back 128 MiB. A change applies only after a Windows restart: a Fast
// Startup shutdown resumes the hibernated kernel and does not re-read them.
//
// Rules follow tools/vram-budget-20261029/vram-budget-policy.ps1: never
// replace another tool's policy, save the original before writing, verify by
// readback, roll back on failure, and refuse to overwrite values that someone
// else changed after Apply. The PowerShell tool's allow-list of verified
// dxgmms2.sys hashes is replaced by checking that the installed dxgmms2.sys
// contains both value names, i.e. that this Windows build reads them.
#include "durable.hpp"
#include <aclapi.h>
#include <algorithm>
#pragma comment(lib, "advapi32.lib")

namespace dlss5::vram_budget {
namespace fs = std::filesystem;

inline constexpr wchar_t kPolicyKey[] = L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers\\MemoryManager";
inline constexpr const wchar_t* kNames[2] = {L"MaxVideoMemoryFragmentationBuffer", L"MaxProcessBudgetCapBuffer"};
inline constexpr const wchar_t* kPercentNames[2] = {L"VideoMemoryFragmentationBuffer", L"ProcessBudgetCapBuffer"};
inline constexpr DWORD kDefaults[2] = {512, 256};
inline constexpr DWORD kPercentDefaults[2] = {10, 5};
inline constexpr DWORD kCandidate = 64;
inline constexpr wchar_t kBackupLeaf[] = L"vram-budget-backup.state";
inline constexpr wchar_t kChangeLeaf[] = L"vram-budget-change.state";
inline constexpr wchar_t kResultLeaf[] = L"vram-budget-result.txt";
// Same name as vram-budget-policy.ps1, so the two tools never write at once.
inline constexpr wchar_t kMutexName[] = L"Global\\DLSS5-VidMmBudgetPolicy-Candidate";

// Tests point this at a private HKCU key and skip the dxgmms2.sys check.
struct Target {
  HKEY root = HKEY_LOCAL_MACHINE;
  std::wstring path = kPolicyKey;
  bool check_reader = true;
  // Third backup copy, outside every game-path-scoped folder, so moving the
  // game or the clean-up recovery archiving dlss5\ cannot lose the original
  // values. Empty: %ProgramData%\dlss5-bridge\vram-budget for the HKLM key,
  // none for any other key.
  fs::path machine_backup;
};
// Non-DWORD contents are not kept: Apply refuses to back up or replace them.
struct Value { bool exists = false; bool dword = false; DWORD data = 0; };
struct Policy {
  bool readable = false;
  bool key_exists = false;
  bool percent_custom = false;
  Value values[2]{};
  uint64_t written = 0; // key last-write time, FILETIME UTC; 0 when absent
};
enum class Kind { Unknown, Default, Candidate, Custom };

inline uint64_t FileTimeValue(const FILETIME& time) {
  return (static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}
inline uint64_t Now() { FILETIME now{}; GetSystemTimeAsFileTime(&now); return FileTimeValue(now); }
// GetTickCount64 includes sleep and hibernation, and Fast Startup resumes a
// hibernated kernel, so this is the last real restart: when VidMm read the policy.
inline uint64_t BootTime() { return Now() - GetTickCount64() * 10000ull; }

inline Value QueryValue(HKEY key, const wchar_t* name, bool& okay) {
  Value value;
  DWORD type = 0, data = 0, size = sizeof(data);
  const LSTATUS result = RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(&data), &size);
  if (result == ERROR_FILE_NOT_FOUND) return value;
  if (result != ERROR_SUCCESS && result != ERROR_MORE_DATA) { okay = false; return value; }
  value.exists = true; // ERROR_MORE_DATA: present but larger than a DWORD
  value.dword = result == ERROR_SUCCESS && type == REG_DWORD && size == sizeof(DWORD);
  value.data = value.dword ? data : 0;
  return value;
}
// Read-only and cheap (no elevation); the panel calls it at most once a second.
inline Policy Query(const Target& target = {}) {
  Policy policy;
  HKEY key = nullptr;
  const LSTATUS opened = RegOpenKeyExW(target.root, target.path.c_str(), 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key);
  if (opened == ERROR_FILE_NOT_FOUND) { policy.readable = true; return policy; }
  if (opened != ERROR_SUCCESS) return policy;
  policy.key_exists = true;
  bool okay = true;
  FILETIME written{};
  if (RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, &written) == ERROR_SUCCESS)
    policy.written = FileTimeValue(written);
  for (int i = 0; i < 2; ++i) {
    policy.values[i] = QueryValue(key, kNames[i], okay);
    const Value percent = QueryValue(key, kPercentNames[i], okay);
    if (percent.exists && (!percent.dword || percent.data != kPercentDefaults[i])) policy.percent_custom = true;
  }
  RegCloseKey(key);
  policy.readable = okay;
  return policy;
}
inline Kind Classify(const Policy& policy) {
  if (!policy.readable) return Kind::Unknown;
  bool candidate = true, defaults = true;
  for (int i = 0; i < 2; ++i) {
    const Value& value = policy.values[i];
    candidate = candidate && value.exists && value.dword && value.data == kCandidate;
    defaults = defaults && (!value.exists || (value.dword && value.data == kDefaults[i]));
  }
  return candidate ? Kind::Candidate : defaults ? Kind::Default : Kind::Custom;
}
inline bool Same(const Value& left, const Value& right) {
  return left.exists == right.exists && (!left.exists || (left.dword == right.dword && left.data == right.data));
}
inline bool SameValues(const Policy& left, const Policy& right) {
  return Same(left.values[0], right.values[0]) && Same(left.values[1], right.values[1]);
}
inline Policy Candidate() {
  Policy policy;
  policy.readable = true; policy.key_exists = true;
  for (auto& value : policy.values) { value.exists = true; value.dword = true; value.data = kCandidate; }
  return policy;
}
// The key write time covers Apply and restores that keep the key; the change
// record covers restores that delete the key (the write time is then gone).
inline bool RestartPending(const Policy& policy, uint64_t last_change) {
  const uint64_t boot = BootTime();
  return policy.written > boot || last_change > boot;
}

// Fixed-layout durable records: no padding, so the checksum covers every byte.
struct Backup {
  uint32_t magic = 0x42565644, version = 1;
  uint64_t machine = 0;
  uint32_t key_existed = 0, reserved = 0;
  uint32_t present[2]{};
  uint32_t data[2]{};
  uint64_t created = 0;
  uint64_t checksum = 0;
};
static_assert(sizeof(Backup) == 56, "Backup must not contain padding");
struct Change {
  uint32_t magic = 0x43565644, version = 1;
  uint64_t machine = 0;
  uint64_t changed = 0;
  uint32_t applied = 0, reserved = 0;
  uint64_t checksum = 0;
};
static_assert(sizeof(Change) == 40, "Change must not contain padding");

// A backup copied from another PC must never be applied here.
inline uint64_t Machine() {
  wchar_t text[128]{};
  DWORD size = sizeof(text);
  if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid", RRF_RT_REG_SZ,
                   nullptr, text, &size) != ERROR_SUCCESS) return 0;
  const size_t length = wcsnlen(text, 128);
  return length ? durable::Checksum(text, length * sizeof(wchar_t)) : 0;
}
inline bool Valid(const Backup& backup) {
  if (backup.key_existed > 1 || backup.reserved) return false;
  for (int i = 0; i < 2; ++i) {
    if (backup.present[i] > 1 || (!backup.present[i] && backup.data[i])) return false;
    if (backup.present[i] && (!backup.key_existed || backup.data[i] < kCandidate)) return false;
  }
  return true;
}
inline Policy Original(const Backup& backup) {
  Policy policy;
  policy.readable = true; policy.key_exists = backup.key_existed != 0;
  for (int i = 0; i < 2; ++i) {
    policy.values[i].exists = policy.values[i].dword = backup.present[i] != 0;
    policy.values[i].data = backup.present[i] ? backup.data[i] : 0u;
  }
  return policy;
}
// Every value is 64 or the backup's original and at least one is no longer the
// original: an Apply that stopped between, or failed after, its two writes.
inline bool PartlyApplied(const Policy& current, const Backup& backup) {
  if (!current.readable || Classify(current) == Kind::Candidate) return false;
  const Policy original = Original(backup), candidate = Candidate();
  bool changed = false;
  for (int i = 0; i < 2; ++i) {
    if (!Same(current.values[i], candidate.values[i]) && !Same(current.values[i], original.values[i])) return false;
    changed = changed || !Same(current.values[i], original.values[i]);
  }
  return changed;
}
inline fs::path MachineBackupDirectory(const Target& target) {
  if (!target.machine_backup.empty()) return target.machine_backup;
  if (target.root != HKEY_LOCAL_MACHINE) return {};
  WidePathBuffer data;
  const DWORD n = data ? GetEnvironmentVariableW(L"ProgramData", data.data(), data.capacity) : 0;
  if (!n || n >= data.capacity) return {};
  return fs::path(data.data()) / L"dlss5-bridge" / L"vram-budget";
}
// Any user may create folders in ProgramData, so the default machine copy is
// trusted only when an administrator or SYSTEM owns it; standard users cannot
// give a file they created to either owner.
inline bool TrustedMachineCopy(const fs::path& file) {
  PSID owner = nullptr;
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  if (GetNamedSecurityInfoW(file.c_str(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner, nullptr, nullptr,
                            nullptr, &descriptor) != ERROR_SUCCESS) return false;
  const bool trusted = owner && (IsWellKnownSid(owner, WinBuiltinAdministratorsSid) ||
                                 IsWellKnownSid(owner, WinLocalSystemSid));
  LocalFree(descriptor);
  return trusted;
}
// Elevated writers may default to their own account as owner; hand the file
// to the Administrators group so the check above accepts it.
inline void GiveToAdministrators(const fs::path& file) {
  BYTE sid[SECURITY_MAX_SID_SIZE]{};
  DWORD size = sizeof(sid);
  if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, sid, &size)) return;
  std::wstring path = file.wstring();
  SetNamedSecurityInfoW(path.data(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, sid, nullptr, nullptr, nullptr);
}
// Without the old record being moved aside, a second Apply could replace the
// original values with a copy of 64/64; ArchiveRecord never overwrites.
inline bool SaveBackup(const fs::path& directory, const Backup& backup) {
  if (directory.empty()) return false;
  const auto file = directory / kBackupLeaf;
  if (!durable::ArchiveRecord(file) || !durable::Save(file, backup)) return false;
  Backup check{};
  if (durable::Read(file, check, backup.magic) != durable::ReadResult::Valid) return false;
  Backup expected = backup;
  check.checksum = 0; expected.checksum = 0;
  return std::memcmp(&check, &expected, sizeof(Backup)) == 0;
}
inline void NoteChange(const fs::path& state, uint64_t machine, bool applied) {
  if (state.empty()) return;
  Change change;
  change.machine = machine; change.changed = Now(); change.applied = applied ? 1u : 0u;
  durable::Save(state / kChangeLeaf, change);
}
inline uint64_t LastChange(const fs::path& state) {
  Change change{};
  if (state.empty() || durable::Read(state / kChangeLeaf, change, change.magic) != durable::ReadResult::Valid) return 0;
  const uint64_t machine = Machine();
  return machine && change.machine == machine ? change.changed : 0;
}

// Builds that honour the two values carry their names as UTF-16 literals.
// Reads about 2 MB; call it only from the maintenance process.
inline bool ReaderPresent() {
  WidePathBuffer system;
  const UINT length = system ? GetSystemDirectoryW(system.data(), system.capacity) : 0;
  if (!length || length >= system.capacity) return false;
  const auto path = fs::path(system.data()) / L"drivers" / L"dxgmms2.sys";
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size{};
  std::vector<char> image;
  bool okay = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= 64ll * 1024 * 1024;
  if (okay) {
    image.resize(static_cast<size_t>(size.QuadPart));
    DWORD got = 0;
    okay = ReadFile(file, image.data(), static_cast<DWORD>(image.size()), &got, nullptr) && got == image.size();
  }
  CloseHandle(file);
  if (!okay) return false;
  for (const wchar_t* name : kNames) {
    const char* begin = reinterpret_cast<const char*>(name);
    const char* end = begin + wcslen(name) * sizeof(wchar_t);
    if (std::search(image.begin(), image.end(), begin, end) == image.end()) return false;
  }
  return true;
}

// Writes exactly the two values. The key is deleted only when the wanted
// state says it did not exist and nothing else (values or subkeys) is left.
inline bool WritePolicy(const Target& target, const Policy& wanted) {
  const bool any = wanted.values[0].exists || wanted.values[1].exists;
  const REGSAM access = KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY;
  HKEY key = nullptr;
  LSTATUS opened = ERROR_SUCCESS;
  if (wanted.key_exists || any) {
    opened = RegCreateKeyExW(target.root, target.path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, access,
                             nullptr, &key, nullptr);
  } else {
    opened = RegOpenKeyExW(target.root, target.path.c_str(), 0, access, &key);
    if (opened == ERROR_FILE_NOT_FOUND) return true;
  }
  if (opened != ERROR_SUCCESS) return false;
  bool okay = true;
  for (int i = 0; i < 2; ++i) {
    LSTATUS result = ERROR_SUCCESS;
    if (wanted.values[i].exists) {
      const DWORD data = wanted.values[i].data;
      result = RegSetValueExW(key, kNames[i], 0, REG_DWORD, reinterpret_cast<const BYTE*>(&data), sizeof(data));
    } else {
      result = RegDeleteValueW(key, kNames[i]);
      if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    }
    okay = okay && result == ERROR_SUCCESS;
  }
  RegFlushKey(key);
  DWORD subkeys = 1, values = 1;
  const bool empty = RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr, &values,
                                      nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS && !subkeys && !values;
  RegCloseKey(key);
  if (okay && !wanted.key_exists && empty) {
    const LSTATUS removed = RegDeleteKeyExW(target.root, target.path.c_str(), KEY_WOW64_64KEY, 0);
    okay = removed == ERROR_SUCCESS || removed == ERROR_FILE_NOT_FOUND;
  }
  return okay;
}

class PolicyLock {
 public:
  PolicyLock() noexcept : handle_(CreateMutexW(nullptr, FALSE, kMutexName)) {
    if (!handle_) return;
    const DWORD wait = WaitForSingleObject(handle_, 0);
    owned_ = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
  }
  ~PolicyLock() {
    if (owned_) ReleaseMutex(handle_);
    if (handle_) CloseHandle(handle_);
  }
  PolicyLock(const PolicyLock&) = delete;
  PolicyLock& operator=(const PolicyLock&) = delete;
  explicit operator bool() const noexcept { return owned_; }
 private:
  HANDLE handle_;
  bool owned_ = false;
};

inline bool FindBackup(const fs::path& state, const fs::path& mirror, const Target& target, uint64_t machine,
                       Backup& out);

// Results are UTF-8, one "中文 / English" pair per line (the panel shows one
// side); the Chinese side must not contain " / ".
inline std::string Apply(const fs::path& state, const fs::path& mirror, const Target& target = {}) {
  PolicyLock lock;
  if (!lock) return "另一个显存预算操作正在进行，未修改 / Another VRAM budget operation is running; nothing changed.";
  if (state.empty()) return "无法写入状态目录，未修改 / Cannot write the state folder; nothing changed.";
  const uint64_t machine = Machine();
  if (!machine) return "无法读取本机标识，未修改 / Cannot read this PC's identity; nothing changed.";
  if (target.check_reader && !ReaderPresent())
    return "此 Windows 版本的显存管理组件不读取这两个设置，未修改 / This Windows build's video memory manager does not read these settings; nothing changed.";
  const Policy original = Query(target);
  if (!original.readable) return "无法读取注册表，未修改 / Cannot read the registry; nothing changed.";
  if (original.percent_custom)
    return "已有自定义的百分比设置，为避免叠加，未修改 / Custom percentage settings exist; nothing changed to avoid combining policies.";
  if (Classify(original) == Kind::Candidate)
    return "已经是 64/64，无需应用；原有备份保留 / Already 64/64; nothing to apply and the existing backup is kept.";
  for (const Value& value : original.values) {
    if (value.exists && (!value.dword || value.data < kCandidate))
      return "现有设置不是 64 或更大的 DWORD，可能属于其他工具，未修改 / An existing value is not a DWORD of 64 or more and may belong to another tool; nothing changed.";
  }
  // After a part-way Apply the backup still holds the real original; a new
  // backup of the mix would replace it, so that backup is reused instead.
  Backup earlier{};
  const bool resume = FindBackup(state, mirror, target, machine, earlier) && PartlyApplied(original, earlier);
  const Policy before = resume ? Original(earlier) : original;
  if (!resume) {
    Backup backup;
    backup.machine = machine;
    backup.key_existed = original.key_exists ? 1u : 0u;
    for (int i = 0; i < 2; ++i) {
      backup.present[i] = original.values[i].exists ? 1u : 0u;
      backup.data[i] = original.values[i].exists ? original.values[i].data : 0u;
    }
    backup.created = Now();
    if (!SaveBackup(state, backup))
      return "无法保存原值备份，未修改 / Cannot save the backup of the original values; nothing changed.";
    if (!mirror.empty()) SaveBackup(mirror, backup); // second copy, best effort
    // Third copy, best effort. ProgramData lets standard users create folders,
    // so an elevated write goes only through a path without reparse points.
    if (const fs::path machine_dir = MachineBackupDirectory(target); !machine_dir.empty()) {
      std::error_code made;
      if (durable::Plain(machine_dir)) fs::create_directories(machine_dir, made);
      if (!made && durable::Plain(machine_dir) && SaveBackup(machine_dir, backup) && target.machine_backup.empty())
        GiveToAdministrators(machine_dir / kBackupLeaf);
    }
  }
  const Policy again = Query(target);
  if (!again.readable || again.key_exists != original.key_exists || !SameValues(again, original) || again.percent_custom)
    return "保存备份期间设置被改动，未修改 / The settings changed while the backup was being saved; nothing changed.";
  const Policy wanted = Candidate();
  if (!WritePolicy(target, wanted) || !SameValues(Query(target), wanted)) {
    const bool rolled = WritePolicy(target, before) && SameValues(Query(target), before);
    return rolled ? "写入后校验失败，已还原原值 / Verification after writing failed; the original values were restored." :
                    "写入后校验失败，还原也失败；备份已保留，请用恢复重试 / Verification after writing failed and so did the rollback; the backup is kept, retry Restore.";
  }
  NoteChange(state, machine, true);
  return "已设为 64/64，重启电脑后生效（开启快速启动时，关机再开机不算重启） / Set to 64/64; restart Windows to apply (with Fast Startup on, shutting down and powering on is not a restart).";
}

// The newest valid backup made on this PC, from the state folder, the
// LocalAppData mirror or the machine-wide copy. A game folder that was moved
// or cleaned has none of its own, and an older game folder can still hold the
// backup of an earlier Apply.
inline bool FindBackup(const fs::path& state, const fs::path& mirror, const Target& target, uint64_t machine,
                       Backup& out) {
  bool found = false;
  const fs::path machine_dir = MachineBackupDirectory(target);
  for (const fs::path* directory : {&state, &mirror, &machine_dir}) {
    Backup copy{};
    if (directory->empty() || !durable::Plain(*directory)) continue;
    if (directory == &machine_dir && target.machine_backup.empty() && !TrustedMachineCopy(machine_dir / kBackupLeaf))
      continue;
    if (durable::Read(*directory / kBackupLeaf, copy, copy.magic) == durable::ReadResult::Valid &&
        copy.machine == machine && Valid(copy) && (!found || copy.created > out.created)) { out = copy; found = true; }
  }
  return found;
}
inline bool HasBackup(const fs::path& state, const fs::path& mirror, const Target& target = {}) {
  const uint64_t machine = Machine();
  Backup ignored{};
  return machine && FindBackup(state, mirror, target, machine, ignored);
}
inline bool PartlyApplied(const fs::path& state, const fs::path& mirror, const Policy& current,
                          const Target& target = {}) {
  const uint64_t machine = Machine();
  Backup backup{};
  return machine && FindBackup(state, mirror, target, machine, backup) && PartlyApplied(current, backup);
}

inline std::string Restore(const fs::path& state, const fs::path& mirror, const Target& target = {}) {
  PolicyLock lock;
  if (!lock) return "另一个显存预算操作正在进行，未修改 / Another VRAM budget operation is running; nothing changed.";
  const uint64_t machine = Machine();
  if (!machine) return "无法读取本机标识，未修改 / Cannot read this PC's identity; nothing changed.";
  const Policy current = Query(target);
  if (!current.readable) return "无法读取注册表，未修改 / Cannot read the registry; nothing changed.";
  Backup backup{};
  const bool found = FindBackup(state, mirror, target, machine, backup);
  if (found) {
    const Policy original = Original(backup);
    const Policy candidate = Candidate();
    // Accept 64/64, the original, or a mix left by an interrupted Apply.
    for (int i = 0; i < 2; ++i) {
      if (!Same(current.values[i], candidate.values[i]) && !Same(current.values[i], original.values[i]))
        return "设置已被其他程序改动，未覆盖；备份保留 / The settings were changed by another program and were not overwritten; the backup is kept.";
    }
    if (SameValues(current, original) && current.key_exists == original.key_exists)
      return "已经是应用前的原值，无需恢复 / Already at the values from before Apply; nothing to restore.";
    if (!WritePolicy(target, original) || !SameValues(Query(target), original))
      return "恢复后校验失败；备份保留，可重试 / Verification after restoring failed; the backup is kept and retry is available.";
    NoteChange(state, machine, false);
    return "已恢复应用前的原值，重启电脑后生效 / Restored the values from before Apply; restart Windows to apply.";
  }
  switch (Classify(current)) {
    case Kind::Default: return "已经是 Windows 默认值，无需恢复 / Already at the Windows defaults; nothing to restore.";
    case Kind::Candidate: break;
    default: return "没有本机的备份，且当前不是 64/64，未修改 / No backup for this PC and the values are not 64/64; nothing changed.";
  }
  // No backup, e.g. a hand-made trial patch: remove exactly the two 64/64
  // values, and the key only if that leaves it empty.
  Policy defaults;
  defaults.readable = true;
  if (!WritePolicy(target, defaults) || Classify(Query(target)) != Kind::Default)
    return "删除 64/64 设置后校验失败，可重试 / Verification after removing the 64/64 values failed; retry is available.";
  NoteChange(state, machine, false);
  return "没有找到备份；已删除这两个 64/64 设置，恢复为 Windows 默认，重启电脑后生效 / No backup was found; the two 64/64 values were removed to restore the Windows defaults. Restart Windows to apply.";
}
} // namespace dlss5::vram_budget
