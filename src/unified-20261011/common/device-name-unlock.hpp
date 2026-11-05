#pragma once
#include <cstddef>
#include <cstring>

// Arknights: Endfield's UnityPlayer.dll (prod_obt1.6.3, SHA256 9AA9397A...2D0)
// creates its Vulkan device without Streamline -- no DLSS, DLSS-G or Reflex --
// when VkPhysicalDeviceProperties.deviceName contains one of the entries below
// (case-sensitive std::string::find; function 0x595d20, table 0x1b3e3e0).
// The same function also matches "RTX 20" / "RTX 30" (0xd9c000, behind
// hg-dlss-4.5-use-limit) and returns the result to the game's C# side as
// HGDLSSUtil.IsStreamlineDLSSUpscalerModelSwitchRecommend, so every rule
// changes one character that lies outside those family prefixes.
namespace dlss5::device_name {

constexpr size_t kNameSize = 256;  // VK_MAX_PHYSICAL_DEVICE_NAME_SIZE

struct Rule {
  const char* entry;  // the game's entry, as stored
  size_t at;          // offset of the character to change
  char to;
};

inline constexpr Rule kRules[] = {
    {"RTX 3050 Ti Laptop", 12, 'l'},  // "Laptop" -> "laptop" (RTX 3050 Laptop: verified)
    {"RTX 3050 Laptop", 9, 'l'},
    // No letter follows "RTX 20" here, so the last digit changes; the
    // game's C# GPU-model regex then reads model 205 instead of 2050.
    {"RTX 2050", 7, 'O'},
};

inline bool Terminated(const char* name, size_t capacity) {
  return name != nullptr && std::memchr(name, 0, capacity) != nullptr;
}

// True when the game would skip Streamline for this name.
inline bool Listed(const char* name, size_t capacity = kNameSize) {
  if (!Terminated(name, capacity)) return false;
  for (const Rule& rule : kRules) {
    if (std::strstr(name, rule.entry) != nullptr) return true;
  }
  return false;
}

// Rewrites a listed name in place (same length); true when it changed.
inline bool Unlock(char* name, size_t capacity = kNameSize) {
  if (!Terminated(name, capacity)) return false;
  bool changed = false;
  for (const Rule& rule : kRules) {
    for (char* hit = std::strstr(name, rule.entry); hit != nullptr;
         hit = std::strstr(hit + 1, rule.entry)) {
      hit[rule.at] = rule.to;
      changed = true;
    }
  }
  return changed;
}

}  // namespace dlss5::device_name
