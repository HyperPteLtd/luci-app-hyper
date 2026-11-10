# luci-app-hyper

[![CI](https://github.com/HyperPteLtd/luci-app-hyper/actions/workflows/ci.yml/badge.svg)](https://github.com/HyperPteLtd/luci-app-hyper/actions/workflows/ci.yml)

Hyper 的 OpenWrt 路由器插件。在路由器上登录 Hyper 账户后，整个局域网经由 sing-box 访问网络，手机、电视、游戏机都不用再单独装客户端。

基于 [immortalwrt/homeproxy](https://github.com/immortalwrt/homeproxy) 改造：保留它的 sing-box 配置生成和 nftables 透明代理，节点改为从 Hyper 账户自动同步，界面换成 Hyper 的设计。

## 功能

- **账户**：邮箱验证码登录（新邮箱自动注册），路由器占账户 1 个设备名额；登录凭证只保存在路由器上，不经过浏览器
- **节点**：登录后自动同步全部国家的节点，支持 sing-box 的全部协议（AnyTLS、VLESS / REALITY、VMess、Trojan、Shadowsocks、Hysteria2、TUIC、WireGuard 等）；测速、自动选择最快节点，每天定时同步
- **三种模式**：智能分流（国内直连）、全局代理、仅 DNS 保护（只加密 DNS，流量全部直连）
- **分流与 DNS**：服务端下发的分流规则可逐条开关；自定义直连 / 代理域名；选择加密 DNS 和国内 DNS
- **接入设备**：查看局域网设备和实时流量，可以让个别设备（游戏机、公司电脑）保持直连
- **账户与订阅**：套餐和到期时间、已登录设备管理、每日签到、邀请好友、服务公告
- **购买套餐**：在路由器页面里下单；扫码类支付直接显示二维码，跳转类给出支付页面，也支持加密货币（可用的支付方式由服务端决定）
- **日志与诊断**：运行日志、一键体检、Ping / 路由追踪 / DNS 查询 / HTTP 测试
- **稳定性**：sing-box 意外退出时自动让局域网直连，恢复后切回，不会整个家断网
- **更新**：在“设置”里检查并安装新版本（来自本仓库的 GitHub Releases，sha256 校验）

## 运行要求

- OpenWrt **24.10**（opkg，`.ipk`）或 **25.12**（apk，`.apk`），使用默认的 firewall4 / nftables
- 闪存剩余约 **100 MB** 以上：sing-box 内核安装后约 86 MB
- 内存建议 **256 MB** 以上
- 路由器能访问 Hyper 服务地址；安装时还要能访问 OpenWrt 官方软件源（装 kmod-tun 等依赖）

支持的 CPU（`cat /etc/openwrt_release` 里的 `DISTRIB_ARCH`）：

| CPU 架构 | 常见设备 | 24.10 | 25.12 |
|---|---|:-:|:-:|
| `aarch64_cortex-a53` | MediaTek Filogic（MT7981 / MT7986）：小米 AX3000T、GL.iNet MT3000 / MT6000、Cudy TR3000 等 | ✓ | ✓ |
| `aarch64_generic` | Rockchip（NanoPi R2S / R4S / R5S 等）、通用 ARMv8 | ✓ | ✓ |
| `x86_64` | 软路由、虚拟机 | ✓ | ✓ |
| `arm_cortex-a7_neon-vfpv4` | IPQ40xx 等 | ✓ | |
| `mipsel_24kc` | MT7621（闪存和内存往往不够，不推荐） | ✓ | |

## 安装

### 1. 下载

每个版本需要三个包，都在同一个地方下载：

| 包 | 说明 |
|---|---|
| `luci-app-hyper` | 插件本体（所有 CPU 通用） |
| `hyper-crypt` | 订阅解密小工具（按 CPU 区分） |
| `sing-box_<版本>_openwrt_<CPU>` | SagerNet 官方发布的 sing-box 内核（按 CPU 区分，版本固定） |

- **正式版**：[Releases](https://github.com/HyperPteLtd/luci-app-hyper/releases) 页面，选和路由器 CPU、OpenWrt 版本对应的文件
- **测试版**：[Actions](https://github.com/HyperPteLtd/luci-app-hyper/actions/workflows/ci.yml) 里任意一次成功的运行，页面底部 Artifacts 下载 `packages-<CPU>-<OpenWrt 版本>`，例如 `packages-aarch64_cortex-a53-24.10.8`（需要登录 GitHub）

### 2. 传到路由器并安装

```sh
scp -O *.ipk root@192.168.1.1:/tmp/    # OpenWrt 25.12 换成 *.apk；OpenWrt 的 dropbear 需要 -O
```

OpenWrt 24.10：

```sh
opkg update
opkg install /tmp/sing-box_*_openwrt_*.ipk /tmp/hyper-crypt_*.ipk /tmp/luci-app-hyper_*.ipk
```

OpenWrt 25.12（包没有签名，需要 `--allow-untrusted`）：

```sh
apk update
apk add --allow-untrusted /tmp/sing-box_*_openwrt_*.apk /tmp/hyper-crypt-*.apk /tmp/luci-app-hyper-*.apk
```

已经装了 OpenWrt 软件源里的旧版 sing-box 也没关系，会被替换成固定版本。

### 3. 首次使用

1. 打开 LuCI 的 **服务 → Hyper**
2. 在 **设置 → 服务器与订阅** 里填写服务地址（PPanel API 地址）
3. 回到 **账户与订阅**，用邮箱验证码登录，登录后会自动同步节点
4. 在 **概览** 里选择模式并打开开关；默认所有局域网设备都走 Hyper，个别设备可以在 **接入设备** 里改成直连

### 更新与卸载

- 更新：**设置 → 关于 Hyper 路由器版** 里点“检查更新”，有新版本时“下载并安装”。也可以打开“每天自动检查更新”（只提醒，不自动安装）
- 卸载：`opkg remove luci-app-hyper hyper-crypt sing-box`（25.12 用 `apk del luci-app-hyper hyper-crypt sing-box`）

## 开发

```
luci-app-hyper/
├── luci-app-hyper/   LuCI 插件：Makefile、rpcd 接口、UCI、procd 服务、sing-box 配置和防火墙生成
├── hyper-crypt/      订阅解密小工具（C + mbedtls）
├── web/              前端（Qwik v2，Vite），构建到 luci-app-hyper/htdocs/luci-static/hyper/
├── tests/            ucode 单元测试、配置渲染（sing-box check、nft -c）、看门狗
├── scripts/          本机打包、下载官方 sing-box 包
├── docs/             实现说明、开发进度、对上游的修复
└── .github/workflows/ci.yml   测试、SDK 打包、发布
```

### 前端

```sh
npm --prefix web ci
npm --prefix web run dev         # http://localhost:5173，不在 LuCI 里时使用演示数据
npm --prefix web run typecheck
npm --prefix web run lint        # Qwik 信号写法检查
npm --prefix web test            # 需要 Node 23.6 以上
npm --prefix web run build
```

### 测试

需要在本机编译 ucode 和 mbedtls 3.6（步骤见 `.github/workflows/ci.yml`）；装了 sing-box 时会对生成的配置做 `sing-box check`，有 nft 和 root 权限时会检查防火墙规则：

```sh
UCODE=/path/to/ucode/build/ucode UCODE_LIB=/path/to/ucode/build sh tests/run.sh
MBEDTLS=/path/to/mbedtls-3.6.x sh hyper-crypt/test/run.sh
```

### 打包

- **CI**：每次推送都会跑测试，并用 OpenWrt 24.10.8 / 25.12.6 的 SDK 打出各 CPU 的包，放在运行的 Artifacts 里
- **本机**：`scripts/build-packages.sh mediatek/filogic`（参数是 OpenWrt 的 target/subtarget，默认 `x86/64`），产物在 `dist/`。需要 Docker 能运行 x86_64 容器；Apple Silicon 上 colima 默认的 qemu 7.0 会让 SDK 的编译器崩溃，要开 Rosetta 或换新版 qemu
- **手动**：在 OpenWrt SDK 里把本仓库加为 feed（先构建前端）

  ```sh
  echo "src-link hyper /path/to/luci-app-hyper" >> feeds.conf.default
  ./scripts/feeds update -a && ./scripts/feeds install luci-app-hyper hyper-crypt
  make package/hyper-crypt/compile package/luci-app-hyper/compile V=s
  ```

### 发布

1. 修改 `luci-app-hyper/Makefile` 和 `hyper-crypt/Makefile` 里的 `PKG_VERSION`
2. 打标签并推送，例如 `git tag v0.1.0 && git push origin v0.1.0`
3. CI 自动创建 Release，附全部包和 `sha256sums`；路由器上的“检查更新”读的就是最新 Release

### 升级 sing-box 内核

sing-box 版本固定，不跟随 OpenWrt 软件源：

1. 改 `luci-app-hyper/Makefile` 里的 `HYPER_SINGBOX_VERSION`
2. 按新版本 Release 页面公布的 sha256 更新 `scripts/sing-box.sha256`
3. 跑测试（会用新内核检查生成的配置），有不兼容的地方再适配

## 文档

- [实现说明](docs/architecture.md)：各部分怎么配合，和 PPanel 的对接细节
- [开发进度与待确认事项](docs/status.md)
- [对上游的修复](docs/upstream-fixes.md)

## 许可证

GPL-2.0-only，继承自 [homeproxy](https://github.com/immortalwrt/homeproxy)，见 [LICENSE](LICENSE)。sing-box 内核是 [SagerNet/sing-box](https://github.com/SagerNet/sing-box) 发布的官方 OpenWrt 包（GPL-3.0-or-later），原样分发，未作修改。
