# Endfield DLSS 5 Vulkan plugin — release unified-20261103l

Release date: 2026-11-05. This release replaces `unified-20261015q` (2026-10-19). The repository was renamed from `dlss5-vulkan-fg-relay` to `endfield-dlss5-vulkan-plugin`; old links are redirected by GitHub. The previous README is kept at [docs/history/README-unified-20261015q.md](https://github.com/Yukikaze20170315/endfield-dlss5-vulkan-plugin/blob/main/docs/history/README-unified-20261015q.md).

发布日期：2026-11-05。本次发布取代 2026-10-19 的 `unified-20261015q`。仓库已由 `dlss5-vulkan-fg-relay` 改名为 `endfield-dlss5-vulkan-plugin`，旧链接会被 GitHub 自动跳转。上一版 README 保留在 [docs/history/README-unified-20261015q.md](https://github.com/Yukikaze20170315/endfield-dlss5-vulkan-plugin/blob/main/docs/history/README-unified-20261015q.md)。

## In short / 简要说明

**What this is.** A ready-to-use package that adds NVIDIA's DLSS 5 neural rendering ("NR", an AI filter that redraws the game's picture with more detailed lighting and materials) to Arknights: Endfield running on Vulkan, together with the game's own DLSS Frame Generation. NVIDIA GeForce RTX 20, 30, 40 and 50 series cards use the same files. You copy one folder into the game folder, change a few settings once, and restart the game.

**这是什么。** 一个开箱即用的插件包，让运行在 Vulkan 下的《明日方舟：终末地》在使用游戏自带 DLSS 帧生成的同时，用上 NVIDIA 的 DLSS 5 神经渲染（NR，一种用 AI 重新绘制画面、让光影和材质更细致的画面滤镜）。NVIDIA GeForce RTX 20、30、40、50 系列显卡使用同一套文件。只需把一个文件夹复制进游戏目录，做一次设置，再重启游戏即可。

**How it works.** The game draws a picture and passes it to its own DLSS Frame Generation, which then creates the extra in-between frames. The add-on runs neural rendering on that finished real picture just before frame generation takes it, so frame generation builds its extra frames from the improved picture and the expensive AI step runs once per real frame instead of once per displayed frame. In this release that step runs directly on the game's own Vulkan device and present queue; the separate Direct3D 12 device and the GPU relay of earlier releases are no longer used, which saved roughly 1.1 to 1.4 GB of video memory on the RTX 4060 Laptop where it was measured.

**怎么做到的。** 游戏先画出一张画面，再交给它自带的 DLSS 帧生成去插出中间的画面。插件在帧生成接手之前，对这张已经画好的真实画面做神经渲染；帧生成随后用这张改进过的画面去生成插帧，最耗性能的 AI 步骤只需每张真实画面做一次，而不是屏幕上每显示一帧都做一次。本版中这一步直接在游戏自己的 Vulkan 设备和呈现队列上运行，不再使用以前版本的独立 Direct3D 12 设备和 GPU 中继，在实测的 RTX 4060 Laptop 上因此约省下 1.1～1.4 GB 显存。

**What is new since unified-20261015q.**

1. **Less video memory, much better on 8 GB cards.** Present-stage NR runs on the game's Vulkan device (see above). NR resources are released before their replacements are created, turning NR off frees its memory, and the add-on's own memory is marked as the first to be moved out when video memory runs short, so the game's own data stays in video memory. A new "Windows VRAM budget" switch can give the game a few hundred MB more on 8 GB cards.
2. **Smoother 3x–6x frame generation with NR on**, because NR no longer delays the generated frames.
3. **RTX 20 series and the RTX 2050 / RTX 3050 Laptop / RTX 3050 Ti Laptop.** RTX 20 is now measured in game. On the three laptop GPUs, the game hides DLSS in Vulkan mode; the add-on makes it available again.
4. **NR resolution scale now also applies to the Upscaled (and Render) stages**, and NR plus frame generation keep running when the game window loses focus.
5. **Fixed:** a crash when opening the NVIDIA overlay's game filter page (Alt+Z); the intermittent "NR runtime not installed"; frame generation switching back and forth on 8 GB cards when Streamline reports a VRAM warning; a rare crash from MFG Unlock installing its hooks twice; crash handlers of other components being lost or called twice; the mouse leaving the game window while the ReShade menu is open; PrintScreen not working as screenshot key; the old recovery menu doing the wrong action on some PCs.
6. **Simpler panels and new defaults:** secondary options are under "Advanced settings", notices sit in a fixed paged area, new first-install defaults (Present 1 layer, Performance resolution), a one-click "Adjust NR's look" switch to compare before/after, and screenshots that save exactly the final image.
7. **Uninstall in one step:** the recovery file now only does a full uninstall after you type Y, also restores the driver settings and the Windows VRAM budget, and leaves no package file in the game folder.

**相比 unified-20261015q 的新内容。**

1. **显存占用更少，8 GB 显卡明显改善。** “呈现”层 NR 改在游戏的 Vulkan 设备上运行（见上）。NR 资源先释放旧的再创建新的；关闭 NR 会释放其显存；显存不够时，插件自己的数据会被优先挪出显存，游戏自己的数据留在显存里。新增“Windows 显存预算”开关，8 GB 显卡可多用几百 MB 显存。
2. **开 NR 时 3～6 倍帧生成更流畅**，因为 NR 不再拖慢生成帧。
3. **RTX 20 系，以及 RTX 2050 / RTX 3050 Laptop / RTX 3050 Ti Laptop。** RTX 20 系已有游戏内实测。游戏在 Vulkan 模式下会对这三款笔记本显卡隐藏 DLSS，插件让它重新可用。
4. **NR 分辨率比例现在也作用于“超分后”（和“渲染”）层**；游戏窗口失去焦点时，NR 和帧生成继续运行。
5. **已修复：** 打开 NVIDIA 覆盖层（Alt+Z）的游戏滤镜页时闪退；偶发“未安装 NR 运行时”；8 GB 显卡上 Streamline 报显存警告时帧生成倍率来回切换；MFG Unlock 重复安装钩子导致的偶发崩溃；其他组件的崩溃处理程序丢失或被调用两次；打开 ReShade 菜单时鼠标跑出游戏窗口；PrintScreen 不能作截图键；旧恢复菜单在部分电脑上执行错误的操作。
6. **面板更简洁、默认值更新：** 次要选项放进“高级设置”，提示统一放在固定大小的翻页提示区；新的首装默认值（呈现 1 层、分辨率“性能”）；一键“调整 NR 的外观”开关方便对比调色前后；截图只保存最终画面。
7. **一步卸载：** 恢复文件现在只做完全卸载，输入 Y 后执行，同时还原驱动设置和 Windows 显存预算，游戏目录中不留本包文件。

**Tested.** The author measured this package in game on RTX 2070, RTX 3070 Ti, RTX 4060 Laptop, RTX 3050 Laptop, RTX 4080 and RTX 5090 (table in "Measured frame rates"). The memory and frame-generation changes were developed and checked in game on an RTX 4060 Laptop (8 GB); every build passed the offline fixtures listed in [BUILDING.md](https://github.com/Yukikaze20170315/endfield-dlss5-vulkan-plugin/blob/main/BUILDING.md). What was not verified is listed in "Limitations".

**测试情况。** 作者用本包在 RTX 2070、RTX 3070 Ti、RTX 4060 Laptop、RTX 3050 Laptop、RTX 4080 和 RTX 5090 上做了游戏内实测（见“实测帧率”中的表格）。显存与帧生成相关的改动是在一台 RTX 4060 Laptop（8 GB）上开发并做游戏内检查的；每个构建都通过了 [BUILDING.md](https://github.com/Yukikaze20170315/endfield-dlss5-vulkan-plugin/blob/main/BUILDING.md) 中列出的离线测试。未验证的部分见“限制”一节。

**Maintenance.** From this release on, this repository is maintained as a standalone project. Changes are published here as complete source and packages; they are no longer prepared as pull requests or merge kits for the projects it is based on (see "Licenses and acknowledgements").

**维护方式。** 从本版起，本仓库作为独立项目维护。改动以完整源码和成品包的形式发布在这里，不再为本项目所基于的各上游项目准备 PR 或合并补丁包（见“许可与致谢”）。

---

# Install guide / 使用教程

This guide assumes that you have never installed ReShade or any add-on for this game. If you already use `unified-20261015q`, read "Updating from unified-20261015q" first.

本教程假设你从未给这个游戏安装过 ReShade 或任何插件。如果你正在用 `unified-20261015q`，请先看“从 unified-20261015q 升级”一节。

## Personal statement from the author / 作者的个人声明

I published this file on 3 November 2026. Up to that date, every case I could find in which a player received a warning or a ban involved a first-person-view add-on called "Enhancer". I have not seen a case in which a warning or ban came only from using injected DLSS 5. That does not mean I can guarantee that this add-on is completely safe. If you value your account very much and do not want to take any risk at all, I do not recommend using this add-on.

我发布这个文件的日期是 2026 年 11 月 3 日。截止目前这个时间点为止，我能够找到的、出现了警告和封号的所有案例中，都只出现在使用了一款叫做“Enhancer”的第一人称视角插件上。我目前还没有见过只开了注入式 DLSS5 导致被警告或封号的案例，但这不代表我能够保证本插件是绝对安全的。如果你非常爱惜你的账号，完全不想冒任何风险，那么我不推荐你使用本插件。

I clearly understand and support the Fair Operation Statement (《公平运营声明》) published by Hypergryph. I promise that this add-on never creates or changes any of the game's gameplay or features and never harms the game's fairness in any way; all of my development is about DLSS 5 and the graphics pipeline.

我明确理解并支持鹰角网络发布的《公平运营声明》，承诺本插件绝不创造、修改任何游戏原有的玩法和功能，绝不破坏游戏的任何公平性，我的一切开发都基于 DLSS5 和图形管线。

I even hope that Hypergryph sees these optimizations and that they help reduce development costs, and I hope that Hypergryph lets more people use an officially fine-tuned DLSS 5 as soon as possible.

我甚至希望鹰角官方能够看到我做的这些优化，并且帮助他们降低研发成本，也希望鹰角官方能够尽早让更多的人都能用上官方精调优化过的 DLSS5。

This statement, my "disclaimer" above, does not mean that I support or oppose any other third-party add-on. Please do not use it to start arguments, compare, or label me. I respect both the official developers and the authors of every third-party add-on at the same time; the two do not conflict.

我的此次声明、也就是上面叠的甲的含义，并不代表我支持或不支持任何其他的第三方插件，请不要因此拉踩引战，也不要给我扣帽子。我同时尊重官方和所有第三方插件的作者，这并不冲突。

## What you need / 需要的条件

**Graphics card / 显卡**

- Supported: NVIDIA GeForce RTX 20, 30, 40 and 50 series.
  确定支持：NVIDIA 20 系、30 系、40 系、50 系。
- Not known yet: NVIDIA RTX Spark, GTX 16 series, Turing RTX 3000–8000, Ampere RTX A500–A6000, Ada RTX 500–6000, Blackwell RTX PRO 500–6000, TITAN RTX, TITAN V.
  尚未确定是否支持：NVIDIA RTX Spark、16 系、Turing RTX 3000～8000、Ampere RTX A500～A6000、Ada RTX 500～6000、Blackwell RTX PRO 500～6000、TITAN RTX、TITAN V。
- Not supported: NVIDIA GTX 10 series, Pascal P400–GP100 and every GPU older than Pascal; all AMD GPUs; all Intel GPUs.
  确定不支持：NVIDIA 10 系、Pascal P400～GP100、Pascal 架构之前的全部 GPU；AMD 全部 GPU；Intel 全部 GPU。

**NVIDIA driver / NVIDIA 驱动：** version 615.00 or newer; Game Ready or Studio both work.
最低 615.00；Game Ready 或 Studio 均可。

**Windows / 系统：** Windows 10 version 2004 (build 19041) or newer, or Windows 11; 64-bit; a genuine, unmodified Windows installation. Some third-party modified Windows builds conflict with this add-on (and some with the game itself); this cannot be fixed in the add-on.
最低 Win10 2004（19041），或 Win11；64 位；正版、未经魔改的 Windows。部分第三方魔改 Windows 会与本插件（有的甚至与未装插件的游戏本身）冲突，无法在插件内解决。

**The game must run with Vulkan.** Do not tick "DirectX 11" in the launcher's game settings; the add-on only works in Vulkan mode.
**游戏必须以 Vulkan 运行。** 不要在启动器的游戏设置里勾选“DirectX11”启动；本插件只在 Vulkan 模式下工作。

## Before you start / 开始之前

- **Background programs.** The add-on adds as little load as it can, but it cannot reduce the load you already have. On a PC with 16 GB of RAM and 8 GB of video memory, many background programs running at the same time will almost certainly cause stutter.
  **后台程序。** 插件只能尽量少增加负载，无法降低你原有的负载。16 GB 内存、8 GB 显存的电脑上同时开着大量后台程序，几乎必然会卡。
- **Programs known to conflict** (found with internal testers). Some keep their system changes even after they are closed, so closing them or restarting the PC is not always enough:
  **已知会冲突的程序**（内测中发现）。其中有些关闭后其系统改动仍然生效，所以只关掉或重启电脑不一定够：
  - DisplayFusion "title bar buttons": stutters the game when the mouse moves, even without this add-on.
    DisplayFusion 的“标题栏按钮”功能：即使不装本插件，也会在鼠标移动时让游戏严重卡顿。
  - DWMBlurGlass and other tools that inject into DWM: occasional heavy stutter, because this add-on's frame pacing goes through DWM.
    DWMBlurGlass 等注入 DWM 的桌面美化工具：偶发严重卡顿，因为本插件的帧节奏经由 DWM 处理。
  - "System optimizer" tools with GPU spoofing, frame scheduling, shader optimisation or driver/runtime replacement.
    带“系统优化”、显卡伪装、帧调度、着色器优化、驱动/运行库替换等功能的工具。
  - RTSS (RivaTuner Statistics Server) frame limit: set it back to 0. The on-screen display can stay on.
    RTSS（RivaTuner Statistics Server）的限帧：请改回 0。OSD 监控可以继续开。
  - Heavy laptop vendor control software; use a lighter alternative where one exists (for example G-Helper instead of Armoury Crate, Lenovo Legion Toolkit instead of Legion Zone).
    臃肿的笔记本厂商控制软件；有轻量替代就用替代（例如用 G-Helper 代替奥创控制中心，用 Lenovo Legion Toolkit 代替 Legion Zone）。
  - A nearly full C: drive can make the NVIDIA App and driver installation fail; free space and do a clean driver install.
    C 盘接近占满会让 NVIDIA App 和驱动安装失败；请腾出空间并清洁安装驱动。
  - Overclocking, undervolting or changed advanced BIOS settings; more than one antivirus program; many always-on-top pop-up windows.
    超频、调电压、改 BIOS 进阶参数；同时装多个杀毒软件；桌面上大量置顶弹窗或浮窗。
- **RTX 5090, RTX 4090 and RTX PRO 6000 Blackwell (including their D variants).** Their power connector is known to be able to overheat during long runs at full power; this is not caused by DLSS 5, but DLSS 5 is a heavy load. Consider lowering the power limit while using this add-on, for example to about 550 W on an RTX 5090.
  **RTX 5090、RTX 4090、RTX PRO 6000 Blackwell（含 D 版）。** 这三款显卡的供电接口在长时间满功耗运行时有熔毁风险；这不是 DLSS 5 导致的，但 DLSS 5 负载较高。使用本插件时建议适当限制功耗，例如把 RTX 5090 的功耗上限设为 550 W 左右。

## Downloads / 下载

1. **ReShade 6.8.0 with full add-on support:** https://reshade.me/downloads/ReShade_Setup_6.8.0_Addon.exe (the same file as the button "Download ReShade 6.8.0 with full add-on support" on https://reshade.me).
   **带完整插件支持的 ReShade 6.8.0：** https://reshade.me/downloads/ReShade_Setup_6.8.0_Addon.exe （与 https://reshade.me 上“Download ReShade 6.8.0 with full add-on support”按钮下载的是同一个文件）。
2. **This package:** on the release page https://github.com/Yukikaze20170315/endfield-dlss5-vulkan-plugin/releases/tag/unified-20261103l, scroll down to "Assets" and click `unified-20261103l-candidate.zip` (about 280 MB). Do not download "Source code"; that is not the add-on. Right-click the downloaded zip file and choose "Extract All"; this creates a folder named `unified-20261103l-candidate`.
   **本插件包：** 打开发布页 https://github.com/Yukikaze20170315/endfield-dlss5-vulkan-plugin/releases/tag/unified-20261103l ，往下找到“Assets”，点击 `unified-20261103l-candidate.zip`（约 280 MB）。不要下载“Source code”，那不是插件。下载后右键这个 zip 文件，选择“全部解压缩”，会得到一个名为 `unified-20261103l-candidate` 的文件夹。

The name contains "candidate" because the package is published exactly as it was built and tested under that name. The package also contains a technical document, `DLSS5-使用与恢复 Usage and Recovery.md`; you do not need it to follow this guide.

文件名里带“candidate”，是因为发布的包与以这个名字构建、测试过的包完全相同。包里还有一份技术文档 `DLSS5-使用与恢复 Usage and Recovery.md`，按本教程操作不需要阅读它。

## Install / 安装

**1.** Run `ReShade_Setup_6.8.0_Addon.exe`. If it first shows a warning about using ReShade with online games, read it and continue.
**1.** 打开 `ReShade_Setup_6.8.0_Addon.exe`。如果先弹出关于在网络游戏中使用 ReShade 的警告，阅读后继续即可。

**2.** Click "Browse...". Go to the Arknights: Endfield game folder. With the default install locations it is usually `C:\Program Files\Hypergryph Launcher\games\Arknights Endfield`. If you do not know where it is: in the Hypergryph launcher, on the Endfield page, click the button with three lines to the right of "开始游戏" (Start game), choose "游戏设置" (game settings); the window shows "游戏安装路径" (installation path). Remember this folder.
**2.** 点击“Browse...”，找到终末地的安装目录。默认安装时通常是 `C:\Program Files\Hypergryph Launcher\games\Arknights Endfield`。不知道在哪时：打开启动器，点击终末地页面“开始游戏”右侧的三道横杠，选择“游戏设置”，页面上的“游戏安装路径”就是这个目录。记住这个目录。

**3.** Select `Endfield.exe` in that folder and click "Open".
**3.** 选中该目录下的 `Endfield.exe`，点“打开”。

**4.** Click "Next". "Downloading compatibility information ..." may spin for a while. Success or failure does not matter (it downloads nothing this add-on needs, and in some regions it fails because of network restrictions). When it stops, click "Next".
**4.** 点“Next”，之后可能会一直转圈显示“Downloading compatibility information ...”。成功或失败都没关系（它下载的不是本插件需要的东西，在中国大陆等地区可能因网络限制而失败）。转圈结束后点“Next”。

**5.** Tick the fourth icon from the top, the red "Vulkan", and click "Next".
**5.** 勾选从上往下数第四个图标，红色的“Vulkan”，点“Next”。

**6.** "Downloading compatibility information ..." may appear again; again, the result does not matter. If you then see a red cross, click "Next". If you see "Select effects to install" with many checkboxes, do not change anything; click "Next".
**6.** “Downloading compatibility information ...”可能再次出现，结果同样无所谓。之后如果显示红叉，点“Next”；如果进入“Select effects to install”页面、出现大量勾选项，什么都不要改，直接点“Next”。

**7.** "Downloading Standard effects from https://github.com/......" may spin; the result does not matter. If you see a red cross, click "Next". If you see "Select add-ons to install" with many checkboxes, do not change anything; click "Next". If a failed download leaves only a "Finish" button, clicking it is also fine.
**7.** 可能出现“Downloading Standard effects from https://github.com/......”的转圈，结果无所谓。显示红叉就点“Next”；进入“Select add-ons to install”页面、出现大量勾选项时，什么都不要改，直接点“Next”。如果下载失败后只剩“Finish”按钮，点它结束也可以。

**8.** A green tick and "Successfully installed ReShade ......" means ReShade is installed. Click "Finish". You can delete the installer now or keep it for a reinstall.
**8.** 出现绿色对勾和“Successfully installed ReShade ......”表示安装成功。点“Finish”。之后可以删除安装程序，也可以留着以备重装。

**9.** Open the `unified-20261103l-candidate` folder, select everything inside it (all files and the `dlss5` folder; Ctrl+A works) and copy it into the game folder from step 2. Copy what is inside the folder, not the folder itself. If Windows asks for administrator permission, click "Continue". You can delete the extracted folder afterwards or keep it for a reinstall.
**9.** 打开 `unified-20261103l-candidate` 文件夹，全选里面的所有文件和文件夹（包括 `dlss5`，可以按 Ctrl+A），全部复制到第 2 步的游戏目录。复制的是文件夹里面的内容，不是这个文件夹本身。Windows 要求管理员权限时点“继续”。之后可以删除解压出的文件夹，也可以留着以备重装。

**10.** Turn on Windows "Hardware-accelerated GPU scheduling"; frame generation does not work without it. Part of this add-on runs at the entrance of frame generation, so you need at least 2x frame generation to use it. If you do not want frame generation at all, this add-on is not for you.
**10.** 打开 Windows 的“硬件加速 GPU 计划”，不开就不能用帧生成。本插件有一部分运行在帧生成的入口上，所以至少要开 2 倍帧生成才能正常使用。如果完全不想开帧生成，本插件不适合你。

- Windows 10: Settings → System → Display → Graphics settings.
  Win10：设置 → 系统 → 显示 → 图形设置。
- Windows 11: Settings → System → Display → Graphics → Advanced graphics settings ("Change default graphics settings" in older versions).
  Win11：设置 → 系统 → 显示 → 图形 → 高级图形设置（较旧版本里叫“更改默认图形设置”）。
- Or press Win+R, type `ms-settings:display-advancedgraphics` and click OK.
  或者按 Win+R，输入 `ms-settings:display-advancedgraphics`，点确定。

If you had to turn it on, restart the PC; if it was already on, no restart is needed.
如果是刚打开的，需要重启电脑；本来就开着则不需要。

## First start and settings / 首次启动与设置

The first start is only for settings, which take effect after a restart of the game. This is needed only once, right after installing.
第一次启动只用来做设置，重启游戏后才生效。只有刚安装后的第一次需要这样做。

**11.** Start the game. Press the **Home** key; the ReShade menu opens with a tutorial. Click "Skip tutorial" ("跳过教程"); ReShade's own tutorial is not made for this add-on. Do not change the game's color space (HDR) in ReShade: keep the game's original 8-bit sRGB format, or Present-stage NR cannot run.
**11.** 启动终末地，按 **Home** 键，会出现 ReShade 面板和教程指引。点“跳过教程”，ReShade 自带的教程不是为本插件设计的。不要在 ReShade 里修改游戏的色彩空间（HDR）：保持终末地原生的 8 位 sRGB 格式，否则“呈现”层 NR 无法运行。

**12.** The menu has the tabs "DLSS 5 Bridge", "DLSS 5 Neural Rendering", "MFG Unlock" and "Statistics" ("统计"). Each add-on tab has a language choice at the top. **Do not change anything under "Advanced settings" on any page, and nothing on other ReShade tabs, unless this guide says so.** The pages without Advanced settings cover normal use, and experience from other ReShade add-ons does not carry over to this one.
**12.** 面板上方有“DLSS 5 Bridge”、“DLSS 5 Neural Rendering”、“MFG Unlock”和“统计”这几页。每个插件页顶部可以选择语言。**除非本教程特别说明，不要改动任何页面“高级设置”里的选项，也不要改动其他 ReShade 页面的任何选项。** 不勾选“高级设置”的页面已能满足正常使用；以前用其他 ReShade 插件的经验不适用于本插件。

**13.** Open "DLSS 5 Bridge":
**13.** 点进“DLSS 5 Bridge”：

- Under "Deployment" ("统一部署"), choose `2.14.1 + DLSS-G 310.9.1`.
  “统一部署”选 `2.14.1 + DLSS-G 310.9.1`。
- Under "NR hook mode" ("NR 钩子模式"), choose "1: NGX + Streamline (suite Present NR)" ("1：NGX + Streamline（本套件呈现 NR）").
  “NR 钩子模式”选“1：NGX + Streamline（本套件呈现 NR）”。
- "Windows VRAM budget" ("Windows 显存预算"): Windows keeps part of the video memory away from the game, which is why the game can report running out of video memory while Task Manager still shows several hundred MB free. "On" ("开启") lifts part of that limit through a registry setting (on an 8 GB card roughly a few hundred MB more). It affects every program on the PC, takes effect only after a restart of Windows (also when turning it off), and is recommended only for cards with 8 GB or less. The full uninstall restores the original values.
  “Windows 显存预算”：Windows 会给游戏留出一部分用不到的显存，所以任务管理器还显示剩几百 MB 时，游戏可能已经提示显存不足。选“开启”会通过注册表在一定程度上解除这个限制（8 GB 显卡约能多用几百 MB）。它影响本机所有程序，开启或关闭后都要重启 Windows 才生效，只建议 8 GB 及以下显存的用户开启。完全卸载时会还原原值。

**14.** Close the game and start it again. Wait about 5–10 seconds after closing; starting again immediately can be blocked by ACE (the game's anti-cheat). If that window appears, close it, wait a few seconds and start again.
**14.** 关闭游戏后重新启动。关闭后请等 5～10 秒再启动，立刻启动可能会被 ACE（游戏的反作弊）拦截。如果弹出拦截窗口，关掉它，过几秒再启动即可。

**15.** In the game's settings, set "画质提升" (upscaling) to "DLSS". Any DLSS Super Resolution mode works; "Quality" with DLSS model preset L is recommended. Turn on DLSS Frame Generation with any multiplier from 2x to 4x (MFG Unlock can take over the multiplier later). Do not choose "Auto" ("自动") and do not leave frame generation off: Present-stage NR runs at the entrance of frame generation, so it stops when frame generation is off, and with "Auto" MFG Unlock cannot take over.
**15.** 进入游戏设置，把“画质提升”改成“DLSS”。“DLSS 超分辨率模式”可选任意档位，推荐“质量”配合 L 超分模型。打开 DLSS 帧生成，倍率选 2～4x 均可（之后可由 MFG Unlock 接管）。不要选“自动”，也不要关闭帧生成：“呈现”层 NR 运行在帧生成的入口上，关闭帧生成会使它失效；选“自动”会让 MFG Unlock 无法接管。

If the game has no "DLSS Super Resolution" option at all:
如果游戏里根本没有“DLSS 超分辨率模式”选项：

- **RTX 2050, RTX 3050 Laptop, RTX 3050 Ti Laptop:** in Vulkan mode the game hides DLSS Super Resolution, DLSS Frame Generation and Reflex on these GPUs (not in DirectX 11 mode). The add-on makes them available again. These GPUs have only 4 GB of video memory, so even DLSS alone is tight; see the RTX 3050 Laptop rows in "Measured frame rates".
  **RTX 2050、RTX 3050 Laptop、RTX 3050 Ti Laptop：** 游戏在 Vulkan 模式下会对这三款显卡隐藏 DLSS 超分、DLSS 帧生成和 Reflex（DX11 模式没有这个限制）。插件让它们重新可用。这些显卡只有 4 GB 显存，即使只开 DLSS 超分也很吃紧，参考“实测帧率”中 RTX 3050 Laptop 的数据。
- **Other RTX 20–50 cards:** the cause is not found yet. Some testers on RTX 40/50 got the option back by turning on Smooth Motion ("AI 插帧") for the game in the NVIDIA App; if you do that, keep the MFG Unlock multiplier at 2x or lower, or it will very likely stutter. RTX 20/30 cannot use Smooth Motion, and for some users it did not help. Reports are welcome.
  **其他 20～50 系显卡：** 原因尚未查明。部分 40/50 系内测用户在 NVIDIA App 里为游戏开启“AI 插帧”（Smooth Motion）后选项恢复出现；如果这样做，MFG Unlock 的倍率请不要超过 2x，否则大概率会很卡。20/30 系无法开启 AI 插帧，也有用户开启后仍无效。欢迎反馈。

**16.** Press Home and open "DLSS 5 Neural Rendering". Under "Hook point" ("介入点") there are two rows of buttons, 0 and 1: "Upscaled layers" ("超分后层数") and "Present layers" ("呈现层数"). Each number is how many times NR runs at that stage. A new installation starts with Upscaled 0 and Present 1 at resolution "Performance" ("性能").
**16.** 按 Home，点进“DLSS 5 Neural Rendering”。“介入点”下方有两行 0 和 1 的按钮：“超分后层数”和“呈现层数”，数字表示 NR 在该阶段运行几次。新安装时默认为超分后 0 层、呈现 1 层，分辨率“性能”。

**17.** Choose the layers and the resolution according to your card; the table in "Measured frame rates" gives tested combinations. For Arknights: Endfield, use only one of these three combinations: Upscaled 0 + Present 1, Upscaled 1 + Present 1, or Upscaled 1 + Present 0. Do not use Advanced settings to get 3 or more layers in total, 2 or more layers in one row, or any Render layer.
**17.** 按显卡情况选择层数和分辨率，“实测帧率”中的表格给出了实测过的组合。对于终末地，只建议使用以下三种组合：超分后 0 + 呈现 1、超分后 1 + 呈现 1、超分后 1 + 呈现 0。不要通过“高级设置”让总层数达到 3 层及以上、某一行达到 2 层及以上，也不要使用“渲染”层。

With 8 GB of video memory or less, or 16 GB of RAM or less, raise quality step by step and watch video memory and memory use in Task Manager. After changing layers or resolution, a slow-motion or stuttering phase of about 10 seconds is normal on small cards; wait, and do not switch often. Going straight to the highest settings can run out of video memory (the game becomes extremely slow, so that even the ReShade menu is hard to use) or out of RAM (the game can freeze or close). If that happens, restart the game and lower the settings in the ReShade menu on the login screen, before entering the game world.

显存 8 GB 及以下或内存 16 GB 及以下时，请逐步提高画质，同时观察任务管理器中的显存和内存占用。小显存显卡在切换层数或分辨率后出现约 10 秒的慢动作或卡顿是正常现象，请耐心等待，不要频繁切换。一下子开到最高容易爆显存（游戏会极其卡顿，连 ReShade 面板都难以操作）或爆内存（游戏可能卡死甚至闪退）。如果出现这种情况，重启游戏，先不要进入游戏世界，在登录界面打开 ReShade 面板把参数改回低档。

**18.** Adjust the other settings of the "DLSS 5 Neural Rendering" page to your taste. The "Adjust NR's look" ("调整 NR 的外观") switch turns all "Result" and "Limits and stability" parameters on or off at once, so you can compare the image before and after your adjustments; it is off by default.
**18.** “DLSS 5 Neural Rendering”页的其他参数按喜好调整。“调整 NR 的外观”开关一键控制“结果”和“限制与稳定性”中的全部参数是否生效，方便对比调色前后；默认关闭。

**19.** The default key that turns NR on and off is F6, which is also the game's pass key. Change it in "Hotkeys & screenshots" ("快捷键与截图") at the bottom of the "DLSS 5 Neural Rendering" page. The screenshot key (F5 by default) saves one PNG of the final image after all NR layers; "Screenshot folder" ("截图保存位置") sets where it goes (empty: a `DLSS5 Screenshots` folder in the game folder). To avoid two different screenshots from one key, the add-on clears ReShade's own screenshot key at every start.
**19.** 开关 NR 的默认快捷键是 F6，它也是终末地通行证页面的按键。在“DLSS 5 Neural Rendering”页最下方的“快捷键与截图”中修改。截图键（默认 F5）每按一次保存一张经过全部 NR 层处理后的最终画面 PNG；“截图保存位置”设置保存文件夹（留空时保存到游戏目录下的 `DLSS5 Screenshots` 文件夹）。为避免同一按键保存两种截图，插件每次启动都会清除 ReShade 自己的截图键。

**20.** For a higher frame rate, open "MFG Unlock" and choose a multiplier up to "6x" instead of "Game default" ("由游戏决定"). The game's own multiplier setting then has no effect; choose "Game default" to give control back to the game. In-game frame generation must still stay on and must not be "Auto".
**20.** 想要更高帧数时，进入“MFG Unlock”，把“由游戏决定”改为最高“6x”的倍率。此后游戏设置里的帧生成倍率不再起作用；选回“由游戏决定”即可交还给游戏。游戏内的帧生成仍必须保持开启，且不能选“自动”。

**21.** Done. Play for a moment, press Home and open "Statistics" ("统计"). You should see a fairly flat frame-time curve with small, gentle bumps. If it looks like a saw blade or is irregular, first check the steps above, then:
**21.** 设置完成。进游戏玩一会儿，按 Home 打开“统计”页，应该看到一条比较平滑、略有起伏的帧时间曲线。如果是锯齿状或不规则形状，请先检查上面的步骤，再检查：

1. **Smooth Motion ("AI 插帧") in the NVIDIA App** must be off for this game (except for the workaround in step 15, with at most 2x). It makes the frame rate very unstable; this add-on does not support it because of its high latency.
   **NVIDIA App 里的“AI 插帧”（Smooth Motion）** 必须对本游戏关闭（第 15 步中的替代方案除外，且倍率不超过 2x）。它会让帧数非常不稳定；因延迟太高，本插件不支持它。
2. **Windows Game Bar and Game Mode:** one tester's stutter disappeared after turning both off; it is not known which of the two caused it.
   **Windows Game Bar 和游戏模式：** 有内测用户两个都关闭后卡顿消失，目前无法确定是其中哪一个导致的。

## If it still stutters or NR does not work / 如果仍然卡顿或 NR 不生效

**22.**

1. Windows "HDR" may be on, but turn off "Auto HDR" and "Automatically manage color for apps" in its menu. If you do not know what they are, turn all three off.
   Windows 的“HDR”可以开，但要关闭其菜单里的“自动 HDR”和“自动管理应用的颜色”。不清楚它们是什么时，就全部关闭。
2. Turn off NVIDIA game filters, RTX HDR and RTX Dynamic Vibrance for this game; they can stop the add-on or cause stutter. The game itself does not work well with them (dull image, wrong UI contrast); a dull image without them usually comes from "Automatically manage color for apps".
   对本游戏关闭 NVIDIA 游戏滤镜、RTX HDR 和 RTX Dynamic Vibrance，否则可能使插件失效或游戏卡顿。游戏本身也不适合这些功能（画面发灰、UI 对比度失调）；不开它们画面发灰时，通常是开了“自动管理应用的颜色”。
3. Try turning off G-SYNC in the NVIDIA driver. It does not stutter on every PC, but many testers reported that turning it off removed stutter.
   尝试在 NVIDIA 驱动中关闭 G-SYNC。并非所有电脑都会因此卡顿，但很多内测用户反馈关闭后卡顿消失。
4. **Safe mode is still on.** If NR still does not work after every setting is correct, and the "DLSS 5 Neural Rendering" page says "NR runtime not installed" ("未安装 NR 运行时") and "NVIDIA's signed nvngx_dlssnr.dll is not next to the addon or the game's executable", safe mode was switched on by an earlier problem. Open "DLSS 5 Bridge"; a ticked "Safe mode next launch" ("下次启动安全模式（手动退出）") is shown. Untick it and restart the game. Safe mode lets you keep playing without the add-on's interventions when something is badly incompatible; it cannot guarantee that in every extreme case.
   **安全模式仍在开启。** 如果所有设置都正确后 NR 仍不生效，且“DLSS 5 Neural Rendering”页提示“未安装 NR 运行时”、“插件或游戏可执行文件旁没有 NVIDIA 签名的 nvngx_dlssnr.dll……”，说明之前的问题让安全模式开启了。点进“DLSS 5 Bridge”，会看到已勾选的“下次启动安全模式（手动退出）”，取消勾选并重启游戏即可。安全模式用于在严重不兼容时让游戏暂时不受插件干预地运行，但无法保证在所有极端情况下都有效。

## Updating from unified-20261015q / 从 unified-20261015q 升级

Close the game, copy everything from `unified-20261103l-candidate` into the game folder as in step 9, and choose "Replace" for existing files. Your settings in `ReShade.ini` are kept: new defaults apply only to values you never saved, and hidden options keep their saved values. Steps 1–8 can be skipped. Optionally set "Windows VRAM budget" (step 13). If you still use the 2026-10-05 version, first follow "Already using the 2026-10-05 version" in [docs/history/README-unified-20261015q.md](https://github.com/Yukikaze20170315/endfield-dlss5-vulkan-plugin/blob/main/docs/history/README-unified-20261015q.md).

关闭游戏，按第 9 步把 `unified-20261103l-candidate` 里的全部内容复制到游戏目录，已存在的文件选“替换”。`ReShade.ini` 里的设置会保留：新默认值只用于你从未保存过的项，被隐藏的选项也保留原值。第 1～8 步可以跳过。可按需设置“Windows 显存预算”（第 13 步）。如果还在用 2026-10-05 版本，请先按 [docs/history/README-unified-20261015q.md](https://github.com/Yukikaze20170315/endfield-dlss5-vulkan-plugin/blob/main/docs/history/README-unified-20261015q.md) 中“已经在用 2026-10-05 版本”一节操作。

## Updates, uninstalling, known issues and bug reports / 更新、卸载、已知问题与反馈

**23. After a game update.** Some features may stop working for a while. Such updates usually do not stop the game from running with NR turned off, even with the old add-on version, and a new version will follow for broken features. A large update that changes how DLSS or frame generation work at a low level can stop the game from starting with the old add-on; then uninstall (step 24) and play without it until a new version is out.
**23. 游戏更新后。** 部分功能可能暂时失效。这类更新通常不影响“新版游戏 + 旧版插件”在关闭 NR 时正常运行，受影响的功能会尽快出新版。若游戏大改了 DLSS 或帧生成的底层逻辑，旧版插件可能导致游戏完全无法启动；此时请按第 24 条卸载，在新版发布前先原生游玩。

**24. Uninstall.** Close the game. In the game folder, double-click `DLSS5-Recovery 故障恢复.cmd`, read what it will do, type **Y** and press Enter. Allow the administrator request if Windows asks. After about ten seconds to a few tens of seconds the window reports the result:
**24. 卸载。** 关闭游戏，双击游戏目录中的 `DLSS5-Recovery 故障恢复.cmd`，阅读窗口说明后输入 **Y** 并回车。Windows 请求管理员权限时选择允许。约十几秒到几十秒后，窗口会显示结果：

- "Uninstall finished": the add-ons, the NR files and the package's settings and logs were moved into a `DLSS5-Recovery-Backups` folder next to the game folder (one level up), and the driver settings and the Windows VRAM budget were restored. Run the file verification/repair of the game in the launcher, and restart Windows if you had turned the VRAM budget on. Once the game works, you can delete `DLSS5-Recovery-Backups`.
  “卸载完成”：插件、NR 文件以及本包的配置和日志已移到游戏目录上一级的 `DLSS5-Recovery-Backups` 文件夹，驱动设置和 Windows 显存预算已还原。请在启动器里对游戏执行一次“校验/修复”；如果之前开启过显存预算，请重启 Windows。确认游戏正常后，可以删除 `DLSS5-Recovery-Backups`。
- If other programs use the same ReShade, the window lists them and asks again; Y continues (ReShade then stops loading for them too).
  如果有其他程序共用同一个 ReShade，窗口会列出它们并再次询问；输入 Y 继续（这些程序也将不再加载 ReShade）。
- "Restore incomplete": the add-ons were removed but the driver settings or the VRAM budget were not fully restored. Close other programs (especially the NVIDIA App and NVIDIA Control Panel) and run the file again; it stays in the game folder for that.
  “还原未完成”：插件已撤除，但驱动设置或显存预算没有完全还原。关闭其他程序（尤其是 NVIDIA App 和 NVIDIA 控制面板）后再运行一次；为此工具会留在游戏目录中。

**25. Known issues that do not stop you from playing.**
**25. 已知但不影响使用的问题。**

1. The game's photo camera takes its picture before Present-stage NR, so its photos do not show it. The game's camera will not be changed (that would break the promise not to change game features). Use the add-on's screenshot key instead; to use the camera's filters, set up the shot, press X to hide the UI, then press the screenshot key.
   游戏内置相机的拍照发生在“呈现”层 NR 之前，拍不到“呈现”层效果。不会修改游戏相机（这违背“不修改游戏功能”的承诺）。请使用本插件的截图键；想用相机滤镜时，调好位置和滤镜后按 X 隐藏 UI，再按截图键。
2. Right after a character revives, right after a map has loaded and on some UI screens such as the backpack, part of a model's shadow behind may show through. The cause is not found.
   角色刚复活、地图刚加载完，以及背包等部分 UI 下，可能映出背后的一部分模型影子。原因尚未查明。
3. In rare cases NR turns off and on several times in a row (flashing). A restart of the game fixes it. It was seen twice in about half a month across four test PCs and the internal testers and could not be reproduced on purpose, so it is not known whether it still happens after this release's changes. Reports are welcome.
   极少数情况下 NR 会快速连续关闭又开启（闪烁），重启游戏即可解决。在 4 台测试机和内测用户中约半个月只出现过 2 次，且无法故意复现，因此不确定本版的改动后是否仍会出现。欢迎反馈。
4. With NR on, dragging in the ReShade menu can feel a little sluggish, even on fast PCs. This comes from ReShade.
   开启 NR 后，在 ReShade 面板里拖动鼠标可能略有卡顿，与性能无关，来自 ReShade 本身。
5. When the real (rendered) frame rate drops below about 15 frames per second, the game does not stutter but runs in slow motion, because the engine advances at most a fixed amount of game time per frame. On cards slower than an RTX 2070, avoid two NR layers.
   真实（渲染）帧率低于约 15 帧/秒时，游戏不会卡顿，而是进入慢动作，因为引擎每帧最多只推进一定的游戏时间。性能低于 RTX 2070 的显卡不建议开两层 NR。

**26. Reporting a bug.** After the bug happens, exit the game and do not start it again until you have sent the report. Send `ReShade.log` from the game folder, and a zip of every file in `dlss5\dlss5_log` inside the game folder.
**26. 反馈 Bug。** 触发 Bug 后退出游戏，在完成反馈前不要重启游戏。发送游戏目录中的 `ReShade.log`，以及游戏目录下 `dlss5\dlss5_log` 文件夹内全部文件的压缩包。

**27. Bugs that are hard to solve** (usually after a report that did not lead to a fix): in "DLSS 5 Bridge", tick "Advanced settings", scroll to the bottom and tick "Full anomaly log (one full line per occurrence)" ("完整异常日志（每次发生都完整记录一行）"); confirm in the window that opens. Trigger the bug a few times, exit the game and send the same files as in step 26. This mode can write a lot of data (in the worst case tens of GB per day); turn it off afterwards and delete large files in `dlss5_log` if needed; they are not needed for the add-on to run.
**27. 难以解决的疑难问题**（通常是反馈后仍未修好时）：在“DLSS 5 Bridge”页勾选“高级设置”，滑到最下方，勾选“完整异常日志（每次发生都完整记录一行）”，并在弹出的窗口中确认。然后触发几次 Bug，退出游戏，按第 26 条发送同样的文件。该模式可能写入大量数据（最极端时一天几十 GB）；反馈后请关闭它，必要时删除 `dlss5_log` 中的大文件，它们不是插件运行所必需的。

---

# Measured frame rates / 实测帧率

Displayed frame rates (including generated frames) measured by the author with this package. "U1+P1" means Upscaled 1 layer + Present 1 layer; "P1" means Present 1 layer. "NR resolution" is the tier on the "DLSS 5 Neural Rendering" page (Performance / Quality / Full) or the resolution scale. "VRAM budget" says whether the "Windows VRAM budget" switch was on. A remote-access program was running during these measurements; it uses some GPU time and video memory itself.

以下为作者用本包实测的上屏帧率（含生成帧）。“U1+P1”表示超分后 1 层 + 呈现 1 层，“P1”表示呈现 1 层。“NR 分辨率”为“DLSS 5 Neural Rendering”页的分辨率档位（性能 / 质量 / 原画）或分辨率比例。“显存预算”表示“Windows 显存预算”开关是否开启。测试期间运行着远程访问软件，它本身也会占用一部分 GPU 性能和显存。

| GPU (VRAM) / 显卡（显存） | Resolution / 分辨率 | VRAM budget / 显存预算 | Purpose / 用途 | Game DLSS SR / 游戏内超分 | NR layers / NR 层 | NR resolution / NR 分辨率 | FG / 帧生成 | SR model / 超分模型 | FPS / 帧率 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| RTX 2070 (8 GB) | 1920×1080 | On / 开 | Highest frame rate / 最高帧率 | Quality / 质量 | P1 | scale 33 % / 比例 33% | 6x | K | 120–130 |
| RTX 2070 (8 GB) | 1920×1080 | On / 开 | Everyday / 日常 | Quality / 质量 | P1 | Performance / 性能 | 4x | L | 60–70 |
| RTX 2070 (8 GB) | 1920×1080 | On / 开 | Highest quality, photos only (heavy slow motion) / 最高画质，仅拍照（慢动作严重） | Quality / 质量 | U1+P1 | Quality / 质量 | 4x | L | 30–40 |
| RTX 3070 Ti (8 GB) | 2560×1440 | On / 开 | One layer, highest frame rate / 一层最高帧率 | Quality / 质量 | P1 | Performance / 性能 | 6x | L | 120–130 |
| RTX 3070 Ti (8 GB) | 2560×1440 | On / 开 | One layer, everyday / 一层日常 | Quality / 质量 | P1 | Performance / 性能 | 3x | L | 70–80 |
| RTX 3070 Ti (8 GB) | 2560×1440 | On / 开 | Two layers, highest frame rate / 两层最高帧率 | Quality / 质量 | U1+P1 | Performance / 性能 | 6x | L | 90–100 |
| RTX 3070 Ti (8 GB) | 2560×1440 | On / 开 | Two layers, everyday / 两层日常 | Quality / 质量 | U1+P1 | Performance / 性能 | 4x | L | 60–70 |
| RTX 4060 Laptop (8 GB) | 2560×1440 | On / 开 | Highest frame rate / 最高帧率 | Quality / 质量 | U1+P1 | Performance / 性能 | 6x | L | 100–110 |
| RTX 4060 Laptop (8 GB) | 2560×1440 | On / 开 | Everyday / 日常 | Quality / 质量 | U1+P1 | Performance / 性能 | 3x | L | 60–70 |
| RTX 3050 Laptop (4 GB) | 1920×1080 | On / 开 | No slow motion / 无慢动作 | Ultra Performance / 超级性能 | P1 | scale 33 % / 比例 33% | 2x | L | 25–30 |
| RTX 3050 Laptop (4 GB) | 1920×1080 | On / 开 | With slow motion, not recommended / 有慢动作，不推荐 | Ultra Performance / 超级性能 | P1 | scale 33 % / 比例 33% | 6x | L | 50–60 |
| RTX 4080 (16 GB) | 3840×2160 | Off / 关 | Highest frame rate / 最高帧率 | Quality / 质量 | U1+P1 | Performance / 性能 | 6x | L | 160–170 |
| RTX 4080 (16 GB) | 3840×2160 | Off / 关 | Everyday, high frame rate / 日常高帧 | Quality / 质量 | U1+P1 | Performance / 性能 | 4x | L | 120–130 |
| RTX 4080 (16 GB) | 3840×2160 | Off / 关 | Everyday / 日常 | Quality / 质量 | U1+P1 | Performance / 性能 | 2x | L | 70–80 |
| RTX 4080 (16 GB) | 3840×2160 | Off / 关 | Highest quality / 最高画质 | Quality / 质量 | U1+P1 | Full / 原画 | 6x | L | 80–90 |
| RTX 5090 (32 GB) | 3840×2160 | Off / 关 | Highest frame rate / 最高帧率 | Quality / 质量 | U1+P1 | Performance / 性能 | 6x | L | 300–320 |
| RTX 5090 (32 GB) | 3840×2160 | Off / 关 | Everyday / 日常 | Quality / 质量 | U1+P1 | Quality / 质量 | 3x | L | 150–160 |
| RTX 5090 (32 GB) | 3840×2160 | Off / 关 | Highest quality / 最高画质 | DLAA | U1+P1 | Full / 原画 | 2x | L | 60–70 |
| RTX 5090 (32 GB) | 5120×2160 | Off / 关 | Highest frame rate / 最高帧率 | Quality / 质量 | U1+P1 | Performance / 性能 | 6x | L | 250–260 |
| RTX 5090 (32 GB) | 5120×2160 | Off / 关 | Everyday / 日常 | Quality / 质量 | U1+P1 | Quality / 质量 | 4x | L | 150–160 |
| RTX 5090 (32 GB) | 5120×2160 | Off / 关 | Highest quality / 最高画质 | DLAA | U1+P1 | Full / 原画 | 3x | L | 65–75 |

Notes / 说明：

- RTX 2070: the limit with two layers is compute, not video memory. Two layers bring the real frame rate below about 15 frames per second, where the game runs in slow motion (known issue 5).
  RTX 2070：两层时的瓶颈是算力而不是显存。两层会让真实帧率低于约 15 帧/秒，游戏随之进入慢动作（已知问题第 5 条）。
- RTX 3050 Laptop at 2x: 25–30 fps, but the frame pacing is even, so motion looks smoother than the number suggests. At 6x the real frame rate is low enough for slow motion. Model L uses only about 30 MB more video memory than K and keeps the most detail at Ultra Performance.
  RTX 3050 Laptop 2 倍时：25～30 帧，但帧节奏均匀，肉眼观感比数字更流畅。6 倍时真实帧率低到会出现慢动作。L 模型只比 K 模型多占约 30 MB 显存，在超级性能档下画面还原度最高。

Shared by an internal tester; not measured by the author, and the test conditions could not be fully checked:
以下由内测用户提供，非作者实测，测试条件无法完全核实：

| GPU (VRAM) / 显卡（显存） | Resolution / 分辨率 | VRAM budget / 显存预算 | Game DLSS SR / 游戏内超分 | NR layers / NR 层 | NR resolution / NR 分辨率 | FG / 帧生成 | SR model / 超分模型 | FPS / 帧率 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| RTX 4060 Ti (16 GB) | 1920×1080 | On / 开 | DLAA | P1 | Performance / 性能 | 2x | K | 133–140 |
| RTX 4060 Ti (16 GB) | 1920×1080 | On / 开 | DLAA | U1+P1 | Performance / 性能 | 6x | L | 80–90 |

---

# Technical details / 技术说明

Everything below is for developers and maintainers: components, what changed since `unified-20261015q` and why, how it was checked, and what is still open. "Intermediate builds" are the builds between the two releases (`unified-20261021r` to `unified-20261102k`, and development builds named `vramserial*` and `pageable*`); their code is contained in this release unless stated.

以下内容面向开发者与维护者：组件、自 `unified-20261015q` 以来的改动及原因、验证方式和未解决的问题。“中间构建”指两次发布之间的构建（`unified-20261021r`～`unified-20261102k`，以及名为 `vramserial*`、`pageable*` 的开发构建）；除特别说明外，其代码都包含在本版中。

## 1. Components and versions / 组件与版本

The package `unified-20261103l-candidate.zip` (SHA-256 `a4bc27f82fb83433fd5bf7243e2b598449b98e79d8f8a5acb3516f012d322985`, 280,391,058 bytes) contains 23 files; `SHA256SUMS.txt` and `release-manifest.json` inside list them.

成品包 `unified-20261103l-candidate.zip`（SHA-256 `a4bc27f82fb83433fd5bf7243e2b598449b98e79d8f8a5acb3516f012d322985`，280,391,058 字节）含 23 个文件，包内 `SHA256SUMS.txt` 和 `release-manifest.json` 列出了全部文件。

| File / 文件 | Version / 版本 | SHA-256 | Source / 来源 |
| --- | --- | --- | --- |
| `dlss5-bridge.addon64` | `1.4.13-pre8-unified2d-20261101` | `778265b3e6ab58119888cddc9c788dabe26e5cc7a2ef0df98538667874acf014` | based on / 基于 [NIGos/dlss5-bridge](https://github.com/NIGos/dlss5-bridge) |
| `renodx-dlss5.addon64` | Generic `v8.5.0-rc10-unified2l-20261103` | `8790e0d3cad277861c198d1f76bd9e9fc3564ab226b243787d59b73e0c7ce7ca` | based on / 基于 RenoDX DLSS 5 Generic 8.5.0-rc10 |
| `renodx-mfgunlock.addon64` | MFG Unlock `0.11+unified2j-20261102` | `a9d88b5403f8d4be3de8de59cbb257fb32ea14a1777065eb2a81b118242f796e` | based on / 基于 [nefh/MFGAmpereUnlock-RenoDx](https://github.com/nefh/MFGAmpereUnlock-RenoDx) tag `0.11` |
| `dlss5/nr/rtx50/nvngx_dlssnr.dll` | 310.8.0.0, NVIDIA-signed, `sm_120` only | `e16bcf15e16e13f527491cdf7845b2fe6521a738d8f7c9c721866a8496e1fc8e` | NVIDIA NR runtime |
| `dlss5/nr/rtx20-40/nvngx_dlssnr.dll` | 310.8.SF.0, not signed / 未签名 | `6eb209e764f39872625debd6abaf45e2bb6322f6f270f781f70c059ae30b3927` | community-modified runtime / 社区修改版，[RHI release dlssnr-310.8.SF-v2](https://github.com/RankFTW/rhi-repo/releases/tag/dlssnr-310.8.SF-v2) |
| `dlss5/sl-2.14.1/` (11 files / 11 个文件) | Streamline 2.14.1, DLSS-G 310.9.1, DLSS/DLSS-D/DeepDVC 310.5.2 | see / 见 `SHA256SUMS.txt` | NVIDIA Streamline / NGX |
| `DLSS5-Recovery 故障恢复.cmd`, `DLSS5-Clean-Recovery.ps1`, `DLSS5-RestoreDriver.exe` | — | see / 见 `SHA256SUMS.txt` | this project / 本项目 |
| `DLSS5-使用与恢复 Usage and Recovery.md`, `payload-manifest.json`, `release-manifest.json`, `SHA256SUMS.txt` | — | — | this project / 本项目 |

The three add-ons were built from the source in this repository. The bridge and MFG Unlock were built in the intermediate builds unified2d and unified2j; their source has not changed since, which is why their versions differ from Generic's. The runtime files are unchanged since `unified-20261015q`.

三个插件均由本仓库中的源码构建。Bridge 和 MFG Unlock 分别构建于中间构建 unified2d 和 unified2j，此后其源码未再改动，所以它们的版本号与 Generic 不同。运行库文件自 `unified-20261015q` 起没有变化。

## 2. How the pieces fit together / 组件如何协作

- **Generic** runs NVIDIA's NR runtime at up to three stages per frame: Render (before the game's upscaler), Upscaled (after it) and Present (on the finished frame). On Vulkan it now runs all three itself: Render and Upscaled inside the game's own DLSS evaluate, Present on the game's present queue just before Streamline and DLSS Frame Generation (DLSS-G) take the frame (section 3.1). It also owns the panel, the screenshot and the overlay cursor confinement.
  **Generic** 每帧最多在三个阶段运行 NVIDIA 的 NR 运行库：渲染（游戏超分之前）、超分后（之后）、呈现（成品画面上）。在 Vulkan 下，三个阶段现在都由它自己运行：渲染和超分后在游戏自己的 DLSS evaluate 内，呈现在游戏的呈现队列上、Streamline 与 DLSS 帧生成（DLSS-G）接手画面之前（3.1 节）。面板、截图和打开菜单时的鼠标限制也由它负责。
- **The bridge** hands the game's `vkQueuePresentKHR` to Generic at its existing detour in front of Streamline, and owns everything specific to this game and PC: which Streamline build is loaded, which NR runtime file is used for the GPU, the NVIDIA driver-profile frame-generation settings, the Windows VRAM budget switch, the start-up health record and safe mode, background FG retention and composition guard, the log folder and the anomaly log. Its former Present path (a private Direct3D 12 device with a DLAA carrier and a GPU relay) stays in the source but is not started when Generic exports the native Vulkan Present entry.
  **Bridge** 在其位于 Streamline 之前的既有 detour 中把游戏的 `vkQueuePresentKHR` 交给 Generic，并负责一切与本游戏、本机相关的事务：加载哪个 Streamline 版本、按显卡使用哪个 NR 运行库文件、NVIDIA 驱动配置中与帧生成相关的设置、Windows 显存预算开关、启动健康记录与安全模式、后台帧生成保持与合成守护、日志目录与异常日志。它以前的呈现路径（私有 Direct3D 12 设备 + DLAA 载波 + GPU 中继）仍留在源码中，但当 Generic 导出原生 Vulkan 呈现入口时不会启动。
- **MFG Unlock** is the only component that changes the frame-generation multiplier. On RTX 50 it only changes the multiplier and keeps NVIDIA's native kernels; on RTX 20/30/40 it keeps the compatibility path of MFG Unlock 0.11.
  **MFG Unlock** 是唯一修改帧生成倍率的组件。在 RTX 50 上只改倍率、保留 NVIDIA 原生内核；在 RTX 20/30/40 上沿用 MFG Unlock 0.11 的兼容路径。

The add-ons talk only through versioned exports: bridge `DLSS5GetNrRuntimePathV2`, `DLSS5LogEventV1`, `DLSS5LogFullModeV1`; Generic `DLSS5HookModeControlV1`, `DLSS5StageProtocolVersion`/`DLSS5GetStagePlan`, and new in this release `DLSS5VulkanPresentNativeV1` and `DLSS5VulkanPrePresentV1`. A missing or older partner is detected and reported.

各插件只通过带版本号的导出函数通信：Bridge 的 `DLSS5GetNrRuntimePathV2`、`DLSS5LogEventV1`、`DLSS5LogFullModeV1`；Generic 的 `DLSS5HookModeControlV1`、`DLSS5StageProtocolVersion`/`DLSS5GetStagePlan`，以及本版新增的 `DLSS5VulkanPresentNativeV1` 和 `DLSS5VulkanPrePresentV1`。缺失或版本较旧的一方会被检测并报告。

## 3. Changes since unified-20261015q / 自 unified-20261015q 以来的改动

### 3.1 Present-stage NR on the game's Vulkan device / “呈现”层 NR 改在游戏的 Vulkan 设备上运行

- **Problem.** On 8 GB cards the game uses about 5.6 GB; the bridge's private Direct3D 12 device for the Present stage added about 1.1–1.4 GB (measured with WPR allocation events on an RTX 4060 Laptop: about 667 MiB for the NGX DLAA carrier feature, 228 MiB for 14 relay slots, about 189 MiB D3D12 working set, plus transient allocations). Together they exceeded the process budget, Windows moved hot game allocations to system memory, and the frame rate dropped to about 47 fps after stress and did not recover.
  **问题。** 在 8 GB 显卡上游戏本身约用 5.6 GB；Bridge 为呈现层建立的私有 Direct3D 12 设备又多占约 1.1～1.4 GB（在 RTX 4060 Laptop 上用 WPR 分配事件测得：NGX DLAA 载波特征约 667 MiB，14 个中继槽 228 MiB，D3D12 工作集约 189 MiB，另有瞬时分配）。两者合计超出进程预算，Windows 把游戏的热分配挪到系统内存，压力测试后帧率降到约 47 fps 且无法恢复。
- **First step (intermediate build vramserial9).** Generic ran the Present layers inside the DLSS-G Vulkan evaluate, on the game's device; the bridge no longer created its private device. This removed the memory, but NR then delayed every generated frame by its GPU time (about 13 ms), and at 3x and above most generated frames missed their slot (at 6x 41 presents per second with NR on against 170 with NR off).
  **第一步（中间构建 vramserial9）。** Generic 在 DLSS-G 的 Vulkan evaluate 内、在游戏自己的设备上运行呈现层，Bridge 不再创建私有设备。显存问题由此消除，但 NR 的 GPU 耗时（约 13 ms）随之推迟了每个生成帧，3 倍及以上时大部分生成帧错过显示时机（6 倍时开 NR 每秒上屏 41 次，关 NR 为 170 次）。
- **Change (vramserial10 to 10e).** The bridge's detour of `sl.interposer`'s `vkQueuePresentKHR` (in front of Streamline) passes the present to Generic's `DLSS5VulkanPrePresentV1`. Generic runs NR on the swapchain image Streamline will hand to DLSS-G, in its own command buffers on the game's present queue, waiting on the present's semaphores and signalling its own, which the bridge substitutes into the present. Motion vectors and depth come from the previous DLSS-G evaluate. If this path cannot serve a frame, NR falls back to running inside the evaluate. Follow-up fixes: the swapchain images are matched against the backbuffers DLSS-G actually names (Streamline gives the game its own images on Vulkan); replaced resources are released by a proof that their command buffers were reused, so changing layers or resolution no longer stalls the Present stage for 5–7 s; the fallback path no longer creates and releases an NR feature every frame; a new swapchain (resolution or window mode change) resets the cached image lists.
  **改动（vramserial10～10e）。** Bridge 对 `sl.interposer` 的 `vkQueuePresentKHR` 的 detour（位于 Streamline 之前）把 present 交给 Generic 的 `DLSS5VulkanPrePresentV1`。Generic 对 Streamline 将交给 DLSS-G 的交换链图像运行 NR，使用自己的命令缓冲、在游戏的呈现队列上提交，等待 present 原有的信号量并发出自己的信号量，再由 Bridge 换入 present。运动矢量和深度取自上一次 DLSS-G evaluate。该路径无法处理某帧时，回退到在 evaluate 内运行 NR。后续修正：交换链图像必须与 DLSS-G 实际使用的 backbuffer 对上（Vulkan 下 Streamline 给游戏的是它自己的一组图像）；被替换的资源凭“其命令缓冲已被重新使用”的证明释放，切换层数或分辨率后呈现层不再停摆 5～7 秒；回退路径不再每帧创建又释放一次 NR 特征；新交换链（改分辨率或窗口模式）会重置缓存的图像列表。
- **Checked.** Offline: Vulkan NR resolution (218 + 27 checks), replacement serialisation (2,325 + 10), NR demand (5 + 9 + 577). In game on an RTX 4060 Laptop with vramserial10d: repeated changes of Present layers and NR resolution under stress; replacement waits ended after 1–5 evaluates (previously up to 304), no "still waiting after 32 evaluates", `not_backbuffer=0`, and the user reported NR stayed effective throughout. The 6x frame rates of the RTX 4060 Laptop in the table (100–110 fps with two layers) were measured with this release.
  **验证。** 离线：Vulkan NR 分辨率（218 + 27 项）、替换串行化（2,325 + 10 项）、NR 需求（5 + 9 + 577 项）。在 RTX 4060 Laptop 上用 vramserial10d 做游戏内检查：在压力下反复切换呈现层数和 NR 分辨率；替换等待在 1～5 次 evaluate 后结束（此前最多 304 次），没有出现“32 次 evaluate 后仍在等待”，`not_backbuffer=0`，用户反馈 NR 全程有效。表中 RTX 4060 Laptop 6 倍的帧率（两层 100～110 fps）是用本版测得的。

### 3.2 Video memory: release before create, residency priority, slow motion after tier changes / 显存：先释放后创建、驻留优先级、改档后的慢动作

- **Release before create.** Changing NR resolution, layers or HDR used to create the new generation of NR resources while the old one was still resident; on an RTX 4060 Laptop 30 worksets and 56 features were alive at once (111.9 % of the budget), and the game ran at about 28 fps for tens of seconds. Replaced resources are now marked, evaluates are declined (with the game's image passed through) until the GPU has proven it no longer uses them, and only then are the new ones created (bounded by 32 evaluates). Turning NR off releases its resources (D3D12 and Vulkan paths), and NGX features are created with `FreeMemOnReleaseFeature=1` so that NGX does not keep their memory cached.
  **先释放后创建。** 以前切换 NR 分辨率、层数或 HDR 时，新一代 NR 资源在旧一代仍驻留时就被创建；在 RTX 4060 Laptop 上曾同时存在 30 个工作集和 56 个特征（预算的 111.9%），游戏以约 28 fps 运行数十秒。现在被替换的资源会被标记，在 GPU 证明不再使用它们之前拒绝 evaluate（游戏画面原样通过），之后才创建新资源（最多等待 32 次 evaluate）。关闭 NR 会释放其资源（D3D12 与 Vulkan 两条路径），NGX 特征以 `FreeMemOnReleaseFeature=1` 创建，NGX 不再缓存这些内存。
- **Residency priority (pageable1, pageable2).** Generic adds `VK_EXT_memory_priority` and `VK_EXT_pageable_device_local_memory` to the game's `vkCreateDevice` when the driver offers them, and sets the priority by owner: the add-on's resources that are rebuilt with the NR resolution 0.10 (lowest class), its fixed resources 0.35, the game's DLSS/DLSS-G NGX allocations 0.70, everything else of the game 0.50 (ini-only keys `NRVkOwnMemoryPriority`, `NRVkOwnFixedMemoryPriority`, `NRVkHotMemoryPriority`; `NRVkPageableMemory=0` turns the extensions off). Under pressure Windows therefore moves the add-on's data out before the game's. An earlier self-heal that switched NR off and on when memory was demoted was removed; demotion is now only reported (`vram_demoted` notice, kernel-sampled demoted MiB). With two NR layers a tier change now applies to both layers in the same frame (previously the second waited 30 frames).
  **驻留优先级（pageable1、pageable2）。** 驱动支持时，Generic 在游戏的 `vkCreateDevice` 中加入 `VK_EXT_memory_priority` 和 `VK_EXT_pageable_device_local_memory`，并按归属设置优先级：插件随 NR 分辨率重建的资源 0.10（最低一类），插件的固定资源 0.35，游戏的 DLSS/DLSS-G NGX 分配 0.70，游戏的其余分配 0.50（仅 ini 键 `NRVkOwnMemoryPriority`、`NRVkOwnFixedMemoryPriority`、`NRVkHotMemoryPriority`；`NRVkPageableMemory=0` 关闭这两个扩展）。显存紧张时 Windows 因此先挪出插件的数据，再挪游戏的数据。此前在显存被降级时自动关开 NR 的“自愈”已删除；降级现在只提示（`vram_demoted` 提示，内核采样的降级 MiB 数）。两层 NR 时改档会在同一帧对两层生效（此前第二层要等 30 帧）。
- **Checked in game (RTX 4060 Laptop, pageable2).** In seven Quality↔Performance switches only the add-on's lowest class was ever demoted; the game's own allocations and the NGX allocations never were. The slow phase after a switch lasted from none to 16.5 s and always ended by itself (before the change: 15–17 s, with the game's data demoted). The user's report: "no anomalies, it returned to the ideal frame rate in about 10 seconds every time". Switching two layers at 2560×1600 to the Full resolution still exhausted system memory and hung the game; this is treated as an over-limit setting (install guide, step 17).
  **游戏内验证（RTX 4060 Laptop，pageable2）。** 在 7 次“质量↔性能”切换中，被降级的始终只有插件的最低一类；游戏自己的分配和 NGX 分配一次都没有被降级。切换后的慢段从没有到最长 16.5 秒，每次都能自行结束（改动前为 15～17 秒，且被挤出的是游戏数据）。用户反馈：“无异常，每次都在 10 秒左右恢复到了理想帧率”。两层 NR、2560×1600 下切到“原画”分辨率仍会耗尽系统内存并卡死游戏；这被视为超限设置（使用教程第 17 步）。
- **Windows VRAM budget switch.** On the DLSS 5 Bridge page. It sets `MaxVideoMemoryFragmentationBuffer` and `MaxProcessBudgetCapBuffer` under `HKLM\SYSTEM\CurrentControlSet\Control\GraphicsDrivers\MemoryManager` to 64 each, lowering the per-process reserve Windows keeps from about 768 MiB to 128 MiB (about 640 MiB more budget offline; in game a few hundred MiB on an 8 GB card). The original values are backed up in the game's state folder, LocalAppData and `%ProgramData%\dlss5-bridge\vram-budget`, written values are read back, a half-written change leaves neither option selected and keeps the original backup, and the full uninstall restores it. Administrator rights are obtained through a maintenance entry started with the system `rundll32`; because Endfield runs as administrator there is normally no separate UAC prompt. It takes effect after a Windows restart and affects all programs. Its effect on stutter was not measured separately.
  **Windows 显存预算开关。** 位于 DLSS 5 Bridge 页。它把 `HKLM\SYSTEM\CurrentControlSet\Control\GraphicsDrivers\MemoryManager` 下的 `MaxVideoMemoryFragmentationBuffer` 和 `MaxProcessBudgetCapBuffer` 各设为 64，使 Windows 为每个进程预留的显存从约 768 MiB 降到 128 MiB（离线计算约多 640 MiB 预算；游戏内在 8 GB 显卡上约为几百 MiB）。原值备份在游戏状态目录、LocalAppData 和 `%ProgramData%\dlss5-bridge\vram-budget`；写入后读回校验；写了一半失败时两个选项都不选中且保留原备份；完全卸载会还原。管理员权限通过系统 `rundll32` 启动的维护入口获得；终末地本身以管理员身份运行，通常不会另弹 UAC。重启 Windows 后生效，影响所有程序。它对卡顿的改善没有单独测量。
- **ReShade loads only enabled effects.** On first run the bridge sets `[GENERAL] SkipLoadingDisabledEffects=1` in `ReShade.ini`, once per file (marker `[DLSS5Bridge] SkipLoadingDisabledEffectsApplied=1`), so disabled effects are not compiled and do not hold video memory. Turning it off later is respected.
  **ReShade 只加载已启用的效果。** 首次运行时 Bridge 在 `ReShade.ini` 中设置 `[GENERAL] SkipLoadingDisabledEffects=1`，每个文件只设一次（标记 `[DLSS5Bridge] SkipLoadingDisabledEffectsApplied=1`），未启用的效果不再编译、不再占显存。之后手动关闭它会被尊重。

### 3.3 NR resolution for Render/Upscaled, focus and composition / 渲染、超分后层的 NR 分辨率，失焦与合成

- **NR resolution.** On Vulkan, the Render and Upscaled stages ignored the NR resolution setting. The model's working size is now separate from the game's output size; results are reconstructed to the output size with an edge-aware shader, and depth and motion vectors keep their own input grids. Checked offline (218 checks of the extracted functions plus 27 wiring checks; the new SPIR-V's bindings and layout were checked separately).
  **NR 分辨率。** Vulkan 下渲染层和超分后层以前不使用 NR 分辨率设置。现在模型工作尺寸与游戏输出尺寸分开，结果用边缘感知的着色器重建回输出尺寸，深度和运动矢量保持各自的输入网格。离线验证（提取函数 218 项 + 接线检查 27 项；新 SPIR-V 的绑定与布局单独核对）。
- **Focus.** Background FG retention applied only with a non-zero Present layer count; it now applies whenever any NR stage is enabled, and NR itself is not gated by ordinary focus loss. Minimised or hidden windows and a game that stops presenting are not ordinary focus loss.
  **失焦。** 以前只有呈现层数不为 0 时才在后台保持帧生成；现在只要任一 NR 阶段启用就保持，NR 本身也不因普通失焦而停。最小化、隐藏窗口或游戏停止提交画面不属于普通失焦。
- **Composition guard** (`HoldComposition`, default on) now covers every enabled NR stage, independent of focus retention. It keeps the game in DWM composition to stabilise display pacing; it does not claim that the underlying independent-flip timing issue is solved. Explicit `HoldComposition=0` / `FocusKeepFG=0` values are respected.
  **合成守护**（`HoldComposition`，默认开）现在覆盖所有启用的 NR 阶段，并与失焦保持相互独立。它让游戏保持 DWM 合成以稳定显示节奏，不代表独立翻转（iFlip）时序的底层问题已解决。已显式写入的 `HoldComposition=0` / `FocusKeepFG=0` 会被尊重。

### 3.4 MFG Unlock / MFG Unlock

- **`eWarnOutOfVRAM`.** On an RTX 4060 Laptop at 3x and above, the multiplier switched back and forth thousands of times per session, with DLSS-G history resets that showed as stutter and flicker. Streamline returns `sl::Result 39 = eWarnOutOfVRAM` as a warning on top of success whenever the process's usage reaches the Windows budget; MFG Unlock compared results with `== eOk` and treated it as a refusal, retried, and fell back to the game's multiplier. It now treats `eWarnOutOfVRAM` as accepted; the original result is still returned to the game and real errors still fall back.
  **`eWarnOutOfVRAM`。** 在 RTX 4060 Laptop 上 3 倍及以上时，倍率每局来回切换数千次，DLSS-G 历史反复重置，表现为卡顿和闪烁。只要进程用量达到 Windows 预算，Streamline 就会在成功结果上附加警告 `sl::Result 39 = eWarnOutOfVRAM`；MFG Unlock 用 `== eOk` 判断，把它当成拒绝，重试后回退到游戏的倍率。现在 `eWarnOutOfVRAM` 被视为已接受；返回给游戏的原值不变，真正的错误仍会回退。
- **Hooks installed twice.** Two threads could enter the hook installation at the same time; the second `DetourAttach` followed the first one's jump and hooked `slGetFeatureFunction` onto itself, which recursed until the stack overflowed at start-up (two crashes on the RTX 4060 Laptop, dump analysed). Installation is now serialised by a process-wide mutex, and a hook whose target already points to its replacement is refused with a warning.
  **钩子被装两次。** 两个线程可能同时进入钩子安装；第二次 `DetourAttach` 顺着第一次的跳转把 `slGetFeatureFunction` 挂到了自己身上，启动时无限递归直到栈溢出（RTX 4060 Laptop 上崩溃两次，已分析转储）。现在安装由进程级互斥锁串行化，目标已指向替换函数的钩子会被拒绝并记录警告。
- **Legacy software flip pacing** is now on by default for every GPU; an upgrade switches a saved "off" to "on" once (`LegacyPacingDefaultRevision=1`), later choices are respected.
  **Legacy 软件呈现节奏** 现在对所有显卡默认开启；升级时一次性把已保存的“关闭”改为“开启”（`LegacyPacingDefaultRevision=1`），之后尊重玩家的选择。
- **Panel.** Notices sit in a fixed two-line area; the two hints (in-game frame generation must stay on; choose a fixed 2x–4x, not "Auto") sit between "Advanced settings" and "Enable MFG Unlock" and wrap when the panel is narrow. In safe mode the page stays visible and explains how to leave safe mode.
  **面板。** 提示放在固定的两行提示区；两句说明（游戏内帧生成必须开启；选 2～4x 固定档位，不选“自动”）位于“高级设置”和“启用 MFG Unlock”之间，面板过窄时自动换行。安全模式下页面保持显示，并说明如何解除。
- **Checked.** Policy fixture 326 checks, start-up policy 10 checks.
  **验证。** 策略测试 326 项，启动策略 10 项。

### 3.5 Stability fixes / 稳定性修复

- **Alt+Z game filter crash.** Opening the NVIDIA overlay's game filter page crashed the game with a stack overflow (`0xC00000FD`): the bridge's `GetProcAddress` hook declared a 32,768-character local path buffer, and the NVIDIA overlay calls it on a driver thread with a 64 KiB stack. The common path no longer uses large stack buffers; only `slInit` resolution uses a heap buffer, and a failure keeps the original function. Similar buffers in Generic and the shared headers were moved off small-stack paths. Checked with fixtures on real 64 KiB threads (180,146 bridge checks; Generic fixture 30 checks).
  **Alt+Z 游戏滤镜页闪退。** 打开 NVIDIA 覆盖层的游戏滤镜页时游戏因栈溢出（`0xC00000FD`）闪退：Bridge 的 `GetProcAddress` 钩子声明了 32,768 字符的局部路径缓冲，而 NVIDIA 覆盖层在一个只有 64 KiB 栈的驱动线程上调用它。常见路径不再使用大栈缓冲，只有解析 `slInit` 时使用堆缓冲，失败时保留原函数。Generic 和共享头文件中类似的缓冲也移出了小栈路径。在真实的 64 KiB 线程上用测试验证（Bridge 180,146 项；Generic 测试 30 项）。
- **"NR runtime not installed".** A runtime-path query made before the GPU was identified was latched as "missing" for the session. It is now retried every 250 ms for up to 60 s, and an initialisation failure returns a definite failure code.
  **“未安装 NR 运行时”。** 在显卡识别完成前查询运行库路径时，结果会被整局记为“缺失”。现在每 250 ms 重试一次，最长 60 秒；初始化失败时返回明确的失败码。
- **Crash rollback and safe mode.** An orderly exit writes a clean-exit mark and, for a confirmed configuration, is no longer counted as a failed start (during a trial and in the first session after a game or driver update it still counts, because the game's crash reporter can end a crash with an orderly exit). A rollback resets the failure count. Status texts distinguish rollback from safe mode.
  **崩溃回退与安全模式。** 正常退出会写入“已正常退出”标记；对已确认的配置，不再计为启动失败（试用新选择期间和游戏或驱动更新后的第一局仍计入，因为游戏自带的崩溃报告可能让崩溃以正常退出结束）。回退后失败计数清零。状态文字区分“已回退”与“安全模式”。
- **Crash handler chain.** When another module (for example the game's crash reporter) replaces the top-level crash handler, the bridge takes it back (at most 8 times before the start-up health point) and still passes each crash on to every earlier handler, once each. Before passing on it checks that the handler's module is still loaded and is the same module; unreadable or erased module headers (packers, protectors) are handled without reading them. Checked offline (76 crash-chain checks).
  **崩溃处理链。** 当其他模块（例如游戏的崩溃报告）替换了顶层崩溃处理程序时，Bridge 会重新接管（启动健康点前最多 8 次），并仍把每次崩溃交给此前的每个处理程序，各一次。转交前会确认该处理程序所在模块仍已加载且是同一模块；对不可读或被抹掉的模块头（加壳、保护程序）不读取头部。离线验证（崩溃链 76 项）。

### 3.6 Hidden DLSS on RTX 2050 / RTX 3050 Laptop / RTX 3050 Ti Laptop / RTX 2050、RTX 3050 Laptop、RTX 3050 Ti Laptop 上被隐藏的 DLSS

In Vulkan mode the game creates its device without Streamline when the GPU name contains `RTX 2050`, `RTX 3050 Ti Laptop` or `RTX 3050 Laptop`, so DLSS, DLSS-G and Reflex are missing. Generic changes one character of the Vulkan GPU name the game reads (`Laptop` → `laptop`; `RTX 2050` → `RTX 205O`). Only `Endfield.exe` is affected; not in safe mode, with `EnableHooks=0`, or with `NRVkDeviceNameUnlock=0` under `[RenoDX.DLSS5]`. Other code of the game sees the changed name too: the 3050 models are still identified as 3050, the RTX 2050 as "205", so its default quality preset may differ. Checked: 15 fixture cases; in game on an RTX 3050 Laptop (4 GB) with a diagnostic add-on applying the same change, DLSS appeared and could be enabled; the RTX 3050 Laptop rows of the frame-rate table were measured with this release. RTX 3050 Ti Laptop and RTX 2050 were not tested.

游戏在 Vulkan 模式下，只要显卡名称含 `RTX 2050`、`RTX 3050 Ti Laptop` 或 `RTX 3050 Laptop`，创建设备时就不经过 Streamline，DLSS、DLSS-G 和 Reflex 因此缺失。Generic 在游戏读取 Vulkan 显卡名称时改动一个字符（`Laptop` → `laptop`；`RTX 2050` → `RTX 205O`）。只对 `Endfield.exe` 生效；安全模式、`EnableHooks=0` 或 `[RenoDX.DLSS5]` 中 `NRVkDeviceNameUnlock=0` 时不改。游戏中其他读取名称的地方也会看到改后的名称：3050 系列仍被识别为 3050，RTX 2050 被识别为“205”，其默认画质可能不同。验证：测试 15 例；在 RTX 3050 Laptop（4 GB）上用同一改法的诊断插件实测，DLSS 出现并可开启；帧率表中 RTX 3050 Laptop 的数据是用本版测得的。RTX 3050 Ti Laptop 和 RTX 2050 未测试。

### 3.7 Panels, defaults and screenshots / 面板、默认值与截图

- The DLSS 5 Neural Rendering page was reduced: the welcome card, preset rows, diagnostics section, issue cards and report function, and the version footer were removed. The Render row, layers 2–4, HDR paper white, Keep HUD, Generated frames, Upscaling, Detail stability and the Stabilize options are under "Advanced settings". With the overlay closed, the top-left corner shows only "Starting..." and a fading "Neural Rendering is active".
  DLSS 5 Neural Rendering 页精简：删除欢迎卡、预设行、诊断区、问题卡片与反馈功能、版本行。渲染层一行、2～4 层、HDR 纸白、保留 HUD、生成帧、上采样、细节稳定以及稳定类选项放入“高级设置”。不开面板时左上角只显示“正在启动...”和渐隐的“Neural Rendering 已启用”。
- Notices on all three pages sit in a fixed-size area with "<" ">" paging, so they never move the controls below. After an NR resolution tier change a "temporary stutter possible" page is shown for 20 seconds. VRAM notices need 10 s over the budget to appear and 8 s below 95 % to clear; spilling into system memory and exceeding the budget have separate texts. Three VRAM notice translations that had turned into question marks in all 14 languages were fixed, and a build step now checks every language file.
  三个页面的提示都放在固定大小、可用“<”“>”翻页的提示区，不会挤动下方控件。切换 NR 分辨率档位后显示 20 秒“可能出现暂时卡顿”。显存提示需超出预算 10 秒才出现、低于 95% 持续 8 秒才消失；“溢出到系统内存”和“超出预算”分为两条文字。14 种语言中三条显存提示曾变成问号乱码，已修正，构建时会检查所有语言文件。
- New first-install defaults (only where nothing is saved): Render 0, Upscaled 0, Present 1, resolution Performance; Present layer 1 strength 2.00, style Cinematic, local tone 1.50, structure 2.00, character/skin 0 with character mask, independent skin and UI correction on. The "Adjust NR's look" switch and all Result / Limits values are back to their identity defaults (switch off, gains 1.00, limits 0.00, detail radius 1 %, halo 0.00), so the switch compares the image with and without the adjustments.
  新的首装默认值（仅用于未保存的项）：渲染 0、超分后 0、呈现 1，分辨率“性能”；呈现第 1 层强度 2.00、风格“电影感”、局部色调 1.50、结构 2.00、角色/皮肤 0，角色遮罩、独立皮肤、UI 矫正开启。“调整 NR 的外观”开关以及“结果”/“限制”的全部参数恢复为恒等默认值（开关关闭、增益 1.00、限制 0.00、细节半径 1%、暗晕 0.00），用该开关即可对比调整前后的画面。
- Screenshots: one unprocessed PNG of the final image after all NR layers per key press; HDR stays HDR; folder field with a folder picker; PrintScreen works as key (a wrongly saved value from earlier builds is migrated), and a press made while the overlay is open no longer triggers an extra capture or NR toggle after it closes. The add-on sets ReShade's `[INPUT] KeyScreenshot` to 0 at start. The screenshot does not include other ReShade effects or the overlay.
  截图：每次按键保存一张经全部 NR 层处理后的最终画面 PNG，未经二次处理；HDR 保持 HDR；保存位置可输入或用文件夹按钮选择；PrintScreen 可作按键（早期构建保存的错误键值会自动迁移），菜单打开期间按下的键不会在关闭菜单后多截一张或多切一次 NR。插件启动时把 ReShade 的 `[INPUT] KeyScreenshot` 设为 0。截图不包含其他 ReShade 特效和菜单。
- Opening the ReShade overlay while the game has the cursor locked keeps the cursor inside the game window and restores the game's lock on close (`NROverlayCursorConfine=0` under `[RenoDX.DLSS5]` turns this off).
  游戏锁定鼠标时打开 ReShade 菜单，鼠标会被限制在游戏窗口内，关闭菜单后恢复游戏原来的锁定（`[RenoDX.DLSS5]` 中 `NROverlayCursorConfine=0` 可关闭）。
- DLSS 5 Bridge page: Deployment and NR hook mode keep only their options; safe mode, diagnostics export and driver settings are under Advanced settings (safe mode is shown without it while active); the Windows VRAM budget is an On/Off choice.
  DLSS 5 Bridge 页：“统一部署”和“NR 钩子模式”只保留选项；安全模式、导出诊断和驱动设置位于“高级设置”中（处于安全模式时不勾选也会显示）；“Windows 显存预算”为开启/关闭二选一。

### 3.8 Uninstaller / 卸载工具

- `DLSS5-Recovery 故障恢复.cmd` is now an ASCII-only one-line launcher with no jumps; it runs `DLSS5-Clean-Recovery.ps1 -Action Uninstall`, which explains the action and runs only after Y. The old numbered menu was removed; on some PCs `cmd.exe` jumped to the wrong label in the UTF-8 batch file and option A disabled the add-ons instead. Safe mode, driver-setting restore, diagnostics export and the VRAM budget are handled on the in-game DLSS 5 Bridge page.
  `DLSS5-Recovery 故障恢复.cmd` 现在是只含 ASCII 字符、没有跳转的单行启动器，运行 `DLSS5-Clean-Recovery.ps1 -Action Uninstall`，先说明操作，输入 Y 后才执行。旧的数字菜单已删除；在部分电脑上 `cmd.exe` 会在含中文的 UTF-8 批处理中跳到错误的标签，选 A 时反而执行了停用插件。安全模式、驱动设置还原、诊断导出和显存预算改在游戏内 DLSS 5 Bridge 页处理。
- The full uninstall disables the ReShade Vulkan layer registration, restores the journalled driver values and the Windows VRAM budget, and moves the add-ons, the `dlss5` folder, the package's settings and logs, and finally the uninstaller itself into `DLSS5-Recovery-Backups\<time-id>\` next to the game folder, with hash checks. If the driver settings or the VRAM budget were not fully restored, it says so (exit code 9), shows the restore helper's messages, and keeps the tools in place for another run. The restore helper reports each failure separately (new result code 8 when both restores failed).
  完全卸载会停用 ReShade 的 Vulkan 层注册，还原记录过的驱动设置值和 Windows 显存预算，并把插件、`dlss5` 文件夹、本包的配置与日志，最后连同卸载工具本身，一起移入游戏目录旁的 `DLSS5-Recovery-Backups\<时间编号>\`，并做哈希校验。驱动设置或显存预算未完全还原时会明确说明（退出码 9），显示恢复助手的输出，并保留工具以便再次运行。恢复助手会分别报告每种失败（两项都失败时为新增的结果码 8）。
- Checked offline with `test-clean-recovery.ps1` (stand-in registry, process and driver functions). No real uninstall with this script on a game installation is recorded in the project's logs.
  离线验证：`test-clean-recovery.ps1`（替身注册表、进程和驱动函数）。项目日志中没有用此脚本对真实游戏安装执行卸载的记录。

### 3.9 Logs / 日志

New log coverage, all bounded: the bridge's `[vram-residency]` lines (process local/non-local/committed memory from the GPU performance counters, on changes); Generic's `Vulkan NR recovery N` (memory and present rate in the 30 s after each NR rebuild), demotion class transitions, NR release and replacement-wait lines, and `prepresent[...]` / `vkmem[...]` telemetry groups; `[deployment] Windows VRAM budget: kind=...` at each start; `Vulkan device-name unlock` lines. Log locations and the anomaly log are unchanged (`dlss5\dlss5_log\`, see the usage document in the package).

新增的日志覆盖面均有上限：Bridge 的 `[vram-residency]` 行（从 GPU 性能计数器读取进程的本地/非本地/提交内存，变化时记录）；Generic 的 `Vulkan NR recovery N`（每次 NR 重建后 30 秒内的显存与上屏速率）、降级类别变化、NR 释放与替换等待行，以及 `prepresent[...]` / `vkmem[...]` 遥测组；每次启动的 `[deployment] Windows VRAM budget: kind=...`；`Vulkan device-name unlock` 行。日志位置与异常日志不变（`dlss5\dlss5_log\`，见包内使用说明）。

## 4. Limitations and open issues / 限制与未解决问题

- **Verification coverage.** The changes were checked by offline fixtures and in game mainly on an RTX 4060 Laptop; the other GPUs have the author's frame-rate measurements but no detailed log review in this document. Not tested: RTX 3050 Ti Laptop and RTX 2050 name unlock; a real uninstall with this release's script; a real crash passing through the new crash-handler chain logic; whether ReShade reads `SkipLoadingDisabledEffects` before its first effect load; a short stutter when the "Adjust NR's look" switch first allocates its stage. Only Arknights: Endfield on Vulkan was tested.
  **验证范围。** 改动主要通过离线测试，以及在 RTX 4060 Laptop 上的游戏内检查验证；其他显卡有作者的帧率实测，但本文档未对其日志做详细分析。未测试：RTX 3050 Ti Laptop 和 RTX 2050 的名称解锁；用本版脚本对真实安装执行卸载；真实崩溃经过新的崩溃链逻辑；ReShade 是否在首次加载效果前读到 `SkipLoadingDisabledEffects`；“调整 NR 的外观”开关首次分配资源时是否短暂卡顿。只测试了 Vulkan 下的《明日方舟：终末地》。
- **Slow motion below about 15 real fps** (install guide, known issue 5). There is no dedicated log entry for it yet; it can be derived from the telemetry.
  **真实帧率低于约 15 帧/秒时的慢动作**（使用教程已知问题第 5 条）。目前没有专门的日志记录，只能从遥测推算。
- **Changes of tier or layers on small cards** still cause a slow phase of about 10–16 s; its cause after the add-on's data is released is not proven. Two layers at the Full resolution on 8 GB cards can exhaust system memory.
  **小显存显卡上切换档位或层数** 仍会出现约 10～16 秒的慢段；插件数据释放后为何仍持续，尚未证实。8 GB 显卡上两层“原画”分辨率可能耗尽系统内存。
- **Missing DLSS option on other RTX cards** (install guide, step 15) is not explained.
  **其他 RTX 显卡缺少 DLSS 选项**（使用教程第 15 步）的原因未查明。
- **Occasional drops to 30–50 fps on one RTX 4070 PC** were traced to the game's main thread loading VFS resources synchronously; not caused by the add-on and not fixable in it.
  **一台 RTX 4070 电脑上偶发跌到 30～50 帧**，已查明是游戏主线程同步加载 VFS 资源，与插件无关，插件侧无法修复。
- **NR runtime for RTX 20/30/40** is a community-modified, unsigned file; NVIDIA may block or change this path in future drivers. The driver minimum 615.00 is what the runtime declares; the real lower limit is unknown.
  **RTX 20/30/40 的 NR 运行库** 是社区修改的未签名文件，NVIDIA 将来的驱动可能封堵或改变这条路径。驱动最低 615.00 是运行库声明的值，实际下限未知。
- **Future game updates.** Only the known `sl.interposer.dll` builds are accepted; with another build the game's own Streamline is kept until a new release adds it.
  **未来的游戏更新。** 只接受已知的 `sl.interposer.dll` 版本；遇到其他版本时保留游戏自带的 Streamline，直到新版加入支持。
- **Not supported:** NVIDIA Smooth Motion, NVIDIA game filters, RTX HDR, RTX Dynamic Vibrance, `EnableHooks=2` for the Present path.
  **不支持：** NVIDIA Smooth Motion、NVIDIA 游戏滤镜、RTX HDR、RTX Dynamic Vibrance，以及呈现路径下的 `EnableHooks=2`。
- **Rebuilding** needs Visual Studio 2022 Build Tools, the NGX SDK headers and, for Generic, a RenoDX `external` folder whose exact commit was not recorded; rebuilt DLLs differ in hash because of time stamps ([BUILDING.md](https://github.com/Yukikaze20170315/endfield-dlss5-vulkan-plugin/blob/main/BUILDING.md)).
  **重新构建** 需要 Visual Studio 2022 Build Tools、NGX SDK 头文件，以及（Generic）一份未记录确切提交的 RenoDX `external` 文件夹；由于时间戳，重建的 DLL 哈希不同（[BUILDING.md](https://github.com/Yukikaze20170315/endfield-dlss5-vulkan-plugin/blob/main/BUILDING.md)）。

## 5. Files / 文件

### 5.1 Release assets / 发布附件

| Asset / 附件 | Bytes / 字节 | SHA-256 | Content / 内容 |
| --- | ---: | --- | --- |
| `unified-20261103l-candidate.zip` | 280,391,058 | `a4bc27f82fb83433fd5bf7243e2b598449b98e79d8f8a5acb3516f012d322985` | the ready-to-use package / 成品包 |
| `unified-20261103l-source.zip` | 20,629,528 | `727f0a0ed6f10343191420bcd638bc4ec3bfc8248b923433f0353f04e0d2e61d` | the complete source, same as the repository's `src/`, `tools/`, `upstream/` and `BUILDING.md` / 完整源码，与仓库中的 `src/`、`tools/`、`upstream/` 和 `BUILDING.md` 相同 |
| `SHA256SUMS.txt` | — | — | SHA-256 of both archives / 两个压缩包的 SHA-256 |

### 5.2 Repository content / 仓库内容

| Path / 路径 | Content / 内容 |
| --- | --- |
| `README.md` | this document / 本文档 |
| `BUILDING.md` | requirements, build, offline tests, packaging / 构建条件、构建、离线测试、打包 |
| `src/unified-20261011/` | production source of the bridge, Generic, MFG Unlock and the shared headers / Bridge、Generic、MFG Unlock 与共享头文件的生产源码 |
| `tools/unified-20261011/` | build, test and packaging scripts, uninstaller, restore helper source / 构建、测试、打包脚本，卸载工具，恢复助手源码 |
| `upstream/NVIDIA-nvapi-87dca62/` | NVAPI SDK |
| `tools/release-unified-20261103l/` | the script that staged this release and its secret scan / 生成本次发布的脚本及敏感信息扫描 |
| `docs/USAGE-AND-RECOVERY.md` | the bilingual technical document from the package / 包内的双语技术文档 |
| `docs/history/` | the READMEs of `unified-20261015q` and `vk-fgrelay-20261005` / 前两次发布的 README |

The merge kits, patches, evidence and tools of earlier releases were removed from the current tree because they were made for pull requests to the upstream projects. They remain available in the git history and under the tags `unified-20261015q`, `vk-fgrelay-20261005`, `vk-fgrelay-20261002` and `vk-fgrelay-20260929`. `.gitattributes` disables line-ending conversion for the project's own files; the bundled ImGui folder keeps its own `.gitattributes`. `unified-20261103l-source.zip` is the byte-exact copy of the build tree.

此前各版本的合并补丁包、补丁、证据和工具是为向上游项目提交 PR 而准备的，已从当前文件树中移除；它们仍保存在 git 历史和 `unified-20261015q`、`vk-fgrelay-20261005`、`vk-fgrelay-20261002`、`vk-fgrelay-20260929` 这几个标签中。`.gitattributes` 对本项目自己的文件关闭了换行转换；随附的 ImGui 文件夹保留其自带的 `.gitattributes`。`unified-20261103l-source.zip` 是构建树的逐字节副本。

## 6. Licenses and acknowledgements / 许可与致谢

The bridge (based on NIGos/dlss5-bridge), Generic (RenoDX) and MFG Unlock (nefh/MFGAmpereUnlock-RenoDx, which builds on mavismmg/MFGAdaUnlock-RenoDx) are MIT licensed; their license files are kept in `src/unified-20261011/`. The tree also contains MinHook, Detours, the ReShade add-on headers with ImGui, the NVIDIA Streamline headers and the NVAPI SDK, each with the license notice it came with. It does not contain the NVIDIA NGX SDK headers, which carry a proprietary notice. The NR runtimes, the Streamline plugins and the NGX snippets in the package are NVIDIA binaries and remain subject to NVIDIA's terms; the RTX 20–40 NR runtime is a community modification of an NVIDIA binary.

Bridge（基于 NIGos/dlss5-bridge）、Generic（RenoDX）和 MFG Unlock（nefh/MFGAmpereUnlock-RenoDx，其基于 mavismmg/MFGAdaUnlock-RenoDx）均为 MIT 许可，许可文件保留在 `src/unified-20261011/` 中。源码树中还包含 MinHook、Detours、带 ImGui 的 ReShade 插件头文件、NVIDIA Streamline 头文件和 NVAPI SDK，各自附带原有的许可声明。源码树不含带专有声明的 NVIDIA NGX SDK 头文件。包内的 NR 运行库、Streamline 插件和 NGX 组件是 NVIDIA 的二进制文件，仍受 NVIDIA 条款约束；RTX 20～40 的 NR 运行库是社区对 NVIDIA 二进制文件的修改版。

Thanks to RenoDX and the RenoDX DLSS 5 Generic authors; NIGos for dlss5-bridge; nefh and mavismmg for MFG Unlock; the RHI project for hosting the NR runtimes; the internal testers and the owners of the test PCs.

感谢 RenoDX 及 RenoDX DLSS 5 Generic 的作者；NIGos 的 dlss5-bridge；nefh 和 mavismmg 的 MFG Unlock；托管 NR 运行库的 RHI 项目；以及内测用户和测试电脑的机主。
