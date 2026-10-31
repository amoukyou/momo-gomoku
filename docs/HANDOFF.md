# 墨墨五子棋：百词斩学习机改造交接文档

更新于 2026-10-31。给接手开发的人和编程 AI（Codex 等）看：先读这份，再读 `AGENTS.md` 和 `NOTES.md`。

## 现状速览

五子棋"内存试玩版"已在真机上跑通；设备的 flash 一个字节都没改过，原厂百词斩系统完好。

- **能用的**：墨墨开机动画 → 主菜单（人机简单 / 普通 / 困难、双人对战、从头开始、关机）→ 五子棋，AI 三档，悔棋、暂停菜单都正常。
- **试玩版的本质**：程序通过 USB 放进芯片内存（SRAM）里运行，复位或关机就消失，回到原厂系统。每次玩都要连电脑加载一次。
- **还没做**：正式版（烧进 flash、开机直接进游戏）、真正的关机和锁屏、侧面音量键（要读 ADC）。锁屏和关机动画已经画好（`boot_anim.c` 里的 `sleep_anim_frames` / `off_frames`），还没接进程序。
- **备份**：原厂 16 MB flash 已完整备份，但**不在仓库里**（含原厂代码和设备个人信息），在原来那台 Mac 上另存。

## 上手

```bash
git clone --recursive https://github.com/amoukyou/momo-gomoku.git
cd momo-gomoku
sh jlmac/setup.sh                                                   # Python 环境 + libusb
swift game/tools/mkfont.swift game/src/font_data.c game/src/app.c   # 生成字库（苹方字体不能公开分发，所以不在仓库里）
```

## 硬件档案

设备是百词斩单词机 Ace-1S。引脚全部从原厂固件反推，并在真机上验证过。

| 部件 | 型号 / 参数 |
| --- | --- |
| 主控 | 杰理 AC7916AB（内部代号 WL82，AC791N 系列），Wi-Fi + 蓝牙，320 MHz，8 MB SDRAM |
| Flash | GigaDevice 16 MB SPI NOR（JEDEC 0xC84018），放固件 |
| SD NAND | 4 GB，FAT 文件系统，原厂用来放词书 |
| 屏幕 | 2.7 寸墨水屏 264×176，控制器 SSD1680 |
| 其他 | 功放 NS4150B，充电 TP4056，电池 750 mAh |
| 芯片密钥 chipkey | 每颗芯片不同；这台是 0xF181（解密 / 加密固件要用，下载工具的快速信息里会显示） |
| 原厂版本 | AC7916AB_1V5.8.1 |

| 功能 | 引脚 | 备注 |
| --- | --- | --- |
| 屏幕 SCK / MOSI | PC09 / PC10 | 硬件 SPI1 的 B 组；试玩版用 GPIO 模拟 |
| 屏幕 CS / DC / RST | PC06 / PC07 / PC08 | |
| 屏幕 BUSY | PC05 | 高电平 = 忙 |
| 十字键 上 / 下 / 左 / 右 | PC01 / PA07 / PH03 / PC00 | 低电平有效，开内部上拉 |
| 大圆键（确认，也是开机键） | PA08 | 低电平有效 |
| 小圆键 | PA10 | 低电平有效 |
| 顶部键 / 侧面音量+ / 音量− | PB01 | 一根 ADC 线靠电压区分：约 0 / 588 / 693（满量程 1023），外部上拉；顶部键会拉到 0 V，可当数字输入读 |
| 调试串口 TX | PH06 | 921600 波特率，还没用上 |

- **画面方向**：屏幕是装反的，要水平、垂直都翻转（`EPD_FLIP_X | EPD_FLIP_Y`，即运行参数 3）。
- **GPIO 寄存器**：端口 A–H 依次在 0x50000 + 0x40 × 端口号；偏移 OUT 0x00、IN 0x04、DIR 0x08（1 = 输入）、DIE 0x0C、PU 0x10、PD 0x14。
- **看门狗**：通过 P33 总线（CON 0x13E08 / BUF 0x13E0C）往寄存器 0x80 或上 0x40 就是喂狗。
- **软复位**：`*(volatile uint32_t *)0x10000 |= 1 << 4`，芯片重启并从 flash 启动。

## 开发环境

