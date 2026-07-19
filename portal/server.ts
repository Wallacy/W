import { join } from "node:path";

const root = import.meta.dir;

const routes = new Map<string, { file: string; type: string }>([
  ["/", { file: "index.html", type: "text/html; charset=utf-8" }],
  ["/index.html", { file: "index.html", type: "text/html; charset=utf-8" }],
  ["/book", { file: "book.html", type: "text/html; charset=utf-8" }],
  ["/book.html", { file: "book.html", type: "text/html; charset=utf-8" }],
  ["/reference", { file: "reference.html", type: "text/html; charset=utf-8" }],
  ["/reference.html", { file: "reference.html", type: "text/html; charset=utf-8" }],
  ["/playground", { file: "playground.html", type: "text/html; charset=utf-8" }],
  ["/playground.html", { file: "playground.html", type: "text/html; charset=utf-8" }],
  ["/status", { file: "status.html", type: "text/html; charset=utf-8" }],
  ["/status.html", { file: "status.html", type: "text/html; charset=utf-8" }],
  ["/styles.css", { file: "styles.css", type: "text/css; charset=utf-8" }],
  ["/app.js", { file: "app.js", type: "text/javascript; charset=utf-8" }],
  ["/w-syntax.js", { file: "w-syntax.js", type: "text/javascript; charset=utf-8" }],
  ["/book.js", { file: "book.js", type: "text/javascript; charset=utf-8" }],
  ["/playground.js", { file: "playground.js", type: "text/javascript; charset=utf-8" }],
  ["/README.md", { file: "README.md", type: "text/markdown; charset=utf-8" }],
  ["/examples/restaurant/README.md", { file: "../examples/restaurant/README.md", type: "text/markdown; charset=utf-8" }],
  ["/examples/restaurant/REQUIREMENTS.md", { file: "../examples/restaurant/REQUIREMENTS.md", type: "text/markdown; charset=utf-8" }],
  ["/examples/restaurant/DB1_ASSAY.md", { file: "../examples/restaurant/DB1_ASSAY.md", type: "text/markdown; charset=utf-8" }],
  ["/examples/restaurant/domain.w", { file: "../examples/restaurant/domain.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/resources.w", { file: "../examples/restaurant/resources.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/menu.w", { file: "../examples/restaurant/menu.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/order_service.w", { file: "../examples/restaurant/order_service.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/kitchen.w", { file: "../examples/restaurant/kitchen.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/dining_room.w", { file: "../examples/restaurant/dining_room.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/units.w", { file: "../examples/restaurant/units.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/oven.w", { file: "../examples/restaurant/oven.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/refrigeration.w", { file: "../examples/restaurant/refrigeration.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/planning.w", { file: "../examples/restaurant/planning.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/billing.w", { file: "../examples/restaurant/billing.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/front_desk.w", { file: "../examples/restaurant/front_desk.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/terminal.w", { file: "../examples/restaurant/terminal.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/web.w", { file: "../examples/restaurant/web.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/app.w", { file: "../examples/restaurant/app.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/interop.w", { file: "../examples/restaurant/interop.w", type: "text/plain; charset=utf-8" }],
  ["/examples/restaurant/multilingual.md", { file: "../examples/restaurant/multilingual.md", type: "text/markdown; charset=utf-8" }],
  ["/corpus/README.md", { file: "../corpus/README.md", type: "text/markdown; charset=utf-8" }],
  ["/corpus/manifest.json", { file: "../corpus/manifest.json", type: "application/json; charset=utf-8" }],
  ["/corpus/schema.json", { file: "../corpus/corpus.schema.json", type: "application/schema+json; charset=utf-8" }],
  ["/corpus/diagnostics.schema.json", { file: "../corpus/diagnostics.schema.json", type: "application/schema+json; charset=utf-8" }],
  ["/docs/README.md", { file: "../README.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/ARCHITECTURE.md", { file: "../ARCHITECTURE.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/STATUS.md", { file: "../STATUS.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/DB1_REVIEW.md", { file: "../DB1_REVIEW.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/design/documentation-and-tests.md", { file: "../design/documentation-and-tests.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/LANGUAGE_TOUR.md", { file: "../LANGUAGE_TOUR.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/ROADMAP.md", { file: "../ROADMAP.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/spec/syntax.md", { file: "../spec/syntax.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/spec/types-and-memory.md", { file: "../spec/types-and-memory.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/spec/concurrency.md", { file: "../spec/concurrency.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/spec/modules.md", { file: "../spec/modules.md", type: "text/markdown; charset=utf-8" }],
  ["/spec/modules.md", { file: "../spec/modules.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/design/compiler.md", { file: "../design/compiler.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/design/modules-and-runtime.md", { file: "../design/modules-and-runtime.md", type: "text/markdown; charset=utf-8" }],
  ["/design/modules-and-runtime.md", { file: "../design/modules-and-runtime.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/design/resource-estimation.md", { file: "../design/resource-estimation.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/design/packages.md", { file: "../design/packages.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/design/stdlib.md", { file: "../design/stdlib.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/design/formatting.md", { file: "../design/formatting.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/design/numerics-and-quantities.md", { file: "../design/numerics-and-quantities.md", type: "text/markdown; charset=utf-8" }],
  ["/docs/research/long-term-program.md", { file: "../research/long-term-program.md", type: "text/markdown; charset=utf-8" }],
]);

const securityHeaders = {
  "Content-Security-Policy": [
    "default-src 'self'",
    "script-src 'self'",
    "style-src 'self'",
    "img-src 'self' data:",
    "font-src 'self'",
    "connect-src 'self'",
    "object-src 'none'",
    "base-uri 'none'",
    "form-action 'self'",
    "frame-ancestors 'none'",
  ].join("; "),
  "Cross-Origin-Opener-Policy": "same-origin",
  "Permissions-Policy": "camera=(), microphone=(), geolocation=()",
  "Referrer-Policy": "no-referrer",
  "X-Content-Type-Options": "nosniff",
  "X-Frame-Options": "DENY",
};

function responseHeaders(type: string): Headers {
  return new Headers({
    ...securityHeaders,
    "Cache-Control": "no-cache",
    "Content-Type": type,
  });
}

function getPort(): number {
  const raw = Bun.env.PORT ?? "3000";
  const port = Number(raw);

  if (!Number.isInteger(port) || port < 1 || port > 65_535) {
    throw new Error(`PORT inválida: ${raw}`);
  }

  return port;
}

const hostname = Bun.env.HOST ?? "127.0.0.1";

export const server = Bun.serve({
  hostname,
  port: getPort(),
  async fetch(request) {
    const url = new URL(request.url);

    if (url.pathname === "/api/playground/compile") {
      if (request.method !== "POST") {
        return new Response("Método não permitido\n", {
          status: 405,
          headers: new Headers({
            ...securityHeaders,
            Allow: "POST",
            "Content-Type": "text/plain; charset=utf-8",
          }),
        });
      }

      return Response.json(
        {
          ok: false,
          error: {
            code: "compiler_unavailable",
            message: "W ainda não possui compilador; nenhum código foi executado.",
          },
          contract: {
            accepts: { source: "string", target: "native | wasm" },
            futureAdapters: ["wasm", "remote-service"],
          },
        },
        {
          status: 501,
          headers: new Headers({
            ...securityHeaders,
            "Cache-Control": "no-store",
            "Content-Type": "application/json; charset=utf-8",
          }),
        },
      );
    }

    if (url.pathname === "/health") {
      if (request.method !== "GET" && request.method !== "HEAD") {
        return new Response("Método não permitido\n", {
          status: 405,
          headers: new Headers({
            ...securityHeaders,
            Allow: "GET, HEAD",
            "Content-Type": "text/plain; charset=utf-8",
          }),
        });
      }

      const body = request.method === "HEAD" ? null : JSON.stringify({ ok: true });
      return new Response(body, {
        status: 200,
        headers: new Headers({
          ...securityHeaders,
          "Cache-Control": "no-store",
          "Content-Type": "application/json; charset=utf-8",
        }),
      });
    }

    if (request.method !== "GET" && request.method !== "HEAD") {
      return new Response("Método não permitido\n", {
        status: 405,
        headers: new Headers({
          ...securityHeaders,
          Allow: "GET, HEAD",
          "Content-Type": "text/plain; charset=utf-8",
        }),
      });
    }

    const route = routes.get(url.pathname);
    if (!route) {
      return new Response("Não encontrado\n", {
        status: 404,
        headers: new Headers({
          ...securityHeaders,
          "Content-Type": "text/plain; charset=utf-8",
        }),
      });
    }

    const file = Bun.file(join(root, route.file));
    if (!(await file.exists())) {
      return new Response("Arquivo indisponível\n", {
        status: 500,
        headers: new Headers({
          ...securityHeaders,
          "Content-Type": "text/plain; charset=utf-8",
        }),
      });
    }

    return new Response(request.method === "HEAD" ? null : file, {
      status: 200,
      headers: responseHeaders(route.type),
    });
  },
});

console.log(`W Portal · http://${server.hostname}:${server.port}`);
