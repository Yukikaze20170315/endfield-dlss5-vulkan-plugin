#pragma once
// Include after ReShade. Captures the actual configuration API boundary,
// including startup migrations and resets; does not label every write a click.
#include "event-log.hpp"
#include <sstream>
#include <iomanip>

namespace dlss5::action {
template<uint32_t Source, typename T>
inline void SetConfig(reshade::api::effect_runtime* runtime, const char* section,
                      const char* key, const T& value) {
  try {
    char before[256]{}; size_t size=sizeof(before);
    const bool known=reshade::get_config_value(runtime,section,key,before,&size);
    before[sizeof(before)-1]=0;
    std::ostringstream line;
    line << "[action] phase=config_request origin=plugin_config_api section=" << section
         << " key=" << key << " before=" << (known?before:"<unavailable>")
         << " requested=" << std::setprecision(9) << value
         << " scope=" << (runtime?"runtime":"global") << " persisted=unconfirmed";
    eventlog::Forward(Source,eventlog::kInfo,line.str().c_str());
  } catch (...) {
    eventlog::Forward(Source,eventlog::kInfo,"[action] phase=coverage_gap reason=config_format_allocation");
  }
    reshade::set_config_value(runtime,section,key,value);
}
template<uint32_t Source>
inline void SetConfig(reshade::api::effect_runtime* runtime, const char* section,
                      const char* key, const char* value, size_t size) {
  try {
    std::ostringstream line;
    line << "[action] phase=config_array_request origin=plugin_config_api section=" << section
         << " key=" << key << " bytes=" << size << " requested=";
    for(size_t i=0;i<size;++i) line << (value[i]?value[i]:'|');
    eventlog::Forward(Source,eventlog::kInfo,line.str().c_str());
  } catch (...) {
    eventlog::Forward(Source,eventlog::kInfo,"[action] phase=coverage_gap reason=config_array_format_allocation");
  }
  reshade::set_config_value(runtime,section,key,value,size);
}
}
