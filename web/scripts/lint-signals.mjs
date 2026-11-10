/**
 * Lint for a Qwik 2.0.0-rc.4 pitfall: in a JSX attribute or child such as
 *   disabled={!dirty || busy.value}
 * the optimizer hoists the expression into a derived signal that captures the
 * render-time constant (`dirty`) once. When a handler changes `busy` and the
 * value behind `dirty` together, the DOM keeps the stale `dirty`.
 * Fix: read the signal into a const in the component body and use that.
 *
 * Flags JSX expressions (outside on*$ handlers) that read `<signal>.value` and
 * also use a const declared in the same component body. Expressions with calls
 * or JSX are evaluated plainly on every render, so they are skipped.
 *
 * Usage: node scripts/lint-signals.mjs src/pages/*.tsx ...
 */
import { readFileSync } from "node:fs";
import ts from "typescript";

const SIGNAL_HOOK = /^(useSignal|useComputed\$)$/;
const OTHER_HOOK = /^(useStore|useContext|\$|useTask\$|useVisibleTask\$|component\$)$/;
let problems = 0;

function calleeName(init) {
  return init && ts.isCallExpression(init) && ts.isIdentifier(init.expression) ? init.expression.text : null;
}

function bindingNames(name, out) {
  if (ts.isIdentifier(name)) out.push(name.text);
  else if (ts.isObjectBindingPattern(name) || ts.isArrayBindingPattern(name))
    for (const e of name.elements) if (!ts.isOmittedExpression(e)) bindingNames(e.name, out);
  return out;
}

function checkComponent(src, file, fn) {
  if (!fn.body || !ts.isBlock(fn.body)) return;
  const signals = new Set();
  const locals = new Set();
  for (const st of fn.body.statements) {
    if (!ts.isVariableStatement(st)) continue;
    for (const d of st.declarationList.declarations) {
      const callee = calleeName(d.initializer);
      const names = bindingNames(d.name, []);
      if (callee && SIGNAL_HOOK.test(callee)) names.forEach((n) => signals.add(n));
      else if (!(callee && OTHER_HOOK.test(callee))) names.forEach((n) => locals.add(n));
    }
  }

  const visit = (node) => {
    if (ts.isJsxExpression(node) && node.expression) {
      const attr = ts.isJsxAttribute(node.parent) ? node.parent.name.getText() : "";
      if (!attr.endsWith("$")) {
        let readsSignal = false;
        let plain = false;
        const used = new Set();
        const scan = (m) => {
          if (ts.isCallExpression(m) || ts.isJsxElement(m) || ts.isJsxSelfClosingElement(m) || ts.isJsxFragment(m) || ts.isArrowFunction(m) || ts.isFunctionExpression(m)) {
            plain = true;
            return;
          }
          if (ts.isPropertyAccessExpression(m) && m.name.text === "value" && ts.isIdentifier(m.expression) && signals.has(m.expression.text)) readsSignal = true;
          if (ts.isIdentifier(m) && locals.has(m.text) && !(ts.isPropertyAccessExpression(m.parent) && m.parent.name === m)) used.add(m.text);
          ts.forEachChild(m, scan);
        };
        scan(node.expression);
        if (readsSignal && used.size && !plain) {
          const { line } = src.getLineAndCharacterOfPosition(node.getStart());
          console.log(`${file}:${line + 1}: signal read next to render-time ${[...used].join(", ")} in {${node.expression.getText().replace(/\s+/g, " ").slice(0, 100)}}`);
          problems++;
        }
      }
    }
    ts.forEachChild(node, visit);
  };
  visit(fn.body);
}

for (const file of process.argv.slice(2)) {
  const src = ts.createSourceFile(file, readFileSync(file, "utf8"), ts.ScriptTarget.Latest, true, ts.ScriptKind.TSX);
  const find = (n) => {
    if (ts.isCallExpression(n) && ts.isIdentifier(n.expression) && n.expression.text === "component$") {
      const fn = n.arguments[0];
      if (fn && (ts.isArrowFunction(fn) || ts.isFunctionExpression(fn))) checkComponent(src, file, fn);
    }
    ts.forEachChild(n, find);
  };
  find(src);
}

if (problems) {
  console.log(`\n${problems} expression(s): read the signal into a const in the component body instead.`);
  process.exit(1);
}
console.log("lint-signals: ok");
