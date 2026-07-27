// Headless-browser smoke test for the web build (phase 5+).
// Drives Chrome over the DevTools protocol with plain node (no puppeteer):
// loads the page, collects console output, waits, screenshots the canvas.
// Usage: node browserTest.mjs [url] [waitMs] [outPrefix] [keyToSend]
import { spawn } from 'node:child_process';
import { writeFileSync } from 'node:fs';

const url = process.argv[2] ?? 'http://localhost:8080/engineBuild/';
const waitMs = Number(process.argv[3] ?? 12000);
const outPrefix = process.argv[4] ?? 'browserTest';
const keyToSend = process.argv[5] ?? '';   // e.g. "F8" (sent after load, repeatable "F8x3")

const chrome = 'C:/Program Files/Google/Chrome/Application/chrome.exe';
const port = 9222 + Math.floor(Math.random() * 500);

const proc = spawn(chrome, [
	'--headless=new', '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
	'--disable-gpu-sandbox', '--no-first-run', `--remote-debugging-port=${port}`,
	'--window-size=1400,980', 'about:blank',
], { stdio: 'ignore' });

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

// wait for the debugger endpoint
let targets = null;
for (let i = 0; i < 50 && !targets; ++i) {
	await sleep(200);
	try { targets = await (await fetch(`http://127.0.0.1:${port}/json`)).json(); } catch {}
}
if (!targets) { console.error('no CDP endpoint'); proc.kill(); process.exit(1); }
const page = targets.find(t => t.type === 'page');

const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);

let msgId = 0;
const pending = new Map();
const logs = [];
ws.onmessage = (ev) => {
	const m = JSON.parse(ev.data);
	if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result); pending.delete(m.id); }
	if (m.method === 'Runtime.consoleAPICalled')
		logs.push(m.params.args.map(a => a.value ?? a.description ?? '').join(' '));
	if (m.method === 'Log.entryAdded')
		logs.push(`[${m.params.entry.level}] ${m.params.entry.text}`);
};
const send = (method, params = {}) => new Promise(r => {
	const id = ++msgId;
	pending.set(id, r);
	ws.send(JSON.stringify({ id, method, params }));
});

await send('Runtime.enable');
await send('Log.enable');
await send('Page.enable');
await send('Page.navigate', { url });
await sleep(waitMs);

if (keyToSend) {
	const [key, countStr] = keyToSend.split('x');
	const count = Number(countStr ?? 1);
	const vk = key === 'F8' ? 119 : key === 'F1' ? 112 : key.charCodeAt(0);
	for (let i = 0; i < count; ++i) {
		// hold the key across several engine frames (software rendering can run at ~2fps)
		await send('Input.dispatchKeyEvent', { type: 'keyDown', key, code: key, windowsVirtualKeyCode: vk });
		await sleep(900);
		await send('Input.dispatchKeyEvent', { type: 'keyUp', key, code: key, windowsVirtualKeyCode: vk });
		await sleep(900);
	}
	await sleep(Number(process.argv[6] ?? 2500));	// post-key settle (software rendering is slow)
}

const shot = await send('Page.captureScreenshot', { format: 'png' });
writeFileSync(`${outPrefix}.png`, Buffer.from(shot.data, 'base64'));
writeFileSync(`${outPrefix}.log`, logs.join('\n'));

// pull any /dump_*.bmp files the build wrote into its in-memory FS
const evalJs = async (expr) => (await send('Runtime.evaluate', { expression: expr, returnByValue: true })).result?.value;
try {
	const names = await evalJs(`(function(){ try { return JSON.stringify(FS.readdir('/').filter(function(f){ return f.indexOf('dump_') === 0 && f.slice(-4) === '.bmp'; })); } catch (e) { return '[]'; } })()`);
	for (const f of JSON.parse(names ?? '[]')) {
		const b64 = await evalJs(`(function(){ var d = FS.readFile('/${f}'); var s = ''; for (var i = 0; i < d.length; i += 32768) s += String.fromCharCode.apply(null, d.subarray(i, i + 32768)); return btoa(s); })()`);
		if (b64) {
			writeFileSync(`${outPrefix}_${f.replace('dump_', '').replace(/\\.bmp$/, '')}.bmp`, Buffer.from(b64, 'base64'));
			console.log(`pulled ${f}`);
		}
	}
} catch (e) { console.error('dump pull failed:', e.message); }

console.log(`wrote ${outPrefix}.png + .log (${logs.length} console lines)`);
proc.kill();
process.exit(0);
