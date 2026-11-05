#pragma once
#include <cstdint>
namespace dlss5::deployment_policy {
struct State {
  uint32_t magic=0x35504c44,version=1;
  uint64_t checksum=0,generation=0,fingerprint=0;
  uint32_t selected=0,confirmed=0,active=0,healthy=0,trial=0,failures=0,safe=0,nr=0,early=0;
  uint32_t confirmed_nr=0,confirmed_early=0,reserved=0;
};
inline bool Valid(const State& s) {
  return s.selected<=1 && s.confirmed<=1 && s.active<=1 && s.healthy<=1 && s.trial<=1 &&
      s.safe<=1 && s.nr<=2 && s.early<=2 && s.confirmed_nr<=2 && s.confirmed_early<=2 && s.failures<1000;
}
// Begin's answer. Non-zero values stay truthy for callers that only ask
// "did this launch change the deployment".
enum BeginResult : uint32_t { kBeginNormal=0, kBeginRolledBack=1, kBeginSafeMode=2 };
// startup-crash.marker: created empty at every launch; the crash filter writes
// '1' and an orderly unload writes 'C' after it. A crash byte always wins.
enum ExitMark : uint32_t { kExitUnknown=0, kExitCrashed=1, kExitClean=2 };
inline ExitMark ClassifyExitMark(const char* data,uint32_t size) {
  bool clean=false;
  for(uint32_t i=0;i<size;++i) {
    if(data[i]=='C') clean=true;
    else if(data[i]!=0) return kExitCrashed;
  }
  return clean ? kExitClean : kExitUnknown;
}
// clean_exit: the previous launch unloaded in order and recorded no crash. It
// forgives a short session only outside a trial, so a configuration that was
// already confirmed is not pushed into safe mode by a player quitting before
// the health point; a trial still counts it, because a crash handled by the
// game's own reporter can end in an orderly exit too.
inline uint32_t Begin(State& s,uint64_t fingerprint,bool crashed,bool clean_exit=false) {
  uint32_t restored=kBeginNormal;
  const bool environment_changed=s.fingerprint && fingerprint!=s.fingerprint;
  if(s.active && !s.healthy && !(clean_exit && !crashed && !s.trial && !environment_changed)) {
    ++s.failures;
    const bool trial=s.trial || environment_changed;
    const uint32_t limit=trial ? (crashed?1u:2u) : (crashed?2u:3u);
    if(s.failures>=limit) {
      if(trial && !environment_changed &&
          (s.selected!=s.confirmed || s.nr!=s.confirmed_nr || s.early!=s.confirmed_early)) {
        s.selected=s.confirmed; s.nr=s.confirmed_nr; s.early=s.confirmed_early;
        // The restored configuration starts with its full allowance; the
        // trial's failures were the trial's, not its own.
        s.failures=0;
        restored=kBeginRolledBack;
      } else {
        s.safe=1;
        restored=kBeginSafeMode;
      }
    }
  }
  s.trial=s.selected!=s.confirmed || s.nr!=s.confirmed_nr || s.early!=s.confirmed_early || environment_changed;
  s.fingerprint=fingerprint; s.active=1; s.healthy=0;
  return restored;
}
inline void Healthy(State& s,uint32_t sl) {
  s.healthy=1; s.trial=0; s.failures=0; s.confirmed=sl;
  s.confirmed_nr=s.nr; s.confirmed_early=s.early;
}
} // namespace dlss5::deployment_policy
