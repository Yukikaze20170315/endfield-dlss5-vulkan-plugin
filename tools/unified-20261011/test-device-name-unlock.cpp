// Offline test for common/device-name-unlock.hpp (unified2k).
//
// Checks each rewritten name against the three things Endfield reads from the
// Vulkan device name: the Streamline name list (UnityPlayer 0x595d20, which
// hides DLSS), the "RTX 20" / "RTX 30" family list (0xd9c000, behind
// hg-dlss-4.5-use-limit) and the C# GPU-model regex from global-metadata.dat.
#include "../../src/unified-20261011/common/device-name-unlock.hpp"

#include <cstdio>
#include <iterator>
#include <regex>
#include <string>

namespace {

int failures = 0;

void Expect(bool ok, const std::string& what) {
  if (!ok) {
    ++failures;
    std::printf("FAIL %s\n", what.c_str());
  }
}

bool GameSkipsStreamline(const std::string& name) {
  for (const char* entry : {"RTX 2050", "RTX 3050 Ti Laptop", "RTX 3050 Laptop"}) {
    if (name.find(entry) != std::string::npos) return true;
  }
  return false;
}

bool Family(const std::string& name) {
  return name.find("RTX 20") != std::string::npos || name.find("RTX 30") != std::string::npos;
}

std::string CsharpModel(const std::string& name) {
  static const std::regex model(R"((RTX|GTX)\s+(?![A-Z])(\d+)\W*(\w+)?)");
  std::smatch match;
  return std::regex_search(name, match, model) ? match[1].str() + " " + match[2].str() : std::string();
}

struct Case {
  const char* name;
  const char* expected;
  const char* model;  // C# "{Prefix} {Model}" after the rewrite
};

}  // namespace

int main() {
  const Case cases[] = {
      {"NVIDIA GeForce RTX 3050 Laptop GPU", "NVIDIA GeForce RTX 3050 laptop GPU", "RTX 3050"},
      {"NVIDIA GeForce RTX 3050 Ti Laptop GPU", "NVIDIA GeForce RTX 3050 Ti laptop GPU", "RTX 3050"},
      {"NVIDIA GeForce RTX 2050", "NVIDIA GeForce RTX 205O", "RTX 205"},
      {"NVIDIA GeForce RTX 3050 6GB Laptop GPU", nullptr, "RTX 3050"},
      {"NVIDIA GeForce RTX 3050 A Laptop GPU", nullptr, "RTX 3050"},
      {"NVIDIA GeForce RTX 3050", nullptr, "RTX 3050"},
      {"NVIDIA GeForce RTX 3060 Laptop GPU", nullptr, "RTX 3060"},
      {"NVIDIA GeForce RTX 3070 Ti Laptop GPU", nullptr, "RTX 3070"},
      {"NVIDIA GeForce RTX 2060", nullptr, "RTX 2060"},
      {"NVIDIA GeForce RTX 4060 Laptop GPU", nullptr, "RTX 4060"},
      {"NVIDIA GeForce RTX 5090", nullptr, "RTX 5090"},
      {"NVIDIA GeForce GTX 1650", nullptr, "GTX 1650"},
      {"", nullptr, ""},
  };
  for (const Case& c : cases) {
    char buffer[dlss5::device_name::kNameSize] = {};
    std::snprintf(buffer, sizeof(buffer), "%s", c.name);
    const std::string before = buffer;
    const bool listed = dlss5::device_name::Listed(buffer);
    Expect(listed == GameSkipsStreamline(before), before + ": Listed agrees with the game's list");
    const bool changed = dlss5::device_name::Unlock(buffer);
    const std::string after = buffer;
    Expect(changed == (c.expected != nullptr), before + ": changed only when listed");
    Expect(after == (c.expected != nullptr ? c.expected : c.name), before + ": result '" + after + "'");
    Expect(after.size() == before.size(), before + ": length kept");
    Expect(!GameSkipsStreamline(after), before + ": no list entry left");
    Expect(!dlss5::device_name::Listed(buffer), before + ": not listed after unlock");
    Expect(Family(after) == Family(before), before + ": RTX 20/30 family match kept");
    Expect(CsharpModel(after) == c.model, before + ": C# model '" + CsharpModel(after) + "'");
    Expect(!dlss5::device_name::Unlock(buffer), before + ": second unlock is a no-op");
  }

  // An unterminated buffer is left alone.
  char unterminated[dlss5::device_name::kNameSize];
  std::memset(unterminated, 'X', sizeof(unterminated));
  std::memcpy(unterminated, "RTX 3050 Laptop", 15);
  Expect(!dlss5::device_name::Listed(unterminated), "unterminated: not listed");
  Expect(!dlss5::device_name::Unlock(unterminated), "unterminated: unchanged");
  Expect(unterminated[9] == 'L', "unterminated: bytes kept");
  Expect(!dlss5::device_name::Unlock(nullptr), "null name");

  // Every entry of a name is rewritten, not only the first.
  char twice[dlss5::device_name::kNameSize] = "RTX 3050 Laptop / RTX 3050 Laptop";
  Expect(dlss5::device_name::Unlock(twice), "twice: changed");
  Expect(std::string(twice) == "RTX 3050 laptop / RTX 3050 laptop", std::string("twice: '") + twice + "'");

  std::printf("%d case(s), %d failure(s)\n", static_cast<int>(std::size(cases)) + 2, failures);
  return failures == 0 ? 0 : 1;
}
