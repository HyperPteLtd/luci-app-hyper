# 开发进度与待确认事项

## 进度

- [x] 后端从 homeproxy fork：去掉服务端和手动节点，改名 hyper，所有节点挂在 selector 后面，开启 clash_api
- [x] 设备登录（邮箱验证码）、PPanel 白名单代理
- [x] rpcd 接口、ACL、菜单、uci-defaults
- [x] hyper-crypt：加解密往返测试通过（mbedtls 3.6.7）
- [x] 节点同步与 App 一致（按国家拉取加密订阅），解析 sing-box 支持的全部协议；服务端规则同步成 sing-box rule-set
- [x] 前端全部页面：概览、节点、分流与 DNS、接入设备、账户与订阅（资料 / 签到 / 邀请 / 消息）、购买、日志与诊断、设置、验证码登录
- [x] 购买用 payment-v2，支付二维码在页面里直接显示
- [x] 智能分流 / 全局代理 / 仅 DNS 保护三种模式；路由器自身流量走不走代理的开关
- [x] 看门狗：sing-box 失联时让局域网临时直连，恢复后自动切回
- [x] 每日签到：路由器本地日历，按账户保存，不发奖励
- [x] 从 GitHub Release 检查和安装更新（sha256 校验，直连失败走代理）
- [x] sing-box 内核固定为官方 1.14.4 OpenWrt 包，配置只对这一个版本做 `sing-box check`
- [x] CI：测试在 Ubuntu 24.04 上通过（dash、真实 nft）
- [ ] CI 的 SDK 打包第一次跑通
- [ ] 第一个 Release（`v0.1.0`）
- [ ] OpenWrt 实机端到端测试（MediaTek Filogic，24.10 和 25.12 各一次）

## 需要确认

1. **AnyTLS 默认跳过证书校验**：订阅链接没写 `insecure` 时，App 默认跳过校验，路由器照做（`subscription.allow_insecure=1`）。如果节点证书都是正规证书，建议改成默认校验。其他协议按链接里的 `allowInsecure` / `insecure` 参数，没写就校验
2. **Hysteria2 端口跳跃**：链接里写了端口范围时，只连第一个端口，暂不生成 sing-box 的 `server_ports`。服务端如果要求必须跳跃，需要再加
3. **规则策略**：服务端规则只认 `DIRECT` / `PROXY`（和 App 一样），其他会被跳过并写进日志
4. **签到文案**：App 弹窗写“已免费领取 N 天 VIP / 登录奖励已同步到账”，Figma 写“积累更多会员时长”，但后端并不发放时长。路由器上写的是“签到暂不发放会员时长”。以后后端加了签到奖励接口，再接上并改回文案
5. **内核体积**：官方 sing-box 1.14.4 带了全部功能（tailscale、cloudflared、openvpn、naive 等），下载 29.5 MB、安装后 86 MB；OpenWrt 自带的 1.12 是 14.5 MB / 41 MB。128 MB 闪存的机器要留意剩余空间。官方包已带 naive 出站，订阅里的 naive 链接以后可以支持
6. **默认服务地址**：`account.api_base` 默认为空，装好后要先在“设置”里填 PPanel API 地址。如果要预置默认地址，需要给出正式地址
7. 真机测试需要一个测试用的 API 地址和账号
