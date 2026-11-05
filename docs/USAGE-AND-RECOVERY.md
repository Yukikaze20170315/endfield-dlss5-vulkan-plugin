# DLSS5 unified2l 候选：使用与恢复 / Usage and recovery

本套件用于《明日方舟：终末地》Vulkan、支持插件的 ReShade 6，以及 NVIDIA RTX 显卡。RTX 20、30、40、50 系列使用同一套 Bridge、Generic NR、MFG Unlock 和运行库文件，无需按显卡选择部署包。unified2l 是 2026-11-03 的候选版本（三个插件的界面精简、新的首装默认值、新的截图方式；游戏外恢复工具只保留完全卸载），已通过编译和离线自动化测试，尚未完成游戏内、跨显卡验收。

This package targets Arknights: Endfield with Vulkan, an add-on capable ReShade 6 installation, and NVIDIA RTX graphics. RTX 20, 30, 40 and 50 series use the same Bridge, Generic NR, MFG Unlock and runtime files; users do not select a package by GPU generation. Unified2l is a candidate from 2026-11-03 (simplified panels in all three add-ons, new first-install defaults and a new screenshot method; the external recovery tool performs only a full uninstall). It passed the build and offline automated tests and has not been accepted in game or across GPUs.

## 本版改动 / Changes in this build

unified2l（更新 Generic NR `v8.5.0-rc10-unified2l-20261103` 和本说明；Bridge、MFG Unlock、卸载工具、恢复助手和运行库与 unified2k 相同）：

1. DLSS5 页“结果 · 实时，重塑 NR 输出”标题下恢复“调整 NR 的外观”开关。它一键控制下方“结果”和“限制与稳定性”里的全部参数：关闭时这些参数都不生效（滑块变灰），NR 输出保持原样；打开时按滑块的值调整。来回切换即可对比调色前后的画面，滑块的值不会因切换而改变。
2. 上述参数及该开关的默认值恢复为 unified2c 之前的原始值：开关默认关闭；编辑强度、提亮、压暗、色彩、色相偏移、阴影、中间调、高光、大尺度色调、细节均为 1.00；最大提亮、最大压暗、最大色彩变化均为 0.00 档；细节半径 1.00%；暗晕抑制、暗色物体旁的暗晕均为 0.00。这些参数为默认值且开关打开时，画面与关闭时基本相同。
3. 默认值只用于配置里没有保存该项的情况：没改过的参数自动变为上面的新默认值；改过并已保存的参数保持原值，右侧的重置按钮会恢复到新默认值。unified2c～2k 期间开关被强制打开、不读配置，因此升级后开关按配置中保存的值显示；配置里没有保存时显示为关闭，需要时手动打开。

unified2l (updates Generic NR `v8.5.0-rc10-unified2l-20261103` and this document; Bridge, MFG Unlock, uninstaller, restore helper and runtime files are unchanged from unified2k):

1. The "Adjust NR's look" switch is back under the "Result · live, reshapes NR's output" caption on the DLSS5 page. It turns every parameter under Result and Limits and stability on or off at once: off, none of them applies (the sliders are greyed out) and NR's output is left as it is; on, the slider values apply. Toggling it compares the image before and after the adjustments; the slider values do not change when it is toggled.
2. These parameters and the switch return to their defaults from before unified2c: the switch is off; Edit strength, Brightening, Darkening, Colour, Hue shift, Shadows, Midtones, Highlights, Large-scale tone and Fine detail are 1.00; Max brightening, Max darkening and Max colour change are 0.00 stops; Detail radius is 1.00 %; Halo suppression and Halo next to dark objects are 0.00. With these defaults the image with the switch on is essentially the same as with it off.
3. Defaults apply only where the configuration has no saved value: unchanged parameters take the new defaults, while changed and saved parameters keep their values and their reset button returns them to the new defaults. From unified2c to unified2k the switch was forced on and its saved value was not read; after the update it shows the saved value, or off when none is saved, and can be turned on manually.

以下为 unified2k 的改动 / unified2k changes:

unified2k（更新 Generic NR `v8.5.0-rc10-unified2k-20261102` 和本说明；Bridge、MFG Unlock、卸载工具、恢复助手和运行库与 unified2j 相同）：

1. 解锁被游戏隐藏的 DLSS。游戏在 Vulkan 模式下，只要显卡名称包含 `RTX 2050`、`RTX 3050 Ti Laptop` 或 `RTX 3050 Laptop`，创建图形设备时就不经过 Streamline，“画质提升”里没有 NVIDIA DLSS，DLSS 帧生成和 Reflex 也不可用（DX11 模式没有这项限制）。本版在游戏读取 Vulkan 显卡名称时改动一个字符，使这三项不再匹配：`RTX 3050 Laptop` 与 `RTX 3050 Ti Laptop` 中的 `Laptop` 改为 `laptop`；`RTX 2050` 改为 `RTX 205O`（最后一位数字 0 改为字母 O）。显卡、驱动和 NGX 都不受影响，其他显卡名称不改。
   - 验证情况：在 RTX 3050 Laptop（4 GB）上用同一改法的独立诊断插件实测，游戏的 Vulkan 设备带上了 DLSS 所需扩展，“画质提升”出现 NVIDIA DLSS 并可以开启。本版插件本身尚未实机运行；RTX 3050 Ti Laptop 与 RTX 2050 没有实测。
   - 已知影响：游戏中其他读取显卡名称的地方也会看到改后的名称。游戏按名称中的 `RTX 20`/`RTX 30` 决定是否推荐切换 DLSS 超分模型，这一判断不受影响。游戏脚本按“RTX + 型号数字”识别显卡（推测用于默认画质档）：RTX 3050 系列仍识别为 3050；RTX 2050 会被识别为 205，默认画质可能与原来不同，可以在游戏设置中手动调整。
   - 4 GB 显存的笔记本显卡开启 DLSS 后显存仍然紧张，游戏可能提示“预估显存占用：接近超载”。
   - 以下情况不改名：进程不是 `Endfield.exe`；DLSS5Bridge 处于安全模式；DLSS5Bridge 页的 NR 钩子模式为关闭（`EnableHooks=0`）；或在 `ReShade.ini` 的 `[RenoDX.DLSS5]` 中写 `NRVkDeviceNameUnlock=0`。是否启用以及改前、改后的名称记录在 `ReShade.log`，以 `Vulkan device-name unlock` 开头。
2. 说明更正：`NROverlayCursorConfine` 位于 `ReShade.ini` 的 `[RenoDX.DLSS5]` 节，不是 `[DLSS5]`。

unified2k (updates Generic NR `v8.5.0-rc10-unified2k-20261102` and this document; Bridge, MFG Unlock, uninstaller, restore helper and runtime files are unchanged from unified2j):

1. DLSS that the game hides is unlocked. In Vulkan mode the game creates its graphics device without Streamline whenever the GPU name contains `RTX 2050`, `RTX 3050 Ti Laptop` or `RTX 3050 Laptop`, so NVIDIA DLSS is missing from the upscaler list and DLSS Frame Generation and Reflex are unavailable (DX11 mode has no such restriction). This build changes one character of the Vulkan GPU name the game reads so that none of the three entries match: `Laptop` becomes `laptop` in `RTX 3050 Laptop` and `RTX 3050 Ti Laptop`; `RTX 2050` becomes `RTX 205O` (the last digit 0 becomes the letter O). The GPU, the driver and NGX are unaffected, and no other GPU name is changed.
   - Verification: on an RTX 3050 Laptop (4 GB), a standalone diagnostic add-on applying the same change made the game's Vulkan device carry the extensions DLSS needs, and NVIDIA DLSS appeared and could be enabled. This build's add-on itself has not run in game yet; RTX 3050 Ti Laptop and RTX 2050 were not tested.
   - Known effects: other places in the game that read the GPU name see the changed name. The game decides from `RTX 20`/`RTX 30` in the name whether to recommend switching the DLSS upscaler model; that decision is unchanged. The game's scripts identify the GPU as "RTX + model number" (presumably for default quality presets): the RTX 3050 models are still read as 3050, while RTX 2050 is read as 205, so default quality settings may differ; adjust them in the game settings if needed.
   - VRAM stays tight on 4 GB laptop GPUs with DLSS on; the game may report the estimated VRAM use as close to overload.
   - The name is not changed when the process is not `Endfield.exe`, when DLSS5Bridge is in safe mode, when the NR hook mode on the DLSS5Bridge page is off (`EnableHooks=0`), or with `NRVkDeviceNameUnlock=0` under `[RenoDX.DLSS5]` in `ReShade.ini`. `ReShade.log` records whether it is on and the names before and after, in lines starting with `Vulkan device-name unlock`.
2. Documentation fix: `NROverlayCursorConfine` belongs in the `[RenoDX.DLSS5]` section of `ReShade.ini`, not `[DLSS5]`.

以下为 unified2j 的改动 / unified2j changes:

unified2j（更新 MFG Unlock `0.11+unified2j-20261102` 和本说明；Bridge、Generic NR、卸载工具、恢复助手和运行库与 unified2i 相同）：

1. MFG Unlock 页的两句提示在面板太窄（或字体调大）放不下时改为自动换行显示，不再被截断；这种情况下“游戏内”不加粗。

unified2j (updates MFG Unlock `0.11+unified2j-20261102` and this document; Bridge, Generic NR, uninstaller, restore helper and runtime files are unchanged from unified2i):

1. When the panel is too narrow (or the font too large) for an MFG Unlock hint line, the line wraps instead of being cut off; the bold emphasis is dropped in that case.

以下为 unified2i 的改动 / unified2i changes:

unified2i（MFG Unlock `0.11+unified2i-20261102`；Bridge、Generic NR、卸载工具、恢复助手和运行库与 unified2h 相同）：

1. MFG Unlock 页的两句提示移到“高级设置”和“启用 MFG Unlock”之间，上下各有一条分割线；提示文字为淡色，“游戏内”三个字加粗。文字改为：“使用本插件时，游戏内的NVIDIA帧生成必须保持开启。”和“游戏内可以选择2~4x的任何档位，但不要选择“自动”档位，也不要不开启。”

