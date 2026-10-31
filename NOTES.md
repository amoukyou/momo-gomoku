# 百词斩 Ace-1S 逆向笔记

## 硬件
- 主控：杰理 AC7916AB（WL82 / AC791N 系列），Wi-Fi + BLE
- SPI Flash：GigaDevice 16 MB（JEDEC 0xC84018），存放固件
- SD NAND：米客方德 MKDV4GCL-ABB 4 GB，FAT 文件系统，存放词书
- 屏幕：2.7" 电子墨水屏 264×176；功放 NS4150B；充电 TP4056
- 原厂固件版本 AC7916AB_1V5.8.1

## 进入下载模式（无需拆机）
- 正常开机后通过 USB 以 "WL82 UDISK" 出现；向它发送任意 JL loader v2 命令（如 FC 0A），
  它就会重启进入 ROM 下载模式（"WL82 UBOOT1.00"）
- 再加载 jl-uboot-tool 里的 wl82loader.bin，即可读写 SPI flash
- macOS 下需要 sudo（libusb 要先卸掉系统的 U 盘驱动）：`sudo ./jlmac/backup.sh`

## 救急
- 侧面针孔里有一个硬件复位键：捅一下就能让芯片彻底复位，回到 flash 里的固件（原厂系统）
- 内存试玩程序绝不要 return 回 ROM：ROM 的 USB 状态会被我们占用的内存破坏，之后只能识别不能通信

## 已确认的引脚（从原厂固件反推，并在真机上验证）
- 墨水屏 SSD1680：SCK PC09、MOSI PC10、CS PC06、DC PC07、RST PC08、BUSY PC05（高=忙）；
  画面要旋转 180°（EPD_FLIP_X | EPD_FLIP_Y）
- 按键（低有效）：上 PC01、下 PA07、左 PH03、右 PC00、大圆键 PA08（确认）、小圆键 PA10
- PB01：ADC 按键线（外部上拉）——顶部电源键≈0、侧面音量+≈588、音量−≈693（满量程 1023）；也是软关机唤醒脚
- 看门狗：P33 寄存器 0x80 置 0x40 喂狗（P33 总线 CON 0x13e08 / BUF 0x13e0c）
- 软复位：*(u32*)0x10000 |= 1<<4

## 固件格式（已验证）
- 芯片密钥 chipkey = 0xF181
- 0x0000：flash 头（32 字节，JL ENC 加密，key=0xFFFF），pid = "AC791N_STORY"
- 0x0020 起：顶层文件表（每条 32 字节，各自用 ENC key=0xFFFF 加密，CRC16-XMODEM 校验）
  - uboot.boot_debug @0xC0、isd_config.ini @0x54A4、app_dir_head @0x6020、
    app_dir_head2 @0x7F5020（OTA 第二分区，空）、key_mac @0xFEF000
- 0x6020 起：应用区，SFC 加密：每 32 字节一块，key = chipkey ^ ((addr - 0x6020) >> 2)
  - app_area_head → app.lz4（rel 0x120，16 字节头 + LZ4 块流，解压后 0x19E688 字节）、
    cfg_tool.bin、VM @0xFE4000、BTIF @0xFEC000、PROFILE @0xFED000
- 原厂 app 基于杰理 fw-AC79_AIoT_SDK，工程 apps/word_app，UI 用 LVGL

## SD NAND 上的路径（来自 app 字符串）
- storage/sd0/C/book/info.txt、userbook/book.zpk、userbook/info.txt、lockscrn/、update.ufw
- 单词字段：word, accent, accentUK, mean, enMean, sentence, sentenceTrans,
  wordAudio*/sentenceAudio* Offset/Length, picUrl*, word4Data, word_cloze, topicId

## 文件
- AGENTS.md：给 Codex 等编程助手的项目说明与规矩
- backup/ace1s_spiflash_16M.bin：原厂完整备份（SHA-256 见 .sha256）
- jlmac/：macOS 下运行 jl-uboot-tool 的 libusb 后端；setup.sh（建 .venv）、backup.sh、ramrun.sh
- third_party/jl-uboot-tool/：kagaimiq 的杰理下载工具（MIT）
- game/：五子棋（src 核心、ramrun 内存试玩版、sim 电脑测试、art 动画、tools 字体/动画生成）
- analysis/：解析文件表、解密、解压 app 的脚本
