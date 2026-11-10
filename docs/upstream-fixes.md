# 与上游 homeproxy 的差异和修复记录

上游：<https://github.com/immortalwrt/homeproxy>，fork 于 commit `fadaa676104f59ff6ace480a780a258d846d0aec`（2026-10-02）。

## 有意的改动

| 文件 | 改动 | 原因 |
|---|---|---|
| 全部 | `homeproxy` → `hyper`（UCI、路径、nft 链名、rpcd 对象） | 独立包名，可与 homeproxy 共存 |
| `init.d/hyper`、`firewall_pre.uc`、配置 | 删除服务端模式 | 路由器版只做客户端 |
| `init.d/hyper` | 增加 `config.enabled` 开关 | 界面上的启动 / 停止按钮 |
| `generate_client.uc` | 所有节点都生成出站，挂在 `main-out` selector 后面 | 通过 clash_api 实时切换节点、测速，不用重启 |
| `generate_client.uc` | 始终开启 `cache_file`，新增 `clash_api`（只监听 127.0.0.1，带密钥） | 流量统计、连接列表 |
| `init.d/hyper` | 每种模式都创建并挂载 `cache.db` | 配合上面的 cache_file |
| `update_subscriptions.uc` | 删除，换成 `sync_subscription.uc` + `snapshot.uc` | 节点改为登录 PPanel 后按国家拉取加密订阅；`parse_uri` 移植成不依赖路由器的纯函数，支持全部协议并加了单元测试 |
| `luci.homeproxy` 和 LuCI 页面 | 删除，换成 `luci.hyper` 和独立前端 | 新界面 |

## 发现的上游问题

| 位置 | 问题 | 处理 |
|---|---|---|
| `update_subscriptions.uc:40` | 默认值拼成了 `bypass_mainalnd_china` | 文件已整体替换，不再适用；可回报上游 |
| `update_subscriptions.uc:383` | `match(uri, /&/)` 对数组做匹配，应为 `uri[1]` | 同上 |
| `update_subscriptions.uc:328,331` | 日志拼写 `sunsupported` | 同上 |
| `update_subscriptions.uc` 各协议 | 密码、名称用 `urldecode()`，把 `+` 当成空格，含 `+` 的密码会被改坏 | 已修：userinfo 和名称按 `decodeURIComponent` 规则解码，只有查询参数把 `+` 当空格 |
| `update_subscriptions.uc` vless / vmess | `tcp` + `headerType=http` 被当成 HTTP 传输，sing-box 没有对应实现，节点连不上 | 已修：跳过并记日志 |
| `update_subscriptions.uc` vless | REALITY 链接没带 `fp` 时不开 uTLS，sing-box 拒绝启动（REALITY 需要 uTLS） | 已修：默认 `chrome` |
| `update_subscriptions.uc` vless | 旧的 `xtls-rprx-direct` 等 flow 原样写入，sing-box 启动失败 | 已修：只接受 `xtls-rprx-vision`，其他跳过 |
| `update_subscriptions.uc` ss | 2022 系列密钥长度不对、未知加密方式、sing-box 不支持的插件原样写入，一个坏节点让整个 sing-box 起不来 | 已修：写入前校验，坏节点跳过 |
| `init.d/homeproxy` | `cache.db` 只在 bypass 模式创建 / 挂载进 ujail，但其他模式下若开启 cache_file 会写失败 | 已修：每种模式都创建并挂载 |
| `generate_client.uc` | 选项名 `hysteria_revc_window`（拼写） | 保留（与上游选项名一致）；我们的同步不写这个选项 |
| `generate_client.uc` `parse_dnsserver()` | DNS 地址解析失败时 `parseURL()` 返回 null，接着取 `.protocol` 直接崩溃，整个配置生成失败 | 已修：解析失败就跳过这个 DNS（PC 上的渲染测试发现） |
| `firewall_post.ut` 中国 IP 集合 | `china_ip4.txt` / `china_ip6.txt` 缺失或为空（比如资源更新失败）时，生成 `elements = { , }`，这是 nft 语法错误，fw4 会拒绝整个规则集 | 已修：跳过空行和注释，列表为空时不写 `elements`；渲染测试新增“资源列表缺失”用例，并在 CI 里用 `nft -c` 检查（包进 `table inet fw4 { }`，和 fw4 的 table-post 包含方式一致） |
| issue #402 | sing-box 启动失败或崩溃时，dnsmasq 和防火墙仍指向它，整个局域网断网 | 已修：新增 `watchdog.sh`，连续 3 次连不上 clash_api 就撤掉 DNS 劫持和 nft 规则，让局域网直连；恢复后自动重启服务 |
| issue #234 | 用户在 dnsmasq 里设置的上游 DNS 会绕过劫持 | 未处理，记录在案 |
| issue #624 | 用一段时间后间歇性断网 | 上游没有日志，无法判断；看门狗至少保证断开时局域网还能直连 |