编译在 Linux 虚拟机里做（杰理编译器只有 Linux / Windows 版），USB 加载和备份在 Mac 上做。

- **虚拟机**：OrbStack 里的 Ubuntu 22.04 x86_64 机器 `jlbuild`。执行命令用 `orb -m jlbuild bash -c '...'`；虚拟机里能用同样的路径访问 Mac 的文件。
- **编译器**：虚拟机里的 `/opt/jieli`（杰理官方 jieli-linux-toolchains-20260730.1，clang 4.0.1，目标 pi32v2）。
- **SDK**：虚拟机里的 `~/sdk`，即杰理 fw-AC79_AIoT_SDK 的 release/AC79NN_SDK_V1.2.0 分支，提交 0617f4cc09685332aaf1104893718291c54ee5bb。
- **换一台 Mac 时**：装 OrbStack 并建机器（`orb create --arch amd64 ubuntu:jammy jlbuild`）；从 [杰理工具链下载页](https://pkgman.jieliapp.com/s/linux-toolchain) 下载编译器，解压到虚拟机的 `/opt/jieli`；克隆 SDK 并核对上面的提交哈希（Gitee 太慢可用 GitHub 镜像 HQWYFHJQ/fw-AC79_AIoT_SDK）。

## 怎么跑试玩版

改代码 → 编译 → 加载，全程不写 flash。命令都在仓库根目录执行。

1. **改了界面文字时**：重新生成字库（命令见"上手"）。漏了这步，新字会显示成问号。
2. **先在 Mac 上看效果**（可选）：编译 `game/sim/render.c`，按键脚本会导出每一帧，再用 `game/tools/raw2png.py` 转成 PNG 查看。
3. **编译真机版**：`orb -m jlbuild bash -c "sh '$PWD/game/ramrun/build.sh'"`，生成 `game/build/ramrun/ramrun.bin`。
4. **设备开机并插 USB**：长按大圆键 3 秒。原厂系统闲置会自动关机、从 USB 上消失，关了就再开。
5. **加载**：`sudo ./jlmac/ramrun.sh 3`（参数 3 = 屏幕旋转 180°），输入 Mac 密码。脚本会把原厂系统切进 ROM 下载模式，把程序写进 SRAM 0x1C02000 并跳过去运行。终端最后报 "Something failed … timed out" 是正常的：程序不会返回。
6. **玩**：方向键移动，大圆键确认 / 落子，小圆键悔棋，顶部键打开暂停菜单；长按大圆键或小圆键约 1 秒也能打开菜单。
7. **退出**：主菜单选"关机"，芯片软复位回原厂系统；卡住就捅侧面针孔复位键。

重新备份整片 flash（只读，安全）用 `sudo ./jlmac/backup.sh`。

## 踩过的坑

最容易再踩的是前三个：程序返回 ROM、忘记喂狗、按键认错。

| 坑 | 现象 | 原因 | 正确做法 |
| --- | --- | --- | --- |
| 程序 return 回 ROM | 设备还能被电脑识别，但任何命令都超时；长按电源键也无效 | 我们占用的内存覆盖了 ROM 的 USB 状态 | 试玩版绝不返回 ROM，退出一律软复位；已经卡住就捅针孔复位键 |
| 忘记喂看门狗 | 屏幕刚开始刷新就"一闪而过"，又变回原厂画面 | ROM 模式下看门狗开着，约 2 秒不喂就复位 | 所有长循环里喂狗（`hal_delay_ms` 已包含） |
| 按键认错 | 有一版进不了游戏，连"关机"都选不了 | 把 PB01 当成了大圆键；原厂提示"长按 ○ 键开机"的 ○ 是大圆键 PA08 | 以硬件表为准；改按键后先确认确认键能用 |
| 侧面音量键没反应 | 试玩版里按了无效 | 它们和顶部键共用 PB01，靠电压区分，必须用 ADC 读 | 正式版用 SDK 的 ADC 按键驱动 |
| 画面倒置 | 标题跑到底部、文字倒着 | 屏幕装反了 | 运行参数用 3（X、Y 都翻转） |
| 按键不灵敏 | 要重按才生效 | 刷新屏幕的那 1 秒里不读按键 | 按键缓冲 + 防抖，刷新中也采样（`hal_idle`）；连按合并成一次刷新 |
| 结构体对齐错位 | 推出来 DC 和 BUSY 都是 PA00，不可能 | 原厂的屏幕引脚结构体比当前 SDK 多一个字段 | 以反汇编里代码实际读的偏移为准 |
| macOS 不支持 | jl-uboot-tool 在 Mac 上直接报错 | 原工具只有 Linux / Windows 后端 | 用我们的 libusb 后端，必须 sudo（要卸掉系统的 U 盘驱动） |
| sudo 脚本停不下来 | 按 Ctrl-C 无效 | 工具重试连接时会吞掉 Ctrl-C | 用 control + \（会弹"Python 意外退出"，点"好"），再拔插 USB 释放设备 |
| 设备从 USB 上消失 | 脚本一直在等 | 原厂系统闲置自动关机 | 重新长按大圆键开机，脚本会自动接管 |
| Gitee 下载 SDK | 20 KB/s，下到 7% 断线 | 服务器太慢 | 用 GitHub 镜像，核对提交哈希与官方一致 |
| SDK 链接 / 打包报错 | sdk.elf 偶尔没生成；download.sh 报找不到 strip-ini、host-client | 打包脚本依赖杰理内部工具 | 链接失败就手动执行 lto-wrapper 那条命令；打包用我们自己的流程 |
| 杰理 objcopy | `-O binary` 直接报错 | 它要求用 `-j` 指定段 | 按段导出再按地址拼接（`build.sh` 已处理） |
| 裸机环境没有 libc | `snprintf`、`string.h` 报错 | 试玩版不链接 SDK | 用工具链的头文件，自己实现 `memset` 等（`mini_libc.c`），不用 stdio |
| 写内存要加密 | 程序写进去变成乱码 | 这颗芯片的 ROM 会先解密再写 | jlrunner 带 `--encrypt`（`ramrun.sh` 已带） |

## 安全须知与救砖

只要不写 flash，怎么折腾都能恢复；写 flash 之前必须有备份和保险。

- **针孔复位键**：机身侧面针孔里的白色小键。用取卡针按住 2–3 秒，芯片彻底复位并从 flash 启动；再长按大圆键 3 秒开机。试玩版怎么卡死都能靠它救回来。
- **内存试玩版是零风险的**：不写 flash，断电或复位就回到原厂系统。
- **不要用 jl-uboot-tool 的 `burnchipkey`**：芯片密钥烧在 eFuse 里，写了无法恢复。
- **不要随便用 `erase`、`erasechip`、`write`**，尤其是 flash 的 0x0000–0x6000（flash 头、引导程序、启动配置），写坏了芯片就启动不了。
- **不要让试玩程序 return 回 ROM**（见上节）。

烧录正式版的风险只有一种：新固件一启动就崩溃，连 USB 都没初始化。那时 USB 刷机通道会断，只能用"USB_KEY"办法救：开机瞬间用一块小开发板（比如树莓派 Pico）往 USB 的 D+/D− 上发密钥 0x16EF，强制芯片进入下载模式。所以：

- [ ] 烧录前先买好一块 Pico 当保险
- [ ] 原封不动保留原厂引导程序和启动配置，只替换应用程序
- [ ] 新固件开机第一件事就启动 USB 升级功能
- [ ] 加"安全模式"：开机时按住小圆键，就只开 USB、不进游戏

## 固件格式与自己打包

所有字段和校验都已用原厂备份验证过，打包新固件时只替换应用程序、重算下表里的校验即可。

| flash 偏移 | 内容 | 加密方式 |
| --- | --- | --- |
| 0x000000 | flash 头 32 字节，产品名 "AC791N_STORY" | ENC，密钥 0xFFFF |
| 0x000020 | 顶层文件表：uboot.boot_debug @0xC0、isd_config.ini @0x54A4、app_dir_head @0x6020、app_dir_head2 @0x7F5020（OTA 第二分区，空）、key_mac @0xFEF000 | 每条 32 字节各自 ENC 0xFFFF |
| 0x006000 | 应用目录头：[头 CRC][应用区明文 CRC16][应用区密文 CRC16][0x003B][应用区长度] | ENC 0xFFFF |
| 0x006020 | 应用区：app_area_head → app.lz4（相对 0x120）→ cfg_tool.bin；另有 VM @0xFE4000、BTIF @0xFEC000、PROFILE @0xFED000 的条目；原厂字体图片资源一直到 0x480FE4 | SFC：每 32 字节一块，密钥 = chipkey ^ ((地址 − 0x6020) >> 2) |

- **ENC 算法**：16 位密钥，每个字节 `b ^= key & 0xFF`，然后 `key = (key << 1) ^ (key & 0x8000 ? 0x1021 : 0)`。
- **CRC16**：多项式 0x1021，初值 0（XMODEM）。条目头 CRC 算 [2:32]；app.lz4 和 cfg_tool.bin 的数据 CRC 算明文；app_area_head 的 CRC 算明文从相对 0x20 到 cfg_tool 末尾。
- **app.lz4**：16 字节头 [0, 0, 0x4000, 解压后大小] + 一整段标准 LZ4 block 流（不分块），引导程序把它解压到 SDRAM 0x4000120 运行。第三个字段 0x4000 的含义没搞清，照抄。
- **app.bin 怎么来**：SDK 的 sdk.elf 里 .text + .data + .dynamic_data + .ram0_data + .cache_ram_data 按顺序拼接。SDK 工程必须用 SDRAM 配置（不要定义 `CONFIG_NO_SDRAM_ENABLE`），.text 才会链接到 0x4000120。
- **原厂应用**是用杰理 SDK 的 wifi_story_machine 工程改的，界面用 LVGL；SDK 里的 `board_7916A.c` 和 `board_7916AB_cfg.h` 就是同型号芯片的板级配置。
- **参考脚本**：`analysis/ents.py`（文件表）、`app2.py`（应用区条目）、`decrypt.py`（解密）、`unlz4.py`（解压）。

## 下一步：正式版

正式版要基于杰理 SDK 编译完整固件，烧进 flash 后开机直接进墨墨五子棋；目前只完成了前期调研，还没开始写。

- [ ] 新建 SDK 工程：参考 `apps/demo/demo_hello` 和 `wifi_story_machine/board/wl82/board_7916A.c`，SDRAM 8 MB、flash 16 MB
- [ ] USB 从机 + U 盘升级（`TCFG_PC_UPDATE`，`apps/common/usb/device/msd_upgrade.c`），开机最先初始化，保证以后能用 USB 重刷
- [ ] 安全模式：开机按住小圆键只开 USB
- [ ] 按键：IO 键 + PB01 上的 ADC 键（顶部键、音量±）
- [ ] 屏幕：复用 `epd_ssd1680.c`，硬件层换成 SDK 的 GPIO 或硬件 SPI1
- [ ] 关机 / 锁屏：SDK 的软关机 + 唤醒脚（大圆键 PA08，待从原厂固件确认唤醒配置），接入已画好的锁屏和关机动画
- [ ] 打包器：LZ4 压缩 + 按上节格式生成应用区，只写 0x6000 起的扇区，并擦掉 VM 设置区
- [ ] 准备好 Pico 保险，人工确认后再烧

还没搞清的问题：引导程序会不会在启动时校验整个应用区的 CRC（打包时照样算对即可）。

## 给编程 AI 的交代

- **先读**：`AGENTS.md`、`NOTES.md`、`game/src/hal.h`、`game/src/app.c`、`game/ramrun/main.c`、`jlmac/ramrun.sh`。
- **分工**：编译可以让 AI 自己跑；带 `sudo` 的加载和备份命令由人在终端里自己输密码执行。
- **节奏**：先在 Mac 上用 `game/sim` 验证界面和 AI，再上真机。
- **雷区**：不写 flash、不烧 chipkey、试玩程序不 return 回 ROM、长循环要喂狗、改界面文字后重新生成字库、屏幕方向参数用 3、按键以硬件表为准。

可以直接复制给 AI 的开场白：

```text
这是一个把百词斩 Ace-1S 墨水屏单词机（杰理 AC7916AB）改造成五子棋游戏机的项目：
https://github.com/amoukyou/momo-gomoku
先读 docs/HANDOFF.md、AGENTS.md 和 NOTES.md，里面有已验证的引脚、固件格式和踩过的坑。
现在要做：<写你的需求>。
规矩：只改内存试玩版，不要写 flash，不要运行 jl-uboot-tool 的 erase / write / burnchipkey；
需要 sudo 的命令写出来让我自己跑；改完先用 game/sim 在 Mac 上出截图给我看。
```
