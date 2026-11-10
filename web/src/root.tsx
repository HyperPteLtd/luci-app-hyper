import { component$, type JSXOutput, useContextProvider, useStore, useTask$ } from "@qwik.dev/core";
import { TopBar } from "./components/TopBar";
import { AppContext, type AppState, type Route, routeFromHash } from "./lib/app";
import { call, isMock, reportHeight } from "./lib/rpc";
import type { Status } from "./lib/types";
import { Account } from "./pages/Account";
import { Devices } from "./pages/Devices";
import { Login } from "./pages/Login";
import { Logs } from "./pages/Logs";
import { Nodes } from "./pages/Nodes";
import { Overview } from "./pages/Overview";
import { Plans } from "./pages/Plans";
import { Routing } from "./pages/Routing";
import { Settings } from "./pages/Settings";

import "./global.css";

export default component$(() => {
  /* Browser-only build: window is always there, so the first render already has the right tab */
  const app = useStore<AppState>({ route: routeFromHash(location.hash), status: null, loading: true, error: "", tick: 0 });
  useContextProvider(AppContext, app);

  /*
   * Hash routing (#/nodes ...), iframe height reporting and status polling.
   * useTask$ rather than useVisibleTask$: in Qwik's CSR mode visible tasks do
   * not reliably fire (seen in both vite dev and the production build).
   */
  useTask$(({ cleanup }) => {
    const onHash = () => {
      app.route = routeFromHash(location.hash);
      window.scrollTo(0, 0);
    };
    window.addEventListener("hashchange", onHash);

    const ro = new ResizeObserver(() => reportHeight());
    ro.observe(document.body);

    /* Refresh status every 5 s so start/stop and watchdog changes show up */
    const id = setInterval(() => app.tick++, 5000);
    cleanup(() => {
      window.removeEventListener("hashchange", onHash);
      ro.disconnect();
      clearInterval(id);
    });
  });

  useTask$(async ({ track }) => {
    track(() => app.tick);
    try {
      app.status = await call<Status>("status");
      app.error = "";
    } catch (e) {
      app.error = (e as Error).message;
    }
    app.loading = false;
  });

  const s = app.status;
  let page;
  if (app.error && !s) page = <main class="page"><div class="banner">无法连接路由器：{app.error}</div></main>;
  else if (!s) page = <main class="page muted">加载中…</main>;
  else if (!s.signed_in && app.route !== "settings") page = <Login />;
  else page = PAGES[app.route]();

  return (
    <>
      {isMock() && (
        <div style={{ background: "#1f1a0e", color: "#ffd27a", fontSize: "12px", padding: "6px 32px" }}>
          演示模式：不在 LuCI 里打开，使用的是模拟数据。
        </div>
      )}
      <TopBar />
      {page}
    </>
  );
});

const PAGES: Record<Route, () => JSXOutput> = {
  overview: () => <Overview />,
  nodes: () => <Nodes />,
  routing: () => <Routing />,
  devices: () => <Devices />,
  account: () => <Account />,
  plans: () => <Plans />,
  logs: () => <Logs />,
  settings: () => <Settings />,
};
