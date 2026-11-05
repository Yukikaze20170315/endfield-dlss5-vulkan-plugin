from pathlib import Path
root=Path(__file__).resolve().parents[2]/'src/unified-20261011/bridge/src'
p=root/'fg-relay.inc';s=p.read_text(encoding='utf-8')
s=s.replace('''    static volatile LONG logged_accept, logged_retry, logged_present;''','''    static volatile LONG logged_accept, logged_retry, logged_present;
    static volatile LONG injection_declined;''')
s=s.replace('''    if (caller != nullptr && caller->sType == kFgrStDeviceCreateInfo &&''','''    if (!InterlockedCompareExchange(&injection_declined,0,0) &&
        caller != nullptr && caller->sType == kFgrStDeviceCreateInfo &&''')
a=s.index('    if (added && result != kSVkSuccess)\n');b=s.index('    if (result == kSVkSuccess && out != nullptr && *out != nullptr) FgGuideNoteDevice',a)
s=s[:a]+'''    if (result != kSVkSuccess) {
        // VkLayerDeviceCreateInfo contains mutable loader links. The first
        // call can consume them even when device creation fails. Reusing this
        // chain inside the layer is unsafe; only the application can rebuild it.
        if(added) InterlockedExchange(&injection_declined,1);
        if(out) *out=nullptr;
        deployment::DeviceCreateFailed(result);
        g_fgr_inject_note="device creation declined; returned to the application without reusing its layer chain";
        if(InterlockedCompareExchange(&logged_retry,1,0)==0)
            Log("[fg-relay] vkCreateDevice result=%u; no internal retry. Future calls preserve the caller's extension list.",result);
        return result;
    }
'''+s[b:]
p.write_text(s,encoding='utf-8')
p=root/'vkmirror.inc';s=p.read_text(encoding='utf-8')
s=s.replace('''static void OnVkmInitCommandList(reshade::api::command_list *cmd)
{''','''static void OnVkmInitCommandList(reshade::api::command_list *cmd)
{
    if (DeploymentSuspended()) return;''')
s=s.replace('''static void OnVkmInitRuntime(reshade::api::effect_runtime *rt)
{''','''static void OnVkmInitRuntime(reshade::api::effect_runtime *rt)
{
    if (DeploymentSuspended()) return;''')
p.write_text(s,encoding='utf-8')
print('Vulkan chain retry removed; failure suspension wired')