unified2i (updates MFG Unlock `0.11+unified2i-20261102` and this document; Bridge, Generic NR, uninstaller, restore helper and runtime files are unchanged from unified2h):

1. The two MFG Unlock hint lines now sit between "Advanced settings" and "Enable MFG Unlock", with a separator above and below; the text is dimmed and "in the game" is emphasized in bold.

以下为 unified2h 的改动 / unified2h changes:

unified2h（更新 Generic NR 插件和本说明；Bridge、MFG Unlock、卸载工具、恢复助手和运行库与 unified2g 相同）：

1. 截图键或 NR 开关键设为 PrintScreen 时，在 ReShade 菜单打开期间按下（包括设置按键时按的那一下），关闭菜单后不会再多截一张图或多切换一次 NR。

unified2h (updates the Generic NR add-on and this document; Bridge, MFG Unlock, uninstaller, restore helper and runtime files are unchanged from unified2g):

1. With PrintScreen as the screenshot or NR toggle key, a press made while the ReShade overlay is open (including the rebind keystroke) no longer causes an extra screenshot or toggle after the overlay closes.

以下为 unified2g 的改动 / unified2g changes:

unified2g（更新 Generic NR 与 MFG Unlock 插件和本说明；Bridge、卸载工具、恢复助手和运行库与 unified2f 相同）：

1. 删除“Neural Rendering 部分启用 / 正在增强 X/Y”状态：只要 NR 在工作，提示区就显示正常的绿色对勾状态。
2. 截图键可以设为 PrintScreen：之前设置时会显示成“0x42”且按下不截图，现已修正（旧版存下的错误键值启动时自动改回 PrintScreen）。“截图保存位置”右侧新增文件夹按钮，可以在资源管理器中选择文件夹；输入框为空时显示“请输入截图保存地址”。
3. DLSS5 页每一层 NR 的上方和下方都有分割线。
4. MFG Unlock 页的提示改为两句：“使用本插件时，游戏内的NVIDIA帧生成必须保持开启。”和“可以选择2~4x的任何档位，但不要选择“自动”档位，也不要不开启。”
5. 打开 ReShade 菜单时鼠标不再跑出游戏窗口：如果打开菜单前游戏锁着鼠标（第一/第三人称操作角色时），菜单打开期间鼠标限制在游戏窗口内（多显示器时不会跑到其他屏幕），关闭菜单后恢复游戏原来的鼠标锁定，不需要再按两次 Win 键。打开菜单前没有锁鼠标时（例如在游戏菜单里）不做任何改变。如需关闭此功能，在 `ReShade.ini` 的 `[RenoDX.DLSS5]` 中写 `NROverlayCursorConfine=0`。

unified2g (updates the Generic NR and MFG Unlock add-ons and this document; Bridge, uninstaller, restore helper and runtime files are unchanged from unified2f):

1. The "Neural Rendering partly active / enhancing X/Y" state is removed; while NR works the notice area shows the normal green status.
2. The screenshot key can be PrintScreen (it previously showed as "0x42" and did not capture; a saved wrong value is migrated at startup). A folder button next to "Screenshot folder" opens a folder picker; the empty field shows a hint.
3. Each NR layer on the DLSS5 page has a separator above and below it.
4. The MFG Unlock page hint is replaced by two sentences: in-game NVIDIA frame generation must stay on; choose any fixed 2x-4x setting, not "Auto" and not off.
5. Opening the ReShade overlay no longer lets the cursor leave the game: if the game had the cursor locked when the overlay opened, the cursor stays inside the game window while it is open, and the game's lock is restored on close. Nothing changes when the game had no lock. Disable with `NROverlayCursorConfine=0` under `[RenoDX.DLSS5]` in `ReShade.ini`.

以下为 unified2f 的改动 / unified2f changes:

unified2f（只更新本说明文档；三个插件、卸载工具、恢复助手和运行库与 unified2e 相同）：

1. 说明更正：DLSS5Bridge 页的安全模式、导出诊断和驱动配置位于“高级设置”中；处于安全模式时，安全模式与导出诊断不勾选“高级设置”也会显示。

unified2f (updates only this document; the three add-ons, uninstaller, restore helper and runtime files are unchanged from unified2e):

1. Documentation fix: on the DLSS5Bridge page, safe mode, diagnostics export and driver settings are under Advanced settings; while safe mode is on, safe mode and diagnostics export are shown without it.

以下为 unified2e 的改动 / unified2e changes:

unified2e（在 unified2d 基础上只更新 MFG Unlock `0.11+unified2e-20261101`；Bridge、Generic、卸载工具、恢复助手和运行库与 unified2d 相同）：

1. 安全模式下 MFG Unlock 页的解除说明不再提已删除的“故障与恢复”标题，改为“在 DLSS5Bridge 页取消勾选“下次启动安全模式”，然后重启游戏”。

unified2e (updates only MFG Unlock `0.11+unified2e-20261101` on top of unified2d; Bridge, Generic, uninstaller, restore helper and runtime files unchanged from unified2d):

1. In safe mode the MFG Unlock page no longer points to the removed "Recovery" heading; it now says to clear "Safe mode next launch" on the DLSS5Bridge page and restart the game.

以下为 unified2d 的改动 / unified2d changes:

unified2d（在 unified2c 基础上修正审查发现的三个问题：Bridge `1.4.13-pre8-unified2d-20261101`，Generic `v8.5.0-rc10-unified2d-20261101`；MFG Unlock、卸载工具、恢复助手和运行库与 unified2c 相同）：

1. Windows 显存预算：如果“开启”只写入了一半就失败（极少见），面板现在两个选项都不选中，点“关闭”即可恢复原值；在这种状态下再点“开启”，会沿用原来的备份，不会用写了一半的值覆盖备份，之后关闭或完全卸载时仍能恢复真正的原值。
2. 切换分辨率档后，DLSS5 页提示区会自动翻到原有的“可能短暂卡顿”提示页，保留 20 秒（2c 中这条提示没有显示）。
3. 说明文档更正：不开 ReShade 面板时左上角只有“正在启动...”和“Neural Rendering 已启用”两种状态，NR 关闭时不显示。

unified2d (fixes three review findings on top of unified2c; Bridge `1.4.13-pre8-unified2d-20261101`, Generic `v8.5.0-rc10-unified2d-20261101`; MFG Unlock, uninstaller, restore helper and runtime files unchanged from unified2c):

1. Windows VRAM budget: if turning it on stopped half-way (rare), neither option is selected and Off restores the original values; turning it on from that state reuses the existing backup instead of overwriting it with the half-written values, so Off or the full uninstall still restores the real original.
2. After a resolution tier change the DLSS5 notice area turns to the existing "may stutter for a moment" page for 20 seconds (2c never showed it).
3. Documentation fix: with the overlay closed the top-left corner shows only "Starting..." and "Neural Rendering is active"; nothing is shown while NR is off.

以下为 unified2c 的改动 / unified2c changes:

unified2c（三个插件都更新：Bridge `1.4.13-pre8-unified2c-20261101`，Generic `v8.5.0-rc10-unified2c-20261101`，MFG Unlock `0.11+unified2c-20261101`；卸载工具、恢复助手和运行库与 unified2b 相同）：

1. DLSS5（NR）页精简：删除欢迎卡、“设置／我的”预设行、诊断区、问题卡片、“报告这些问题”功能、Streamline 挂钩黄字提示和页面底部的版本行。不开 ReShade 面板时，左上角只会显示两种状态：NR 准备期间的“正在启动...”，以及 NR 开始工作后显示几秒并渐隐的“Neural Rendering 已启用”；NR 关闭时角落不显示任何内容。
2. 以下内容移到“高级设置”里，不勾选高级设置就看不到：渲染层数整行；超分后层数、呈现层数的 2～4 层（不勾选时只能选 0 和 1）；HDR纸白、保留HUD、生成帧、上采样、细节稳定；“限制和稳定性”里的稳定、稳定时间、稳定细节。“当前工作分辨率”一行删除；“分辨率比例”滑块移到“分辨率”下方；“调整NR的外观”不再是开关，始终生效；“限制和稳定性”始终展开。已保存的参数不会因为隐藏而被重置。
3. 随时可能出现的提示统一放进固定大小的提示区：内容多时用“<”“>”翻页，提示出现或消失都不会把下面的按钮挤动位置。切换分辨率档后，提示区会自动翻到“可能短暂卡顿”的提示页，保留 20 秒。DLSS5 页、DLSS5Bridge 页、MFG Unlock 页都这样处理。
4. 截图：每按一次截图键（默认 F5）只保存一张未经二次处理的 PNG，内容是 NR 全部层（含呈现层）处理后、交给屏幕的最终游戏画面；HDR 画面保持 HDR。“快捷键与截图”区放在 DLSS5 页最下方，不用勾选高级设置；在“截图保存位置”里可以直接填写保存文件夹，也可以点右侧的文件夹按钮选择，留空时保存到游戏目录下的 `DLSS5 Screenshots` 文件夹。“截图状态”、文件格式、截图来源、对比布局选项已删除。为避免同一按键保存两种不同的截图，插件启动时会把 ReShade 自己的截图键（`ReShade.ini` 中 `[INPUT] KeyScreenshot`，默认 PrintScreen）设为 0（不使用）；需要 ReShade 自己截图时可在 ReShade 设置里重新指定按键，但下次启动会再次被清零。注意：截图不包含其他 ReShade 特效和 ReShade 菜单本身。
5. 首次安装（配置里还没有保存这些值）时的默认参数改为：渲染层数 0、超分后层数 0、呈现层数 1、分辨率“性能”；呈现第 1 层强度 2.00、HDR传递 1.00、色彩 1.00、预设“默认”、风格“电影感”、局部色调 1.50、结构 2.00、角色／皮肤 0、角色遮罩开、独立皮肤开、UI矫正开；调整NR的外观：编辑强度 1.20、提亮 1.00、压暗 0.75、色彩 0.75、色相偏移 1.00、阴影 0.70、中间调 1.00、高光 0.50；限制和稳定性：最大提亮 0.00、最大压暗 0.00、最大色彩变化 0.00、大尺度色调 1.00、细节 1.50、细节半径 1.00%、暗晕抑制 0.00、暗色物体旁的暗晕 0.00。已有配置里保存的值继续保留，不会被覆盖。
6. DLSS5Bridge 页：“统一部署”和“NR钩子模式”只保留选项，删除下方说明；“故障与恢复”的标题和说明删除，安全模式、导出诊断两个勾选项保留，位于“高级设置”中（处于安全模式时不勾选也会显示）。“Windows 显存预算”改为“开启／关闭”二选一，说明只保留三行：Windows 会限制本插件的显存使用，打开该开关可以解除一定程度的限制；对于 8G 显存的显卡来说，大约能多用几百 M 左右的显存；打开或关闭后需要重启电脑才能生效。不再弹出确认对话框。
7. MFG Unlock 页：“所选倍率尚未生效或不可用”这类提示放在固定的两行提示区里。

