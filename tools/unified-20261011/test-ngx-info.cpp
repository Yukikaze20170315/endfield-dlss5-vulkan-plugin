#include "../../src/unified-20261011/mfg/deps/ngx/nvsdk_ngx_defs.h"
#include "../../src/unified-20261011/bridge/src/ngx-runtime-info.h"

static_assert(sizeof(BridgeNgxRuntimeInfo) == sizeof(NVSDK_NGX_FeatureCommonInfo));
static_assert(sizeof(BridgeNgxRuntimeInfo::Paths) == sizeof(NVSDK_NGX_PathListInfo));
static_assert(sizeof(BridgeNgxRuntimeInfo::Logging) == sizeof(NVSDK_NGX_LoggingInfo));
static_assert(offsetof(BridgeNgxRuntimeInfo, paths) == offsetof(NVSDK_NGX_FeatureCommonInfo, PathListInfo));
static_assert(offsetof(BridgeNgxRuntimeInfo, internal) == offsetof(NVSDK_NGX_FeatureCommonInfo, InternalData));
static_assert(offsetof(BridgeNgxRuntimeInfo, logging) == offsetof(NVSDK_NGX_FeatureCommonInfo, LoggingInfo));
static_assert(offsetof(BridgeNgxRuntimeInfo::Paths, path) == offsetof(NVSDK_NGX_PathListInfo, Path));
static_assert(offsetof(BridgeNgxRuntimeInfo::Paths, length) == offsetof(NVSDK_NGX_PathListInfo, Length));
static_assert(offsetof(BridgeNgxRuntimeInfo::Logging, callback) == offsetof(NVSDK_NGX_LoggingInfo, LoggingCallback));
static_assert(offsetof(BridgeNgxRuntimeInfo::Logging, minimum_level) == offsetof(NVSDK_NGX_LoggingInfo, MinimumLoggingLevel));
static_assert(offsetof(BridgeNgxRuntimeInfo::Logging, disable_other_sinks) == offsetof(NVSDK_NGX_LoggingInfo, DisableOtherLoggingSinks));
