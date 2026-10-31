# 墨墨五子棋 Momo Gomoku

把百词斩 Ace-1S 墨水屏单词机（杰理 AC7916AB）改造成一台五子棋游戏机的业余项目。

- 可爱的墨墨开机动画、主菜单、五子棋（人机三档 AI + 双人对战）、悔棋与暂停菜单
- 目前是"内存试玩版"：通过 USB 把程序放进芯片 SRAM 里运行，**不写 flash**，复位即回到原厂系统
- 正式版（烧进 flash、开机直接进游戏）还在计划中，见 [`docs/HANDOFF.md`](docs/HANDOFF.md)

> 与百词斩、杰理无任何关系。仓库里不含任何原厂固件；杰理 SDK 和编译器需自行从官方渠道获取。
> 改造有风险，请先完整备份自己设备的 flash（`jlmac/backup.sh`），并读完 `AGENTS.md` 里的注意事项。

## 快速开始（macOS）

```bash
git clone --recursive https://github.com/amoukyou/momo-gomoku.git
cd momo-gomoku
sh jlmac/setup.sh                                                   # Python 环境 + libusb
swift game/tools/mkfont.swift game/src/font_data.c game/src/app.c   # 用系统字体生成字库（不随仓库分发）
```

编译需要一台装有杰理工具链的 x86_64 Linux（我们用 OrbStack 虚拟机 `jlbuild`，工具链在 `/opt/jieli`）：

```bash
orb -m jlbuild bash -c "sh '$PWD/game/ramrun/build.sh'"
```

设备开机（长按大圆键 3 秒）、插上 USB 后加载运行（需要输入 Mac 密码）：

```bash
sudo ./jlmac/ramrun.sh 3
```

## 按键

| 键 | 作用 |
| --- | --- |
| 方向键 | 移动光标 / 选择 |
| 大圆键 | 确认、落子 |
| 小圆键 | 悔棋 |
| 顶部键 | 暂停菜单 |
| 侧面针孔 | 硬件复位（任何时候卡住都能救回） |

## 目录

- `game/src/` 游戏核心：规则与 AI、界面、绘图、SSD1680 墨水屏驱动
- `game/ramrun/` 内存试玩版的裸机硬件层与入口
- `game/sim/` 在 Mac 上测试 AI 和界面
- `game/art/` 动画画布（浏览器里画，导出成点阵）
- `jlmac/` macOS 下的 USB 下载后端
- `analysis/` 杰理固件格式解析脚本
- `docs/HANDOFF.md` 交接文档（现状、踩过的坑、下一步）· `NOTES.md` 逆向笔记 · `AGENTS.md` 给编程助手的说明

## 致谢

- [kagaimiq/jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool)（MIT），以子模块引用
- 动画标题字体：站酷快乐体 ZCOOL KuaiLe（SIL OFL 1.1）