1. The DLSS5 (NR) page is simplified: the welcome card, the Settings/My preset row, the diagnostics section, issue cards, the issue-report feature, the yellow Streamline hook note and the version footer are removed. With the ReShade overlay closed, the top-left corner shows only two states: "Starting..." while NR gets ready, and "Neural Rendering is active", which fades out a few seconds after NR starts working; nothing is shown while NR is off.
2. These items now appear only with Advanced settings ticked: the Render layer row; layers 2-4 for Upscaled and Present (otherwise only 0 and 1); HDR paper white, Keep HUD, Generated frames, Upscaling and Detail stability; Stabilize, Stabilize time and Stabilize detail under Limits and stability. The current working resolution line is removed; the resolution scale slider is placed under Resolution; "Adjust NR's look" is always active instead of a toggle; Limits and stability is always expanded. Hiding items does not reset saved values.
3. Notices that can appear at any time are placed in a fixed-size notice area with "<" and ">" paging, so they never push the buttons below them. After a resolution tier change the area turns to the "may stutter for a moment" page for 20 seconds. This applies to the DLSS5, DLSS5Bridge and MFG Unlock pages.
4. Screenshots: each press of the screenshot key (F5 by default) saves one unprocessed PNG of the final game image after every NR layer, including Present layers; HDR stays HDR. The Hotkeys and screenshot section sits at the bottom of the DLSS5 page without Advanced settings, and "Screenshot folder" sets the destination (type a path or use the folder button next to it); empty means a `DLSS5 Screenshots` folder in the game directory. Screenshot status, file format, source and comparison layout options are removed. To avoid one key saving two different pictures, the add-on sets ReShade's own screenshot key (`[INPUT] KeyScreenshot` in `ReShade.ini`, PrintScreen by default) to 0 (unused) at startup; it can be reassigned in ReShade's settings but is cleared again on the next start. The screenshot does not include other ReShade effects or the ReShade overlay.
5. New first-install defaults (used only where no value is saved yet): Render layers 0, Upscaled layers 0, Present layers 1, Resolution Performance; Present layer 1 Strength 2.00, HDR transfer 1.00, Color 1.00, Preset Default, Style Cinematic, Local tone 1.50, Structure 2.00, Character/skin 0, Character mask on, Independent skin on, UI correction on; Adjust NR's look: Edit strength 1.20, Lift 1.00, Darken 0.75, Color 0.75, Hue shift 1.00, Shadows 0.70, Midtones 1.00, Highlights 0.50; Limits and stability: Max lift 0.00, Max darken 0.00, Max color change 0.00, Large-scale tone 1.00, Detail 1.50, Detail radius 1.00%, Halo suppression 0.00, Dark halo beside dark objects 0.00. Values already saved in an existing configuration are kept.
6. DLSS5Bridge page: Deployment and NR hook mode keep only their options; the Recovery heading and description are removed while the safe-mode and export-diagnostics boxes stay under Advanced settings (shown without it while safe mode is on). The Windows VRAM budget is an On/Off choice with three lines of explanation (Windows limits this add-on's VRAM use and the switch lifts part of that limit; on an 8 GB card that is roughly a few hundred MB more; restart the PC after turning it on or off). No confirmation dialog opens.
7. MFG Unlock page: notices such as "the selected multiplier is not active or unavailable" appear in a fixed two-line notice area.

unified2b（在下列 unified2a、unified1z 改动之外，Bridge 更新为 `1.4.13-pre8-unified2b-20261101`；Generic、MFG Unlock 与 unified1y 相同）：

1. 游戏崩溃时，如果其他组件（例如游戏自带的崩溃上报）在 Bridge 之后又把自己的崩溃处理程序重新挂到最上层，该处理程序以前会被执行两次（可能写出两份崩溃报告，极少数情况下卡住）。现在每个崩溃处理程序只执行一次。只影响游戏已经崩溃之后的处理，正常运行不受影响。

1. When the game crashes after another component (for example the game's own crash reporter) has installed its crash handler on top again after the Bridge, that handler used to run twice (possibly writing two crash reports or, rarely, hanging). Each crash handler now runs once. This only affects handling after the game has already crashed; normal play is unaffected.

unified2a（只改卸载脚本和本说明）：

1. 卸载时恢复助手 `DLSS5-RestoreDriver.exe` 输出的说明（例如“另一个显存预算操作正在进行”“驱动记录无效”“重启 Windows 后生效”）现在会显示在卸载窗口和管理员输出报告里，便于判断“还原未完成”时是重试即可还是需要处理其他问题。

1. Messages printed by the restore helper `DLSS5-RestoreDriver.exe` during the uninstall (for example that another VRAM budget operation is running, that a driver journal is invalid, or that Windows must restart) now appear in the uninstall window and in the elevated output report, so an incomplete restore shows whether a retry is enough.

unified1z：

1. 驱动设置或 Windows 显存预算没有完全还原时，窗口不再显示“卸载完成”，而是说明“插件已撤除、但还原未完成”，并提示关闭其他程序（尤其是 NVIDIA App、NVIDIA 控制面板）后再双击运行一次；管理员输出报告保留在系统临时目录，窗口显示其路径。
2. 卸载时，恢复助手会在游戏目录重新建出 `dlss5\state` 并写入状态记录。全部还原成功后，这些记录也移入备份的 `leftover` 子文件夹，`dlss5`、`reshade-shaders` 留下的空文件夹一并删除。
3. Bridge 在游戏根目录写的 `DLSS5-上次启动异常-说明.txt`，以及旧版本留在根目录的 `RenoDX-DLSS5-normtrace.csv`、`RenoDX-DLSS5-edittrace.csv`，也纳入卸载。

1. If the driver settings or the Windows VRAM budget were not fully restored, the window no longer says the uninstall finished. It says the add-ons were removed but the restore is incomplete, and asks to close other programs (especially NVIDIA App and NVIDIA Control Panel) and run it again. The elevated output report stays in the system temporary folder and its path is shown.
2. During the uninstall the restore helper recreates `dlss5\state` in the game folder and writes state records there. After a fully successful restore these records move into the backup's `leftover` folder, and the emptied `dlss5` and `reshade-shaders` folders are removed.
3. `DLSS5-上次启动异常-说明.txt`, written by the Bridge into the game root, and `RenoDX-DLSS5-normtrace.csv` and `RenoDX-DLSS5-edittrace.csv` left in the root by older builds are now uninstalled too.

unified1y：

1. 游戏外工具 `DLSS5-Recovery 故障恢复.cmd` 只保留“完全卸载”：双击后窗口说明将要做的事，输入 Y 回车才执行，不再有数字菜单。安全模式、驱动设置还原、诊断导出（这三项需先勾选“高级设置”）和 Windows 显存预算还原改在游戏内 DLSS5Bridge 页操作；完全卸载时会自动还原驱动设置和显存预算。
2. 修复：旧版恢复菜单在部分电脑上选 A 会错误执行“停用插件”（Windows 命令行在含中文的 UTF-8 批处理中跳转到了错误位置）。新入口是只含英文字符、没有跳转的单行启动器。
3. 完全卸载全部成功后，卸载工具自身（cmd、ps1、`DLSS5-RestoreDriver.exe`、本说明）和旧版留下的 `DLSS5-Clean-Recovery-result.txt` 也移入备份目录的 `tools` 子文件夹，游戏目录不再残留本套件文件；管理员输出报告改写到系统临时目录，成功后自动删除。驱动或显存预算没有完全还原时，工具留在原处，方便再运行一次。
4. 游戏内 Bridge、MFG Unlock 页的文字同步修改，不再提到恢复菜单选项。
5. 终末地本身以管理员身份运行，所以在游戏内点“Windows 显存预算”等需要管理员的按钮时，通常不会再单独弹出 UAC 窗口，直接生效，这是正常现象。

1. The external `DLSS5-Recovery 故障恢复.cmd` now only performs a full uninstall: it explains what it will do and runs only after Y and Enter; there is no numbered menu. Safe mode, driver setting restore, diagnostics export (these three under Advanced settings) and Windows VRAM budget restore are on the in-game DLSS5Bridge page; a full uninstall restores the driver settings and the VRAM budget automatically.
2. Fixed: on some PCs, choosing A in the old recovery menu disabled the add-ons instead (cmd.exe jumped to the wrong place in a UTF-8 batch file containing Chinese text). The new entry point is an ASCII-only, single-line launcher without jumps.
3. After a fully successful uninstall, the uninstaller's own files (cmd, ps1, `DLSS5-RestoreDriver.exe`, this document) and a `DLSS5-Clean-Recovery-result.txt` left by older builds are moved into the backup's `tools` folder, so no package file stays in the game folder. The elevated output report is written to the system temporary folder and deleted on success. If the driver settings or the VRAM budget were not fully restored, the tools stay in place so it can simply be run again.
4. In-game texts on the Bridge and MFG Unlock pages no longer mention recovery menu options.
5. Endfield itself runs as administrator, so in-game buttons that need administrator rights, such as the Windows VRAM budget, normally take effect without a separate UAC prompt. This is expected.

此前版本（unified1u～1x）的改动：


1. 界面文字：14 种语言里三条显存提示的译文曾变成问号乱码，现已修正；构建前自动检查所有语言文件，防止再次出现。
2. 崩溃处理器被其他模块反复替换时，Bridge 重新接管后仍会把崩溃交还给每一个原先的处理器（含游戏自带的崩溃报告），不再丢失其中任何一个。
3. 完全卸载（撤除 ReShade 和 NR）现在也会还原 Windows 显存预算设置。
4. 显存预算原值除游戏目录和 LocalAppData 外，另存一份到 `%ProgramData%\dlss5-bridge\vram-budget`。游戏目录被移动或清理后仍能还原；有多份备份时使用本机最新的一份。
5. NR 运行时路径查询接口在初始化失败时返回明确的失败码，而不是“稍后再试”。
6. （unified1w）Bridge 转交崩溃前会确认下一个崩溃处理器所在的模块仍在内存中、且仍是登记时的同一个模块；某个处理器所在的插件已被卸载时直接跳过它，崩溃仍会交给其余处理器和游戏自带的崩溃报告，不会因调用已卸载的代码而二次崩溃。
7. （unified1w）恢复助手的结果提示更准确：显存预算和驱动设置都没恢复成功时两条都会提示（新增结果码 8）；“游戏仍在运行”“游戏目录不对”和未知结果也分别给出对应提示，不再一律显示为驱动恢复失败。
8. （unified1x）第 6 条的模块检查不会因读取模块头部而出错：某些加壳或保护程序会把模块头设为不可读或抹掉，Bridge 此时不读头部、只核对模块地址，不会自身崩溃，也不会因此跳过游戏自带的崩溃报告。

1. UI text: three VRAM notice translations had turned into question marks in all 14 languages; they are fixed, and every language file is checked automatically before a build.
2. When other modules replace the crash handler repeatedly, the Bridge still hands each crash to every earlier handler (including the game's own crash reporter) after taking over again; none is lost.
3. The full uninstall (remove ReShade and NR) now also restores the Windows VRAM budget settings.
4. The original VRAM budget values are also kept in `%ProgramData%\dlss5-bridge\vram-budget`, besides the game folder and LocalAppData, so they survive a moved or cleaned game folder; the newest backup of this PC is used.
5. The NR runtime path query returns a definite failure when initialization failed, instead of "try again later".
6. (unified1w) Before passing a crash on, the Bridge checks that the next crash handler's module is still loaded and is the same module that was registered; a handler whose add-on was unloaded is skipped, and the crash still reaches the remaining handlers and the game's own crash reporter instead of crashing again in unloaded code.
7. (unified1w) The recovery helper result is reported precisely: when both the VRAM budget and the driver setting could not be restored, both warnings are shown (new result code 8); "game still running", "wrong game folder" and unknown results each get their own message instead of all being shown as a driver restore failure.
8. (unified1x) The module check in item 6 cannot fail on module headers: some packers or protectors make a module's headers unreadable or erase them; the Bridge then compares only the module address, does not crash itself and does not skip the game's own crash reporter for that reason.

unified1t 在 unified1s 的基础上合入以下改动（本版保留）。此前只在单台测试机试装过的“子弹时间修复”（pageable2）也是第一次进入统一部署包。

1. 崩溃回退与安全模式更可靠：正常退出时写“已正常退出”标记；已确认的配置，未到启动健康点就正常退出的那一局不计失败。试用新选择期间，以及游戏或驱动更新后的那一局，未到健康点的正常退出仍计失败，因为游戏自带的崩溃报告程序可能让崩溃以正常退出结束；回退到游戏目录后失败计数清零，重新获得完整尝试次数；Bridge 检测到崩溃处理器被其他模块替换时会重新接管（启动健康点之前，每局最多 8 次）；状态文字分开显示“已回退到游戏目录”和“已进入安全模式”；安全模式下 MFG Unlock 页不再整页消失，而是显示原因和解除方法。
2. 修复偶发“未安装 NR 运行时”：NR 运行库路径在显卡识别完成前查询时，不再被一次性记为“缺失”，而是每 250 ms 重试，最长 60 秒。
3. 显存提示：超出 Windows 预算须持续 10 秒才提示，降到 95% 以下并持续 8 秒才消失，避免在临界值附近反复闪；“显存已溢出到系统内存”与“显存用量超出 Windows 预算”分成两条文字，并提示可试用 DLSS5Bridge 页的 Windows 显存预算选项；用量达到预算 95% 只写日志，不弹提示。
4. ReShade 只加载已启用的效果：首次运行时把 `ReShade.ini` 的 `[GENERAL] SkipLoadingDisabledEffects` 设为 1，未启用的着色器不再编译、不再占用显存。每份 `ReShade.ini` 只设置一次（标记 `[DLSS5Bridge] SkipLoadingDisabledEffectsApplied=1`）；之后在 ReShade 里关掉它，插件不会再改回。
5. DLSS5Bridge 页新增“Windows 显存预算”选项：把 Windows 为每个进程预留的显存从约 768 MiB 降到 128 MiB（进程可用预算约多 640 MiB）。需要管理员授权，重启电脑后生效，影响本机所有程序；原值会备份（游戏状态目录、LocalAppData、ProgramData 各一份），可随时在面板还原，完全卸载也会一并还原。对卡顿的改善尚未实测。
6. 切换 NR 分辨率档位后，在操作的控件旁显示 20 秒“切换档位时可能出现暂时卡顿”。
7. 子弹时间修复（pageable2）：删除会自动关开 NR 的显存降级自愈；插件自身资源设为最低显存驻留优先级，显存紧张时先挤插件数据而不是游戏数据；显存降级只提示、不改设置；双层 NR 改档时两层同时生效。另含改档后“慢段”的日志统计，以及默认关闭的 ini 选项 `NRVkRetiredMemoryPriority`。
8. MFG Unlock 的 Legacy 软件呈现节奏改为所有显卡默认开启；升级时一次性把已保存的关闭值改为开启，之后尊重玩家的修改。

1. Crash rollback and safe mode are more reliable. An orderly exit writes a clean-exit mark; for a confirmed configuration, an orderly exit before the startup health point does not count as a failure. During a trial of a new selection, and in the first session after a game or driver update, it still counts, because a crash handled by the game's own crash reporter can end in an orderly exit. Rolling back to the game directory resets the failure count. The Bridge re-takes the crash handler when another module replaces it (before the startup health point, at most 8 times per session). Status text distinguishes rollback from safe mode. In safe mode the MFG Unlock page stays visible and explains the reason and how to leave safe mode.
2. Fixes the intermittent "NR runtime not installed": a runtime-path query made before the GPU is identified is retried every 250 ms for up to 60 seconds instead of being latched as missing.
3. VRAM notices: over the Windows budget must persist 10 seconds to show and clear below 95% for 8 seconds, so values near the limit no longer flicker. Spilling into system memory and exceeding the budget have separate texts, and both point to the Windows VRAM budget option on the DLSS5Bridge page. Usage at 95% of the budget is logged only.
4. ReShade loads only enabled effects: on first run the Bridge sets `[GENERAL] SkipLoadingDisabledEffects=1` in `ReShade.ini`, so disabled shaders are not compiled and do not hold VRAM. This happens once per `ReShade.ini` (marker `[DLSS5Bridge] SkipLoadingDisabledEffectsApplied=1`); turning it off later in ReShade is respected.
5. New Windows VRAM budget option on the DLSS5Bridge page: it lowers the per-process reserve Windows keeps from about 768 MiB to 128 MiB (about 640 MiB more budget). It needs administrator approval, takes effect after a Windows restart and affects every program. Original values are backed up (game state folder, LocalAppData and ProgramData) and can be restored from the panel; the full uninstall restores them too. The effect on stutter has not been measured.
6. After switching the NR resolution tier, a 20-second "temporary stutter possible" hint appears next to the control used.
7. Bullet-time fix (pageable2): the VRAM demotion self-heal that toggled NR is removed; the add-on's own resources get the lowest residency priority so that pressure evicts add-on data before game data; demotion is reported, not acted on; dual-layer NR applies tier changes to both layers at once. Includes post-switch slow-phase logging and the off-by-default ini key `NRVkRetiredMemoryPriority`.
8. MFG Unlock's Legacy software flip pacing now defaults to on for every GPU. An upgrade changes a saved off value to on once; later choices are respected.

下文说明安装条件、配置保留、日志和恢复方式。包内不附带个人配置；请在游戏退出时完整复制，并保留已有配置。构建与离线回归通过不等于所有硬件、驱动、覆盖层组合均完成实机验收。

The following sections describe installation requirements, configuration preservation, logs and recovery. Personal configuration files are not included. Copy the complete package while the game is closed and preserve existing settings. Successful builds and offline regression checks do not constitute in-game acceptance of every hardware, driver or overlay combination.

## 日志位置与异常日志 / Log location and anomaly log

本套件产生的全部日志都在游戏目录的 `dlss5\dlss5_log\` 中，不再散落在游戏根目录：`dlss5-bridge.log`（本局主日志；超过 8 MB 后的内容进入 `dlss5-bridge.overflow.log` 与 `.overflow.prev.log` 两个 4 MB 轮换文件）、`dlss5-bridge.prev.log`（上一局）、`dlss5-observe.log` / `.prev.log`（启动观察）、`dlss5-sl.log`（Streamline 自身日志）、`RenoDX-DLSS5-crash.log`（Generic 崩溃记录）、`dlss5-events-*.log`（异常事件日志）、诊断导出 `diagnostics-*`，以及按需开启的 NR 跟踪 CSV。首次启动本版时，旧版本留在游戏根目录的这些日志会被移动到该文件夹（同名时加 `.legacy-<数字>` 后缀，不删除）。`ReShade.log` 由 ReShade 本体写在 `ReShade.ini` 旁边，其位置由 ReShade 决定，本套件不移动它；Generic 与 MFG Unlock 写入 `ReShade.log` 的告警和错误同时也会进入异常事件日志。

Every log this package produces is in `dlss5\dlss5_log\` in the game folder rather than in the game root: `dlss5-bridge.log` (this session's main log; content beyond 8 MB goes to the two 4 MB rotating files `dlss5-bridge.overflow.log` and `.overflow.prev.log`), `dlss5-bridge.prev.log` (previous session), `dlss5-observe.log` / `.prev.log` (startup observer), `dlss5-sl.log` (Streamline's own log), `RenoDX-DLSS5-crash.log` (Generic crash record), `dlss5-events-*.log` (anomaly event log), diagnostics exports `diagnostics-*`, and the optional NR trace CSVs. On the first start of this version, such logs left in the game root by an earlier version are moved into this folder (with a `.legacy-<number>` suffix when the name is taken; nothing is deleted). `ReShade.log` is written by ReShade itself beside `ReShade.ini`; its location is decided by ReShade and this package does not move it. Warnings and errors that Generic and MFG Unlock write to `ReShade.log` also go to the anomaly event log.

异常事件日志汇总三个插件的异常：Bridge 的错误与带故障特征的日志行、Generic 与 MFG Unlock 的告警和错误、MFG Unlock 每一次未应用面板倍率的决策、上一局崩溃记录、两次呈现间隔达到 100 毫秒的卡顿，以及游戏窗口在前台时 2 秒没有呈现的停帧。默认是有界模式：每种异常在本局首次出现时完整记录一行，重复出现的按 10 秒合并成一行“repeat x次数”；每局最多写 16 MB，文件夹内的有界事件日志最多保留 64 MB 和 30 个文件，超出时删除最旧的。

The anomaly event log collects anomalies from all three add-ons: Bridge errors and log lines with failure signatures, warnings and errors from Generic and MFG Unlock, every MFG Unlock decision that leaves the panel multiplier unapplied, the previous session's crash record, hitches where two presents are 100 ms or more apart, and stalls with no present for 2 s while the game window is in the foreground. The default is bounded mode: the first occurrence of each anomaly in a session is written in full, and repeats are merged into one "repeat x count" line per 10 s; a session writes at most 16 MB, and bounded event logs in the folder are kept to 64 MB and 30 files, deleting the oldest beyond that.

需要完整记录时，在 DLSS5 Bridge 页勾选“高级设置”，在最下方勾选“完整异常日志（每次发生都完整记录一行）”。会弹出容量占用警告，勾选“我已了解上述容量占用风险”并点“确认开启”后才生效；关闭时直接取消勾选即可。完整模式下每次异常都写一行、不合并，文件为 `dlss5-events-full-*.log`（64 MB 分段），不会被自动删除，需要手动清理。正常游玩约每小时数 MB；若某个异常每帧发生，按每秒约 140 帧、每行约 250 字节估算约 3 GB/天，多处同时每帧发生时可达约 30 GB/天。日志所在磁盘剩余空间低于 4 GB 时暂停写入（只计数），回到 6 GB 以上后继续。该设置立即生效并在下次启动时保持，保存在 `dlss5\state\event-log-mode.txt`；游戏无法进入时，删除该文件即可回到有界模式。

For complete recording, tick Advanced settings on the DLSS5 Bridge page and, at the bottom, tick "Full anomaly log (one full line per occurrence)". A disk-use warning opens; the mode changes only after ticking "I understand the disk-use risk above" and clicking "Turn on". To turn it off, untick the box. In full mode every anomaly is written as its own line without merging, into `dlss5-events-full-*.log` files in 64 MB parts that are never deleted automatically and must be removed by hand. Normal play writes a few MB per hour; an anomaly that repeats every frame is about 3 GB per day at about 140 frames per second and 250 bytes per line, and about 30 GB per day when several repeat at once. Writing pauses while the log volume has less than 4 GB free (events are only counted) and resumes above 6 GB. The setting applies immediately, persists across launches and is stored in `dlss5\state\event-log-mode.txt`; if the game cannot be entered, deleting that file returns to bounded mode.

当前覆盖范围的限制：Generic 中部分自带退避或次数上限的告警点只在其实际打印时被记录，其总发生次数见 Generic 每 30 秒一次的统计行；游戏绕过 MFG Unlock 钩子的调用不经过其决策记录；Streamline 自身的告警只在 `dlss5-sl.log` 中。

Current coverage limits: some Generic warning sites that apply their own backoff or count limit are recorded only when they actually print, and their total counts appear in Generic's 30-second statistics lines; calls by the game that bypass MFG Unlock's hooks do not pass through its decision record; Streamline's own warnings are only in `dlss5-sl.log`.

本包修复了游戏启动阶段可能直接崩溃、没有窗口、进程挂在后台的问题。原因是：ReShade 在 Vulkan 下每销毁一个实例就卸载一次插件，终末地启动时会反复创建和销毁实例，而 MFG Unlock 是唯一真正被卸载的插件；它卸载时会撤掉 `kernel32` 里的 `LoadLibraryExW` 等钩子，但若另一个线程（游戏的 Direct3D 9 探测线程）此刻已经进入钩子、正在排队等待加载器锁，等它返回时插件映像已被移除，进程即在原插件地址处崩溃。是否触发取决于启动时序，磁盘较慢或刚复制完文件时可能更容易出现，与显卡代际无关。现在 MFG Unlock 在安装第一个钩子之前就把自身（以及 ReShade 模块）固定到进程结束，与 Bridge、Generic 一样常驻；后续每次 ReShade 重新加载插件时，原本由重新加载完成的模块扫描改为在 `AddonInit` 中重做。修复由离线夹具验证：同一时序下旧版每次都在同一偏移崩溃，新版每次都正常返回；六轮加载/卸载循环中钩子只安装一次、补扫五次、钩子持续有效。**尚未在游戏内和 30/40 系上验证。** 若启动后没有窗口而任务管理器里仍有 `Endfield.exe`，先结束该进程再重新启动，并保留游戏目录中的 `ReShade.log` 和 `dlss5\dlss5_log\` 文件夹供诊断。

This package fixes a failure in which the game could crash during startup, showing no window and leaving the process in the background. ReShade on Vulkan unloads its add-ons each time a Vulkan instance is destroyed, Endfield creates and destroys several instances during startup, and MFG Unlock was the only add-on that was really unloaded. Unloading removed its `kernel32` detours such as `LoadLibraryExW`; if another thread (the game's Direct3D 9 probe thread) had already entered a detour and was queued on the loader lock, the add-on image was gone by the time that thread returned, and the process crashed at the former add-on address. Whether this happens depends on startup timing; it may be more likely on a slow disk or right after the files were copied, and it does not depend on the GPU generation. MFG Unlock now pins itself (and the ReShade module) for the process lifetime before installing its first hook, staying resident like the Bridge and Generic add-ons; on each later ReShade add-on reload, the module scan that a reload used to perform is repeated in `AddonInit`. The fix is verified by an offline fixture: under the same timing the previous build crashed at the same offset on every run and the new build returned normally on every run; across six load/unload cycles the hooks were installed once, rescanned five times and stayed effective. **It is not yet verified in game or on RTX 30/40 series.** If the game shows no window after launch while `Endfield.exe` remains in Task Manager, end that process before starting again, and preserve `ReShade.log` and the `dlss5\dlss5_log\` folder from the game folder for diagnosis.

## 安装和选择 / Installation and selection

Throttle 默认值已内置为2；没有 `vk-present-adapter.ini` 或缺少 `Throttle` 键时自动使用，无需创建文件或手动选择。已有 INI 中明确保存的 Throttle 值继续优先，不被更新覆盖。该默认值用于放宽额外 CPU 节流的等待门槛，同时保留积压时的有界等待；实际表现仍需当前候选的跨显卡与高负载验证。

The built-in Throttle default is 2. It applies automatically when `vk-present-adapter.ini` or its `Throttle` key is absent; no file creation or manual choice is required. An explicitly saved Throttle value in an existing INI retains precedence and is not overwritten by an update. This default relaxes the extra CPU throttle's wait threshold while retaining bounded waits under backlog; actual behavior still requires cross-GPU and high-load validation of this candidate.

本包的 MFG Unlock 保留原有的 64KB 内存块挂钩，并修复了“倍率整局不接管”的两个成因。其一，挂钩需要登记进程内全部其他线程，若某个线程恰好在枚举与登记之间结束，登记必然失败并中止整个挂钩事务；这类线程不可能停在待改写的指令上，因此现在会复查其退出状态并跳过，而仍在运行、只是无法挂起的线程依旧中止事务，原有保护不变。其二，游戏只在第一次询问时取一次倍率入口指针并长期缓存，一次瞬时失败就整局不再重试；现在只要已从插件模块解析出任何一个 DLSS-G 入口，就会提前装好倍率入口，此外每帧的呈现路径还带有限的补装重试与退避。两处修复均通过离线夹具验证（已退出线程跳过、存活不可挂起线程仍中止、重试有界且成功后自停），**尚未在游戏内验证**，30/40 系的实机确认同样未完成。若面板切倍率仍没有实际生效，退出或重启前保留 `ReShade.log` 和 `dlss5\dlss5_log\` 文件夹供诊断。

This package's MFG Unlock keeps the original 64 KB hook allocation and fixes the two causes of a multiplier that never takes over for the whole session. First, hook installation registers every other thread in the process; if a thread ends between enumeration and registration the registration necessarily fails and aborts the whole hook transaction. Such a thread cannot be parked inside the bytes being rewritten, so its exit is now re-checked and it is skipped, while a thread that is still running but cannot be suspended still aborts the transaction -- the original protection is unchanged. Second, the game asks for the multiplier entry point once and caches the pointer for the rest of the session, so a single transient failure was never retried; the entry is now installed ahead of that lookup as soon as any DLSS-G entry has been resolved from the plugin module, and the per-frame present path additionally retries a failed install with a bounded attempt count and backoff. Both changes are verified by offline fixtures (an exited thread is skipped, a live unsuspendable thread still aborts, and the retry is bounded and self-disarming), but **they are not yet verified in game**, and in-game confirmation on RTX 30/40 series is also outstanding. If changing the multiplier in the panel still has no effect, preserve `ReShade.log` and the `dlss5\dlss5_log\` folder before exiting or restarting for diagnosis.

本包修复了呈现阶段 NR 在高层数负载后停用、且整局不恢复的问题。修复前的等待链把事件的一次唤醒当成 GPU 停滞：超时返回时留在 fence 上的旧注册会在稍后置位事件，使下一次等待瞬间得到“已完成”的假信号，于是一次本可在预算内完成的等待被判为失败，NR 进入拒绝状态直到重启。修复后，等待只把事件当作唤醒提醒，完成与否一律以 fence 的完成值为准，并在调用方原有的预算、守护门限与拒绝策略不变的前提下重试。修复以本机真实层数切换（1→4→2→3→1）验证：该局 11 次等待真实跑满 900 毫秒全部正确恢复，未出现一次“4 秒未完成”，NR 全程持续出帧。**跨显卡验收尚未完成**；Vulkan 侧与 relay 恢复路径上同类的“单次唤醒即返回”写法尚未逐一核对，因此不能宣称该类缺陷已全部消除。

This package fixes Present-stage NR stopping after a high-layer load and never recovering within the session. Before the fix, the wait chain treated a single event wake as a GPU stall: the registration left on the fence by a timed-out wait signalled the auto-reset event later, so the next wait received an immediate false "completed" signal, a wait that could have finished within budget was judged failed, and NR entered a refusal state until restart. The fix treats the event purely as a wake-up and establishes completion solely from the fence's completed value, retrying within the caller's unchanged budget, guard threshold and refusal policy. It was verified on the development machine by replaying real layer changes (1→4→2→3→1): all 11 waits that genuinely ran the full 900 ms recovered correctly, no "4 s incomplete" was reported, and NR presented continuously. **Cross-GPU acceptance is not complete**, and other waits of the same single-wake form on the Vulkan side and the relay recovery path have not been individually reviewed, so this class of defect cannot be called fully eliminated.

新配置各层默认开启“角色遮罩”，“角色／皮肤”设为0；“独立皮肤”继续默认开启。这些值用于减轻角色面部异常，不代表所有场景都不会出现伪影。缺失相应配置键时使用默认值；已有配置中明确保存的值保留，包括遮罩关闭和皮肤-1。手动执行“重置为默认值”时采用本版默认值。安装包不附带或覆盖个人ReShade.ini。

New configurations enable Character mask and set Character / skin to 0 for every layer; Independent skin remains enabled by default. These values aim to reduce facial artifacts and do not guarantee artifact-free output in every scene. Defaults apply when the corresponding configuration keys are absent; explicitly saved values are preserved, including a disabled mask or skin value of -1. A manual reset to defaults uses this version's defaults. The package does not include or overwrite a personal ReShade.ini.

退出游戏后，先备份将覆盖的现有插件、运行库及配置，并核对备份 SHA256；再将套件完整放到含 `Endfield.exe` 的游戏目录，保持 `dlss5` 文件夹结构。现有 `ReShade.ini`、`dlss5-bridge.cfg`、`vk-present-adapter.ini` 应保留。从更早版本升级时，请手动删除旧的 `dlss5/nr/common-310.8/` 目录：本版把 NR 运行库改放在 `dlss5/nr/rtx50/` 与 `dlss5/nr/rtx20-40/`，旧目录不再被读取，留着只会多占约 166 MB。删除它不影响任何功能，插件只加载按显卡选中的那一份。套件不包含个人配置；无配置首装使用插件内置默认值，呈现通路默认使用 GPU relay（`Pipeline=2`、`Import=1`、`Throttle=2`）。已有 bridge 配置须保留 `# dlss5-bridge keep` 首行。先使用支持插件的 ReShade 安装器为 `Endfield.exe` 安装 Vulkan 支持；复制套件不能代替这一步。不再使用 `EndfieldFGSwitch.exe`。

Exit the game, back up the existing add-ons, runtime files and settings that will be overwritten, and verify the backup SHA256 hashes. Then place the complete package beside `Endfield.exe`, keeping the `dlss5` directory structure. Preserve existing `ReShade.ini`, `dlss5-bridge.cfg` and `vk-present-adapter.ini`. When upgrading from an earlier version, delete the old `dlss5/nr/common-310.8/` directory by hand: this version keeps the NR runtimes under `dlss5/nr/rtx50/` and `dlss5/nr/rtx20-40/`, nothing reads the old path any more, and leaving it only costs about 166 MB. Removing it changes no behaviour -- the plugin loads only the one file selected for this GPU. Personal configuration files are not included; a fresh installation uses built-in defaults, including GPU relay presentation (`Pipeline=2`, `Import=1`, `Throttle=2`). Preserve the `# dlss5-bridge keep` first line in an existing bridge configuration. First install Vulkan support for `Endfield.exe` with an add-on capable ReShade installer; copying the package does not replace this step. `EndfieldFGSwitch.exe` is no longer used.

启动游戏，在游戏设置中启用 DLSS 超分或 DLAA。进入开放场景后按 Home，在 DLSS5 Bridge 的“统一部署”区点击游戏目录或 `2.14.1 + DLSS-G 310.9.1` 即保存并同步驱动，无二次确认；等待维护结果后正常退出并重启。30/40 系使用兼容 MFG 时选择套件的 2.14.1 组合。无需事先手动恢复驱动配置。版本选择与帧生成开关、倍率无关，可随时选择另一个版本并重启，也可撤销尚未生效的版本更改。首次迁移保留 cfg 已有的副目录选择。只有游戏的 `sl.interposer.dll` 属于已验证构建（目前是更新前后两种 Streamline 2.10.3 构建），且套件的全部 11 个运行文件哈希匹配时，才允许副目录选择；不满足时保留已有选择，并分别提示是哪一项不满足。游戏目录若被旧工具改过，使用的是这些现有文件；恢复原装文件需用启动器校验修复。

Start the game and enable DLSS super resolution or DLAA in its settings. After entering an open scene, press Home and click the game directory or `2.14.1 + DLSS-G 310.9.1` in DLSS5 Bridge's Deployment section. Clicking saves the selection and synchronizes driver settings without a second confirmation; wait for maintenance, exit normally, and restart. Select the packaged 2.14.1 stack when using compatibility MFG on RTX 30/40. No manual driver reset is required beforehand. Version selection is independent of frame generation and its multiplier; either version can be selected again with a restart, and a pending version change can be canceled. Initial migration preserves an existing subdirectory selection in the cfg. Redirection requires the game's `sl.interposer.dll` to be a verified build (currently the two Streamline 2.10.3 builds from before and after the update) and matching hashes for all 11 packaged runtime files; otherwise the existing selection is kept and the panel names which condition failed. If older tools changed that directory, those existing files are used; use launcher verification/repair to restore original files.

“统一部署”主区域提供 `EnableHooks`：0 停用 NR 钩子；1 允许 NGX 与 Streamline 钩子；2 仅 NGX，是 Generic 的缺省值。**本候选的呈现阶段 NR 使用模式 1，首次安装含呈现层的组合请在此选择 1。渲染/超分后阶段的后台保持与模式 1、2 均可配合，仍需焦点钩子成功安装。** 点击即通过 Generic 的 ReShade 配置接口提交；正常退出并重启后生效。本局不会即时安装或拆除钩子，SL 版本与钩子模式互不代改。模式 0 可以从同一面板再次改为 1 或 2。此功能需要同包 Bridge 与 Generic；缺失或旧版 Generic 时显示不可用提示，不直接改写 INI。模式 2 的完整呈现链尚未验收；模式 1 允许额外钩子，不保证对所有游戏或未来运行库版本无兼容影响。

The main Deployment section exposes `EnableHooks`: 0 disables NR hooks; 1 permits NGX and Streamline hooks; 2 uses NGX only and is Generic's default. **Present-stage NR in this candidate uses mode 1; select it here on first installation when using Present layers. Background retention for Render/Upscaled stages supports modes 1 and 2 and still requires a successfully installed focus hook.** A click submits the choice through Generic's ReShade configuration API; exit normally and restart to apply. Hooks are not installed or removed immediately, and SL selection and hook mode do not overwrite each other. Mode 0 can be changed back to 1 or 2 from the same panel. Matching Bridge and Generic builds are required; a missing or older Generic displays an unavailable message instead of directly editing the INI. The complete Present path in mode 2 has not been accepted; mode 1 permits additional hooks and does not guarantee compatibility with every game or future runtime version.

NR 按实际 D3D12 设备的 LUID 匹配 NVIDIA 物理 GPU，再按该设备的架构从随包运行库中自动选择一份，用户不需要选择运行库，也不需要按代际下载不同文件。RTX 50（Blackwell）使用 `dlss5/nr/rtx50/nvngx_dlssnr.dll`：它是 NVIDIA 签名的 310.8.0.0，只含 sm_120，在 RTX 5090 上已实机验证。RTX 20/30/40（Turing/Ampere/Ada）使用 `dlss5/nr/rtx20-40/nvngx_dlssnr.dll`：它的 fatbin 含 sm_75、sm_86、sm_89 与 sm_120，主机侧最低架构判定也同步放宽，因此一份文件覆盖三代，无需按代际分包。该文件不是 NVIDIA 签名件，Generic 的诊断只把它标记为具名非参考构建，不影响加载。无法匹配的设备和跨架构链接节点不使用其他 GPU 的运行库；两份文件都不存在或哈希不符时本次暂停 NR，而不改用未校验的文件。面板高级设置显示本次实际选中的运行库与校验结果。RTX 20/30 默认提前初始化 CUDA；RTX 40/50 默认不提前，高级设置允许更改并在重启后试用。

NR matches the actual D3D12 device LUID to a physical NVIDIA GPU, then selects one of the packaged runtimes by that device's architecture. The user never chooses a runtime and does not download a per-generation file set. RTX 50 (Blackwell) uses `dlss5/nr/rtx50/nvngx_dlssnr.dll`: the NVIDIA-signed 310.8.0.0 build, sm_120 only, verified in game on the RTX 5090. RTX 20/30/40 (Turing/Ampere/Ada) use `dlss5/nr/rtx20-40/nvngx_dlssnr.dll`: its fatbin carries sm_75, sm_86, sm_89 and sm_120, and its host-side minimum-architecture constant is relaxed to match, so one file serves three generations without per-generation packages. That file is not NVIDIA-signed; Generic's diagnostics label it as a named non-reference build, which does not affect loading. Unmatched devices and linked nodes spanning different architectures do not borrow another GPU's mapping; when neither file is present or a hash does not match, NR is suspended for the session rather than falling back to an unverified file. Advanced settings show the runtime actually selected this launch and its verification result. Early CUDA initialization defaults to enabled on RTX 20/30 and disabled on RTX 40/50; advanced settings support a restart trial of a different selection.

成功初始化所选 Streamline 副目录后，bridge 自己的 NGX 会话也把该目录作为优先特性搜索路径，避免另从游戏目录加载不同版本的 DLSS-G。选择游戏目录或未成功重定向时保留 NGX 默认搜索方式；尚未重启的面板选择不会改变本局的搜索路径。游戏原 DLL 不被覆盖。该目录修复已通过离线回归和5090本机重新安装验证，30/40系同包验收尚未完成。

After the selected Streamline subdirectory initializes successfully, the bridge's own NGX sessions also use that directory as their preferred feature search path, avoiding a separate DLSS-G version from the game directory. Selecting the game directory or failing to redirect retains NGX's default search behavior; a panel choice awaiting restart does not change the current session's search path. Original game DLLs are not overwritten. This directory fix has passed offline regression and reinstallation checks on the RTX 5090 development machine; same-package acceptance on RTX 30 and 40 series remains pending.

MFG Unlock 独立控制倍率，默认页选择“由游戏决定”或 2×至 6×。在游戏内开启帧生成后才会应用倍率。Legacy software flip pacing 位于高级设置，所有显卡默认开启；从旧版升级时，已保存的关闭值会被一次性改为开启（`[RenoDX.MFGUnlock] LegacyPacingDefaultRevision=1` 记录已执行），之后的手动选择继续保留。Blackwell 保留原生 NGX 内核。Legacy 更改须重启，bridge 不写入 MFG 配置。

MFG Unlock independently controls the multiplier, offering Game default or 2x through 6x on the basic panel. Multipliers apply only when frame generation is enabled in the game. Legacy software flip pacing is in advanced settings. It defaults to on for every GPU. When upgrading, a saved off value is changed to on once (`[RenoDX.MFGUnlock] LegacyPacingDefaultRevision=1` records this); later manual choices are retained. Blackwell preserves native NGX kernels. Changing Legacy requires a restart, and the bridge does not write MFG settings.

插件在启动时自动检测并同步 8 个公开 DLSS 帧生成配置项，包括固定／动态倍率、目标帧率及 FG 模型覆盖，与设置来自哪个工具无关。需要时在 Endfield 配置内写入默认值以屏蔽游戏局部或继承的全局覆盖；全局配置和其他游戏不变，原局部值或缺省状态保存到双份恢复记录。更改 SL 选择时也会同步，高级设置提供单独同步和还原。权限不足时，维护进程请求同一 Windows 账户的 UAC 授权。取消授权、读取或写入失败时保留记录并允许重试，不阻止游戏启动。同步成功后重启游戏，已加载的运行库未必实时重读驱动值。还原保留其他工具后来写入的不同值。损坏或同代次冲突的记录保留并暂停写入。未公开、语义未知的私有驱动项不进行猜测性改写；SR/RR 等仅作诊断读取。

At startup, the plugin automatically detects and synchronizes eight public DLSS frame-generation settings, including fixed/dynamic multipliers, target frame rate and FG model overrides, regardless of which tool set them. Where needed, it writes defaults in the Endfield profile to shadow local or inherited global overrides. Global settings and other games remain unchanged, while prior local values or absence are saved in two recovery journals. Changing the SL selection also synchronizes; advanced settings provide separate synchronization and restoration. Insufficient privileges trigger a maintenance-process UAC request for the same Windows account. Canceled authorization and read/write failures preserve journals and allow retry without blocking game startup. Restart after successful synchronization because loaded runtimes may not reread driver values immediately. Restore preserves differing values written later by other tools. Damaged or contradictory journals of the same generation are retained and stop writes. Undocumented private settings with unknown semantics are not modified speculatively; SR/RR and other settings are read for diagnostics only.

三个插件的语言选择均位于面板顶部，可手动切换，正文每次显示一种语言。默认隐藏高级设置；DLSS5 默认保留层数、分辨率与画面效果，MFG 保留启用开关和倍率，Bridge 保留 SL 选择、EnableHooks 模式和 Windows 显存预算开关。勾选高级设置才显示兼容策略、诊断参数、驱动配置、安全模式与诊断导出（处于安全模式时，安全模式与诊断导出不勾选也会显示）；隐藏操作不重置已保存的参数。原始诊断中的 API、配置键和日志文本可能仍使用英文。

Language selection is at the top of all three panels and can be changed manually; panel text displays one language at a time. Advanced settings are hidden by default. DLSS5 retains layers, resolution and image controls; MFG retains enable and multiplier controls; Bridge retains SL selection, EnableHooks mode and the Windows VRAM budget switch. Compatibility policies, diagnostics, driver settings, safe mode and diagnostics export appear when advanced settings are enabled (safe mode and diagnostics export also appear while safe mode is on). Hiding them does not reset saved values. Raw diagnostics may retain English API names, configuration keys and log text.

## 层数确认与启动恢复 / Layer confirmation and startup recovery

Generic 的每层参数按阶段和全局层号分隔。切换到新的总层数 ≥5 组合时，先保存回退记录，再在 DLSS5 页面顶部给出警示和 15 秒确认；前 2 秒不能保留。确认控件属于 DLSS5 页面，不出现在 MFG Unlock 页面。关闭菜单后计时仍有效；每次 NR 调用读取层计划时会拒绝过期组合。GPU 完全挂起时无法强行中断驱动，重新启动后仍可回到未确认试用前的组合。

Generic separates layer controls by stage and global layer number. A new combination totaling at least five layers saves its rollback record before displaying a warning and a 15-second confirmation period at the top of the DLSS5 page. Keep is unavailable for the first two seconds. The confirmation controls belong to the DLSS5 page and do not appear on the MFG Unlock page. Closing the overlay does not suspend the deadline; NR plan reads reject an expired combination. A fully stalled GPU driver cannot be forcibly interrupted by this timer; restarting still restores the combination preceding the unconfirmed trial.

部署状态在 `dlss5/state`，不可写时使用 `%LOCALAPPDATA%/dlss5-bridge/<目录标识>`。启动健康点要求持续出帧 60 秒；选择副目录时还要求重定向成功、必要 SL 模块来自选定目录且游戏 DLSS 已发生评估。试用项连续 1 次已记录崩溃或 2 次未明原因中断会回退；已确认配置连续 2 次崩溃或 3 次未明原因中断进入安全模式。到达健康点后再强退不计失败；已确认的配置正常退出也不计失败，但试用期间及游戏或驱动更新后的那一局，未到健康点的正常退出仍计失败。回退到游戏目录后失败计数清零。安全模式必须手动退出（Bridge 页），临时停用不清除层数参数。安全模式下 MFG Unlock 页只显示原因和解除方法。

Deployment state is stored in `dlss5/state`, with `%LOCALAPPDATA%/dlss5-bridge/<directory ID>` as fallback. Startup health requires 60 seconds of continuous presents. Subdirectory mode additionally requires successful redirection, required SL modules from the selected directory and an observed game DLSS evaluation. A trial rolls back after one recorded crash or two unexplained interrupted startups. A confirmed configuration enters safe mode after two crashes or three unexplained interrupted startups. Termination after health confirmation does not count as failure, and neither does an orderly exit of a confirmed configuration; during a trial and after a game or driver update, an orderly exit before the health point still counts. Rolling back to the game directory resets the failure count. Safe mode requires manual exit (Bridge page) and does not clear layer parameters. In safe mode the MFG Unlock page shows only the reason and how to leave.

## 游戏无法启动 / When the game cannot start

**游戏或驱动大更新使 ReShade 自身失效时，使用游戏外撤除流程。游戏内安全模式不能替代该流程。** 游戏无需能够启动，ReShade 与三个插件也无需能够加载。

**If a game or driver update breaks ReShade itself, use the external removal procedure. In-game safe mode does not replace it.** Neither the game nor ReShade or the three add-ons needs to load successfully.

1. 完全退出游戏，双击游戏根目录 `DLSS5-Recovery 故障恢复.cmd`。窗口说明将要做的事，输入 **Y** 回车确认（直接回车或其他输入表示取消，不做任何改动），然后允许同一 Windows 账户的管理员授权。该路径只执行 Windows PowerShell 和独立的 `DLSS5-RestoreDriver.exe`，不执行 bridge 或 ReShade。如果入口缺失，须从本套件重新复制 `DLSS5-Recovery 故障恢复.cmd`、`DLSS5-Clean-Recovery.ps1`、`DLSS5-RestoreDriver.exe` 三个文件；无需访问 GitHub。
2. 工具备份 ReShade Vulkan 注册值，将已启用的 ReShade 层设为停用，使 Vulkan 加载器不再加载它。随后按 SHA256 备份、归档三个插件、NR 运行目录及相关配置。备份放在游戏目录旁的 `DLSS5-Recovery-Backups/<时间-随机标识>/`，原文件不会被删除。系统 ReShade DLL 留在原位置，但已登记的加载入口被停用。若 ReShade 同时服务其他 Vulkan 游戏，工具会先暂停撤除并列出共用应用，窗口再询问是否继续：输入 Y 表示接受这些游戏也暂时停用 ReShade，输入 N 则不做任何改动。已不存在的程序路径（例如游戏换过安装目录后留下的旧路径）不算共用，会被忽略。也可以用下方共用参数直接执行。
3. 工具使用独立程序尝试还原本套件有记录的驱动改动；用过“Windows 显存预算”选项的，同时还原显存预算原值（重启电脑后生效）。读取或恢复失败仍会继续撤除渲染组件，并在结果中标记驱动恢复未完成；记录保留在备份及 LocalAppData，不能据此宣布全部恢复成功。此时窗口提示“还原未完成”（不会显示“卸载完成”），卸载工具留在游戏目录，按提示处理后再双击一次即可重试。全部还原成功后，恢复助手留下的状态记录移入该备份的 `leftover` 子文件夹，空的 `dlss5`、`reshade-shaders` 文件夹被删除，卸载工具自身也移入 `tools` 子文件夹；个别文件被占用无法移走时窗口会列出，可手动删除。
4. 在官方启动器执行当前游戏版本的**校验／修复**。本套件不替换游戏原 DLL，但旧工具可能曾替换根目录的 SL／NGX 文件。撤除工具不会在大更新后用旧 DLL 覆盖新版本，也不会把保留的旧工具文件当成原装。**只有渲染组件撤除、驱动恢复结果检查和官方文件修复均完成后，才进行原版启动验证。**
5. 若启动器修复后仍异常，或不能确定其是否清除了旧工具额外加入的 DLL，保留恢复备份，把现有游戏目录改名保存，再通过官方启动器在新的空目录安装当前版本。将旧存档和账户数据留在原位置，不向新目录复制旧插件、ReShade 配置或 SL／NGX DLL。这是获取可确认纯净游戏目录的兜底方法。原版成功启动后再决定是否重新安装兼容的新 NR 套件。

1. Exit the game and run `DLSS5-Recovery 故障恢复.cmd` from the game directory. It explains what it will do; type **Y** and Enter to continue (Enter alone or any other answer cancels without changes), then authorize elevation using the same Windows account. This path runs only Windows PowerShell and the independent `DLSS5-RestoreDriver.exe`; it does not run the bridge or ReShade. If the entry point is missing, copy `DLSS5-Recovery 故障恢复.cmd`, `DLSS5-Clean-Recovery.ps1` and `DLSS5-RestoreDriver.exe` from this package again; GitHub is not required.
2. The tool records ReShade Vulkan registry values and disables enabled ReShade layer registrations, preventing the Vulkan loader from loading those layers. It then backs up and archives the three add-ons, the NR runtime directory and related settings with SHA256 verification. Backups are stored beside the game directory under `DLSS5-Recovery-Backups/<time-random-id>/`; originals are not deleted. The system ReShade DLLs remain in place with their registered loading paths disabled. If other Vulkan games share ReShade, the tool first pauses removal and lists the shared applications, then the window asks whether to continue. Answer Y to accept that ReShade will also be disabled temporarily for those games, or N to leave everything unchanged. Program paths that no longer exist, such as the old path left behind after the game was moved to another folder, do not count as shared and are ignored. The shared-scope option below runs the same removal directly.
3. An independent executable attempts to restore driver changes recorded by this package and, if the Windows VRAM budget option was used, the original VRAM budget values (restart Windows to apply). Read or restoration failures do not prevent render-component removal; the result marks driver recovery as incomplete. Journals remain in the backup and LocalAppData. Such a result does not establish complete recovery; the window reports an incomplete restore (not a finished uninstall) and the uninstaller stays in the game directory, so fix the reported problem and run it again. After a fully successful restore, the state records left by the restore helper move into the backup's `leftover` folder, the empty `dlss5` and `reshade-shaders` folders are removed, and the uninstaller itself moves into the `tools` folder; files that are in use and cannot be moved are listed and can be deleted by hand.
4. Run **file verification/repair** for the current game version in the official launcher. This package does not replace original game DLLs, but older tools may have replaced root SL/NGX files. The removal tool does not overwrite an updated game with old DLLs or treat retained third-party replacements as original files. **Verify an original-game launch only after removing the render components, reviewing driver recovery, and completing official file repair.**
5. If problems remain after repair, or the launcher cannot be confirmed to remove extra DLLs added by older tools, retain the recovery backup, rename the existing game directory to preserve it, and install the current game version into a new empty directory with the official launcher. Keep existing saves and account data in place. Do not copy old add-ons, ReShade settings or SL/NGX DLLs into the new directory. This is the fallback for obtaining a verifiably clean game directory. Decide whether to reinstall a compatible NR package after the original game starts successfully.

恢复结果在备份的 `result.json`；`render-components-removed; launcher-repair-required` 明确表示还需启动器修复，并非“已恢复纯净原版”。记录中 `driver_exit=0` 表示没有待还原驱动记录或记录的改动已处理；其他值需要查看日志。`leftovers_archived`、`tools_archived` 列出移入 `leftover`、`tools` 的文件，`leftovers_left`、`tools_left` 列出没能移走的文件；`leftover` 中的记录是卸载过程新写的，Undo 不会放回。管理员子进程的输出写入系统临时目录，成功后自动删除；失败时窗口显示该报告的路径。确认游戏正常、也不打算用 Undo 重新启用后，可以删除游戏目录旁的 `DLSS5-Recovery-Backups`。

The backup's `result.json` records the outcome. `render-components-removed; launcher-repair-required` explicitly means launcher repair is still required, not that a clean original installation has been established. `driver_exit=0` means no pending driver journal exists or recorded changes have been resolved; other values require reviewing the log. `leftovers_archived` and `tools_archived` list the files moved into `leftover` and `tools`; `leftovers_left` and `tools_left` list files that could not be moved. The records in `leftover` were written during the uninstall and Undo does not put them back. Output from the elevated child process goes to the system temporary folder and is deleted on success; on failure the window shows its path. Once the game works and Undo is no longer wanted, `DLSS5-Recovery-Backups` beside the game directory may be deleted.

高级操作在游戏目录的 PowerShell 中执行；路径参数支持中文和空格。Plan只读检查，Restore执行撤除，Undo从指定备份重新启用组件。Undo不会覆盖后来修改的文件，也不会重新施加驱动倍率覆盖；大更新导致不兼容时不要执行Undo。完全卸载成功后脚本已移入备份的 `tools` 子文件夹，此时用下面最后一条命令，从该位置运行并用 `-Game` 指明游戏目录；Undo 不会把卸载工具放回游戏目录。

Advanced commands run in PowerShell from the game directory and support paths containing spaces and non-ASCII characters. Plan is read-only, Restore removes the components, and Undo re-enables components from the specified backup. Undo refuses to overwrite subsequently changed files and does not reapply driver multiplier overrides. Do not use Undo when a major update has made those components incompatible. After a successful full uninstall the script is in the backup's `tools` folder; run it from there with `-Game`, as in the last command below. Undo does not put the uninstaller back into the game directory.

```powershell
# Read-only inventory / 只读核查
.\DLSS5-Clean-Recovery.ps1 -Action Plan
# Explicitly accept disabling shared Vulkan ReShade / 明确接受共用 Vulkan ReShade 停用
.\DLSS5-Clean-Recovery.ps1 -Action Restore -AllowSharedReShade
# Restore archived components only when compatible / 仅在兼容时恢复归档组件
.\DLSS5-Clean-Recovery.ps1 -Action Undo -Backup 'C:\完整备份路径'
# After a successful full uninstall / 完全卸载成功后
& 'C:\完整备份路径\tools\DLSS5-Clean-Recovery.ps1' -Action Undo -Backup 'C:\完整备份路径' -Game 'C:\游戏目录'
```

安全模式、驱动设置还原、诊断导出（保存到 `dlss5\dlss5_log\diagnostics-*`）和 Windows 显存预算还原都在游戏内 DLSS5Bridge 页操作（前三项需先勾选“高级设置”），不能代替上面的完全卸载与原版文件修复。游戏无法启动时用完全卸载。需要停用 ReShade 本体时，在 ReShade 安装器选择 Endfield.exe → Vulkan → 卸载。不要删除 `dlss5/state`、LocalAppData 的驱动还原记录或 `%ProgramData%\dlss5-bridge`。

Safe mode, driver setting restore, diagnostics export (into `dlss5\dlss5_log\diagnostics-*`) and Windows VRAM budget restore are on the in-game DLSS5Bridge page (the first three under Advanced settings); they do not replace the full uninstall and original-file repair above. Use the full uninstall when the game cannot start. To uninstall ReShade itself, select Endfield.exe, Vulkan and Uninstall in its installer. Preserve `dlss5/state`, the driver recovery journal in LocalAppData and `%ProgramData%\dlss5-bridge`.

候选验证仍需覆盖：SL 双向切换与实际模块路径、游戏更新与损坏文件回退、关闭菜单后的层数超时、强退与安全模式恢复、驱动同步及外部改动后的还原、跨显卡功能与帧曲线、本版新增的冷启动 NR 运行时重试、显存提示与 Windows 显存预算（应用/重启/还原）、`SkipLoadingDisabledEffects` 一次性设置、Legacy 升级迁移，以及 RTX 2050／3050 Ti Laptop／3050 Laptop 上的显卡名称解锁（本版插件实机运行、DLSS 可用性与显存）。维护导出的模块清单来自导出时的进程；游戏外导出反映维护进程，游戏运行模块应在游戏内导出。

Remaining candidate acceptance covers both SL directions and actual module paths, update/corrupt-file rollback, layer timeout with the overlay closed, interrupted-startup and safe-mode recovery, driver synchronization and restoration after external changes, and cross-GPU functions and pacing, plus this build's cold-start NR runtime retry, VRAM notices and the Windows VRAM budget option (apply, restart, restore), the one-time `SkipLoadingDisabledEffects` setting, the Legacy upgrade migration, and the GPU-name unlock on RTX 2050 / 3050 Ti Laptop / 3050 Laptop (this build's add-on in game, DLSS availability and VRAM). Exported module inventories describe the exporting process; an export outside the game describes the maintenance process. Export in game to capture game modules.
