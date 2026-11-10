# 实现说明

给维护者看的内部细节：各部分怎么配合、和 PPanel 怎么对接。使用说明见 [README](../README.md)。

## 组成

```
浏览器（LuCI 会话）──ubus──▶ rpcd luci.hyper ──curl──▶ PPanel API
                                   │
                                   ├─▶ /etc/hyper/session（JWT、订阅 token，0600）
                                   ├─▶ /etc/hyper/checkin.json（签到日期，按账户）
                                   ├─▶ UCI /etc/config/hyper ──▶ generate_client.uc ──▶ sing-box（官方包，固定版本）
                                   └─▶ clash_api 127.0.0.1:5334（流量、测速、切换节点）
```

- 前端是独立页面（Qwik v2 浏览器端模式），由 LuCI 菜单页 `hyper/app` 用 iframe 挂载，只通过 LuCI 会话调用 ubus 对象 `luci.hyper`。
- 浏览器拿不到任何账户凭证：PPanel 的 JWT 和订阅 token 只存在路由器的 `/etc/hyper/session`（0600），请求由 rpcd 代发，只放行白名单里的路径。
- 流量统计来自 sing-box 的 clash_api，只监听 127.0.0.1 并带密钥，由 rpcd 转发。

### 主要文件

```
luci-app-hyper/root/
├── etc/config/hyper                      UCI 配置
├── etc/init.d/hyper                      procd 服务（sing-box + 看门狗）
├── etc/uci-defaults/luci-hyper           生成 device_id / clash_api 密钥，注册 fw4 include，关掉官方 sing-box 包自带的服务
├── etc/hyper/scripts/
│   ├── hyper.uc               公共函数（来自 homeproxy.uc，加了 session / curl）
│   ├── ppanel.uc              PPanel 客户端（设备登录、路径白名单、订阅 token、payment-v2 幂等键）
│   ├── snapshot.uc            解密后的订阅内容（分享链接）→ UCI node
│   ├── rules.uc               服务端规则列表（Clash 格式）→ sing-box rule-set
│   ├── checkin.uc             每日签到
│   ├── sync_subscription.uc   同步节点
│   ├── generate_client.uc     生成 sing-box 配置（所有节点挂在 selector 后面，开 clash_api）
│   ├── firewall_*.u[ct]       nftables 规则（fw4 include）
│   └── watchdog.sh            sing-box 失联时让局域网临时直连
└── usr/share/rpcd/ucode/luci.hyper       前端调用的 ubus 接口
```

## 登录和节点同步

流程与 Hyper App（同级仓库 `hyper-tunnel`，`packages/app-client/src/subscribe/client.ts`）一致：

1. 登录：`POST /v1/common/send_code`（type 2）发验证码 → `POST /v1/auth/login`，带 `Login-Type: device`、`identifier`（路由器的 device_id）、`email`、`code`（同时作为 password）。新邮箱自动注册。路由器占账户 1 个设备名额
2. `GET /v1/public/user/subscribe` 取订阅 token（只有设备会话会返回）
3. 订阅接口固定在 API 地址下的 `/api/subscribe`：`GET /api/subscribe/location?token=` 列出有节点的国家，每次同步拉全部国家（最多 64 个）
4. 每个国家：`hyper-crypt keygen` 生成临时 X25519 密钥 → `GET /api/subscribe?token=&country=&client_public_key=` 拿到 `{spk, salt, nonce, data}` → `hyper-crypt open`（HKDF info `subscribe-v1`，AES-256-GCM）解出 `REMARKS=` / `STATUS=` / 分享链接
5. 分享链接按 sing-box 支持的全部协议解析：AnyTLS、VLESS（TLS / REALITY / Vision）、VMess、Trojan、Shadowsocks（SIP002、整段 base64、SIP008 JSON，2022 系列，obfs / v2ray-plugin）、Hysteria、Hysteria2、TUIC、WireGuard、SOCKS、HTTP(S)；传输支持 TCP / WebSocket / gRPC / HTTP/2 / HTTPUpgrade / QUIC。sing-box 用不了的链接（kcp、xhttp、ssr、naive 等）和坏链接跳过并写进日志，不会让 sing-box 起不来；任何字段带控制字符的节点直接丢弃
6. `GET /api/subscribe/rules` 取服务端规则（`{key, value, policy: DIRECT|PROXY, enabled, sort}`，value 是规则文本或列表 URL），转成 sing-box 本地 rule-set，只在智能分流模式生效
7. 写入 UCI `node`，`generate_client.uc` 组装 sing-box 出站；有变化就重启服务

某个国家拉取失败时，它原来的节点保留；全部失败时什么都不改。

## 代理模式

- **智能分流**：国内域名和 IP 直连，其余走代理（`routing_mode=bypass_mainland_china`）
- **全局代理**：全部走代理
- **仅 DNS 保护**（`config.dns_only=1`）：sing-box 只给 dnsmasq 做加密 DNS，国内域名走国内 DNS，其余经代理解析；不装透明代理规则，局域网流量全部直连

## 购买（payment-v2）

和 App 一样：`/v1/public/order/pre` 算价格 → `POST /v2/orders/purchase`（带 `Idempotency-Key`，重复点击不会生成第二个订单）→ 按返回的支付方式显示：扫码类直接画二维码，跳转类给支付页面按钮和二维码，加密货币先选网络和币种再显示收款地址和二维码 → 轮询 `GET /v2/orders/{no}/payment` 直到支付成功，然后同步订阅。Apple 内购只能在 iOS App 里完成，路由器上不显示。

## 每日签到

PPanel 没有签到接口，App 的签到日历也只存在手机上。路由器版放在“账户与订阅 → 每日签到”，按 Figma M21 画面做：点“立即签到”记下浏览器当天的日期，日历显示本月签到、累计天数和连续天数。记录存在 `/etc/hyper/checkin.json`，按账户（邮箱的 md5 前 16 位）分开，升级时保留；路由器时钟已同步时，只接受和路由器日期相差一天以内的日期。不发放会员时长。

## sing-box 内核

用 SagerNet 官方发布的 OpenWrt 包（`sing-box_<版本>_openwrt_<CPU>.ipk/.apk`），直接下载、不自己编译。版本写在 `luci-app-hyper/Makefile` 的 `HYPER_SINGBOX_VERSION`，哈希在 `scripts/sing-box.sha256`。luci-app-hyper 依赖 `sing-box>=<该版本>`，所以 OpenWrt 软件源里旧的 sing-box（24.10 是 1.12，25.12 是 1.13）不能代替它；Release 和测试包都附带对应 CPU 的官方包。官方包自带的 `sing-box` 服务在安装和启动 Hyper 时会被关掉，只由 Hyper 启动 sing-box。

生成的配置只对这一个版本测试（CI 用官方 x86_64 包里的静态二进制做 `sing-box check`）。

## 更新

“设置 → 关于”里的检查更新读 `api.github.com/repos/<update.repo>/releases/latest`，按 opkg / apk 和本机 CPU 选资产，用同一个 Release 里的 `sha256sums` 校验后安装：先 sing-box，再 hyper-crypt，最后 luci-app-hyper。直连 GitHub 失败时经 sing-box 代理下载。安装在后台进行（新包会替换 rpcd 脚本本身），前端轮询 `check_update` 的 `update_state`。

## 设计

- 设计稿（Hyper 深色视觉 + LuCI 结构）：<https://claude.ai/artifact/L5tcUjHQ2TX2u4K8qDiZQx>
- Figma「Hyper VPN 2.0」是 macOS App 的画面，路由器版只沿用视觉，结构按 LuCI 的标签页来
