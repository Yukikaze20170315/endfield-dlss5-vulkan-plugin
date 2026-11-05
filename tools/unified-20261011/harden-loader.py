from pathlib import Path
p=Path(__file__).resolve().parents[2]/'src/unified-20261011/bridge/src/deploy-observer.inc'
s=p.read_text(encoding='utf-8')
a=s.index('static void SldDescribePreferences(');b=s.index('static bool SldFieldsPlausible(',a)
s=s[:a]+'''static void SldDescribePreferences(const SldPreferencesV1 *p, const char *label) {
    // Never follow strings or lists merely to print diagnostic data.
    ObsLog("[sl-dir] %s: version=%zu flags=0x%llx renderAPI=%u paths=%u features=%u",
           label,p->structVersion,static_cast<unsigned long long>(p->flags),p->renderAPI,
           p->numPathsToPlugins,p->numFeaturesToLoad);
}
static bool SldCopyPreferences(const SldPreferencesV1* source,SldPreferencesV1* copy) {
    __try { if(!source) return false; memcpy(copy,source,sizeof(*copy)); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool SldReadable(const void* p,size_t size) {
    if(!p || reinterpret_cast<uintptr_t>(p)+size<reinterpret_cast<uintptr_t>(p)) return false;
    auto current=reinterpret_cast<uintptr_t>(p);
    const auto end=current+size;
    while(current<end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if(!VirtualQuery(reinterpret_cast<void*>(current),&mbi,sizeof(mbi)) || mbi.State!=MEM_COMMIT ||
           (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        const auto next=reinterpret_cast<uintptr_t>(mbi.BaseAddress)+mbi.RegionSize;
        if(next<=current) return false;
        current=next;
    }
    return true;
}
''' +s[b:]
s=s.replace('''    if (p->numPathsToPlugins > 64 || p->numFeaturesToLoad > 256) return false;''','''    if (p->structVersion > 1 || p->next != nullptr || p->renderAPI != 2 ||
        p->numPathsToPlugins > 64 || p->numFeaturesToLoad > 256) return false;''')
s=s.replace('!SldReadable(p->pathsToPlugins)','!SldReadable(p->pathsToPlugins,p->numPathsToPlugins*sizeof(void*))')
s=s.replace('!SldReadable(p->featuresToLoad)','!SldReadable(p->featuresToLoad,p->numFeaturesToLoad*sizeof(uint32_t))')
s=s.replace('''    const SldPreferencesV1 *use = pref;
    SldPreferencesV1 copy{};
    if (pref == nullptr)''','''    const SldPreferencesV1 *use = pref;
    SldPreferencesV1 copy{};
    SldPreferencesV1 snapshot{};
    const SldPreferencesV1* const original_pref=pref;
    if (!SldCopyPreferences(pref,&snapshot))''')
s=s.replace('''        SldDescribePreferences(pref, "game");''','''        pref=&snapshot;
        SldDescribePreferences(pref, "game");''')
s=s.replace('deployment::SlResult(result, use != pref);','deployment::SlResult(result, use != original_pref);')
s=s.replace('static PFN_SldSlInit  g_sld_original;','static PVOID volatile g_sld_original;')
s=s.replace('''    const int result = g_sld_original(use, sdk_version);''','''    const auto original=reinterpret_cast<PFN_SldSlInit>(InterlockedCompareExchangePointer(&g_sld_original,nullptr,nullptr));
    const int result = original(use, sdk_version);''')
p.write_text(s,encoding='utf-8')
print('SL ABI reads bounded; diagnostic pointer traversal removed')
