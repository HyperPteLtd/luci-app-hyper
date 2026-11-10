/**
 * Browser-only (CSR) Qwik v2 build. The router serves plain static files from
 * /luci-static/hyper/, so there is no SSR/SSG step and Qwik Router is not used;
 * tabs are switched with the URL hash.
 */
import { qwikVite } from "@qwik.dev/core/optimizer";
import { defineConfig } from "vite";

export default defineConfig(({ command }) => ({
  // Qwik needs an absolute base; LuCI serves htdocs/luci-static/hyper/ at this path
  base: command === "build" ? "/luci-static/hyper/" : "/",
  plugins: [qwikVite({ csr: true })],
  build: {
    outDir: "../luci-app-hyper/htdocs/luci-static/hyper",
    emptyOutDir: true,
    target: "es2020",
    sourcemap: false,
  },
  server: {
    port: 5173,
  },
}));