## 本仓库自查发现的问题（2026-11-09）

| 位置 | 问题 | 处理 |
|---|---|---|
| 所有 ucode 校验正则 | ucode 编译正则时带 `REG_NEWLINE`，`^...$` 只要求其中一行匹配：`match("ok\nrest", /^ok$/)` 成立。payment-v2 幂等键因此能夹带换行，在 curl 配置文件里多出一行 `url = ...`，请求头（含账户 JWT）会同时发往这个地址（只有已登录的 LuCI 管理员能调用）。DNS 设置同理能往 nft 模板里多写一行。另外正则遇到 NUL 就停止，`"ok\u0000\nrest"` 连换行检查也能绕过 | 已修：`hyper.uc` 新增 `matchWhole()`（先拒绝任何控制字符，NUL 单独查），rpcd 和 ppanel 的校验全部改用它；`httpRequest()` 发现 URL / 方法 / 代理 / 请求头里有控制字符直接拒绝；订阅解析里任何字段带控制字符的节点丢弃，名称里的控制字符换成空格。上游的同类写法可能也受影响，未逐一核对 |
| `luci.hyper` SETTINGS | `{1,256}` 超过 `RE_DUP_MAX`（255）。musl（OpenWrt）在第一次用到这个正则时报错，而它在脚本顶层，等于整个 `luci.hyper` 在路由器上加载失败。`ucode -c` 不编译正则，所以编译检查没发现 | 已修：改成 `{1,255}`；`tests/run.sh` 新增重复次数扫描，并实际加载一次 `luci.hyper` |
| `luci-app-hyper/Makefile` | `luci.mk` 默认开着 `CONFIG_LUCI_JSMIN` / `CONFIG_LUCI_CSSTIDY`，会把包里所有 `.js` / `.css` 过一遍老的 jsmin / csstidy，它们不认识模板字符串和新 CSS，Vite 打包的前端会被改坏（SDK 打包时才会发生，本机预览看不出来） | 已修：`LUCI_MINIFY_JS:=0`、`LUCI_MINIFY_CSS:=0`，打包后的 JS 与 Vite 输出逐字节一致 |
| `hyper.uc` `executeCommand()` / `httpRequest()` | 用 `>&12` 这种两位数文件描述符重定向。路由器上的 busybox ash 支持，但 Debian/Ubuntu 的 dash 只认 0–9，CI 里 HTTP 测试直接 `Bad fd number` | 已修：改成 `>/dev/fd/12` 这类路径，各种 sh 都能用 |
| 前端按钮状态 | Qwik 2.0.0-rc.4 会把 `disabled={!dirty \|\| busy.value}` 这类“信号 + 渲染时常量”的表达式提成派生信号，常量只记第一次的值。处理函数同时改了信号和常量背后的数据时，按钮停在旧状态（例如保存后仍显示“保存并应用”） | 已修：接入设备、日志体检、概览、分流 DNS、设置、账户、签到 7 处改成先把信号读进常量；新增 `web/scripts/lint-signals.mjs`，CI 里检查 |

## 新增功能（相对上游）

- `control.proxy_router_self`：设为 `0` 时跳过 output 钩子，路由器自身流量（opkg、NTP 等）直连
- `watchdog.sh`：见上表 #402
- `config.dns_only`：仅 DNS 保护。sing-box 只作为 dnsmasq 的上游（国内域名走国内 DNS，其余经代理加密解析），不生成 redirect / tproxy / tun 入站，`firewall_post.ut` 不输出任何规则，局域网流量全部直连
- 订阅解析新增 WireGuard 链接（`wireguard://` / `wg://`）和 SIP008 JSON
- sing-box 内核固定版本：不再用发行版自带的 sing-box（24.10 是 1.12、25.12 是 1.13），改为依赖 SagerNet 官方 OpenWrt 包 `sing-box>=1.14.4`，随 Release 一起发；官方包自带的 sing-box 服务在安装和启动时关掉
- 每日签到（`checkin.uc`，rpcd `checkin` / `checkin_status`）：PPanel 没有签到接口，记录只存在路由器上，按账户分开

## 待做

- [x] 过一遍上游 open issues（2026-11-09：#624、#402、#320、#275、#234 等 7 个）
- [ ] 值得回馈的修复（parse_dnsserver、#402 看门狗、cache.db 挂载）整理成上游 PR，需确认后再提
