//prueba la interfaz real contra Crow, sin respuestas simuladas
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { mkdtemp, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
const { chromium } = await import(process.env.MIA_PLAYWRIGHT_MODULE || 'playwright');
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const base = await mkdtemp(resolve(tmpdir(), 'mia-browser-'));
const api = 'http://127.0.0.1:2615';
const front = 'http://127.0.0.1:5175';
const checks = [];
const server = spawn(resolve(root, 'backend/build/202405047_ext2_backend'), [], { cwd: root, env: { ...process.env, MIA_PORT: '2615', MIA_DISK_DIR: base, MIA_REPORT_DIR: base, MIA_INPUT_DIR: base, MIA_CORS_ORIGIN: front }, stdio: 'ignore' });
const vite = spawn(process.execPath, ['node_modules/vite/bin/vite.js', '--host', '127.0.0.1', '--port', '5175', '--strictPort'], { cwd: resolve(root, 'frontend'), env: { ...process.env, VITE_API_URL: api }, stdio: 'ignore' });
let browser;
let status = 'failed';
async function ready(url) {
    for (let attempt = 0; attempt < 80; attempt++) {
        try { if ((await fetch(url)).ok) return; } catch {}
        await new Promise(resolve => setTimeout(resolve, 100));
    }
    throw new Error(`Servidor no responde: ${url}`);
}
try {
    await ready(api+'/api/health'); await ready(front);
    browser = await chromium.launch({ executablePath: process.env.MIA_CHROMIUM_PATH || '/usr/bin/chromium', headless: true, args: ['--no-sandbox'] });
    const context = await browser.newContext();
    const page = await context.newPage();
    const failures = [];
    page.on('pageerror', error => failures.push(error.message));
    await page.goto(front);
    await page.getByText('Sin discos. Cree uno mediante MKDISK.').waitFor();
    async function execute(text) {
        await page.locator('textarea').fill(text);
        const response = page.waitForResponse(response => response.url() === api+'/api/analyze');
        await page.getByRole('button', { name: 'Analizar Comandos' }).click();
        const result = await (await response).json();
        assert.equal(result.success, true, result.messages.join('\n'));
        await page.getByRole('button', { name: 'Analizar Comandos' }).waitFor({ state:'visible' });
        return result;
    }
    const script = '# demo smia\nmkdisk -size=4 -path=web.mia\nfdisk -size=3 -unit=M -path=web.mia -name=p\nmount -path=web.mia -name=p\nmkfs -id=471A -fs=3fs';
    await page.locator('input[type=file]').setInputFiles({ name: 'demo.smia', mimeType: 'text/plain', buffer: Buffer.from(script) });
    assert.equal(await page.locator('textarea').inputValue(), script);
    await execute(script); checks.push('script smia, comentarios y terminal');
    await page.getByRole('button', { name: /web.mia/ }).click();
    await page.getByRole('button', { name: 'p', exact:true }).waitFor();
    await page.getByLabel('Usuario', { exact:true }).fill('root');
    await page.getByLabel('Contraseña', { exact:true }).fill('incorrecta');
    await page.locator('.login-form select').selectOption('471A');
    await page.getByRole('button', { name:'Iniciar sesión' }).click();
    await page.getByRole('alert').filter({ hasText: /incorrectos/ }).waitFor();
    checks.push('login incorrecto muestra error');
    await page.getByLabel('Contraseña', { exact:true }).fill('123');
    await page.getByRole('button', { name:'Iniciar sesión' }).click();
    await page.getByRole('button', { name:'Cerrar sesión' }).waitFor();
    await execute('mkdir -path=/docs\nmkfile -path=/docs/a.txt -size=25');
    await page.getByRole('button', { name:'docs', exact:true }).waitFor();
    await page.getByRole('button', { name:'docs', exact:true }).click();
    await page.getByRole('button', { name:'a.txt', exact:true }).click();
    await page.locator('.file-content').filter({ hasText:'0123456789012345678901234' }).waitFor();
    checks.push('discos, particiones, carpetas y lectura');
    await execute('mkfile -path=/docs/a.txt -size=3');
    await page.getByLabel('Confirme la operación [y/n]:').fill('y');
    await page.getByRole('button', { name:'Responder' }).click();
    await page.getByLabel('Confirme la operación [y/n]:').waitFor({ state:'hidden' });
    await page.getByRole('button', { name:'a.txt', exact:true }).click();
    await page.locator('.file-content').filter({ hasText:'012' }).waitFor();
    checks.push('confirmación web de sobrescritura');
    await page.getByRole('button', { name:'Journaling', exact:true }).click();
    await page.getByRole('cell', { name:'mkdir', exact:true }).waitFor();
    assert.equal(await page.getByRole('cell', { name:'mkfile', exact:true }).count(), 2);
    checks.push('journal visible con tres operaciones');
    await execute('rep -name=tree -id=471A -path=tree.svg\nrep -name=bm_inode -id=471A -path=before.txt');
    await page.getByRole('button', { name:'Reportes', exact:true }).click();
    await page.getByRole('button', { name:'tree.svg', exact:true }).click();
    await page.locator('iframe').waitFor();
    const frame = await page.locator('iframe').elementHandle();
    await (await frame.contentFrame()).locator('svg').waitFor();
    checks.push('reporte Graphviz real en iframe');
    const second = await browser.newContext();
    const other = await second.newPage();
    await other.goto(front);
    await other.getByRole('button', { name:'Iniciar sesión' }).waitFor();
    assert.equal(await other.getByRole('button', { name:'Cerrar sesión' }).count(), 0);
    checks.push('dos navegadores con sesiones independientes');
    await page.getByRole('button', { name:'Cerrar sesión' }).click();
    await page.getByRole('button', { name:'Iniciar sesión' }).waitFor();
    assert.equal(await page.locator('.file-content').count(), 0);
    checks.push('logout limpia contenido y sesión');
    await execute('loss -id=471A\nrep -name=bm_inode -id=471A -path=after.txt');
    await page.getByRole('button', { name:'Journaling', exact:true }).click();
    await page.getByRole('cell', { name:'mkdir', exact:true }).waitFor();
    checks.push('journal consultable en React después de LOSS');
    await page.getByRole('button', { name:'Reportes', exact:true }).click();
    await page.getByRole('button', { name:'after.txt', exact:true }).waitFor();
    checks.push('bitmap posterior a LOSS visible');
    await page.setViewportSize({ width:390, height:844 });
    assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth));
    await page.screenshot({ path:resolve(root,'tests/evidence/frontend-mobile.png'),fullPage:true });
    await page.setViewportSize({ width:1280, height:1000 });
    await page.screenshot({ path:resolve(root,'tests/evidence/frontend.png'),fullPage:true });
    checks.push('vista móvil sin desbordamiento');
    assert.deepEqual(failures, []);
    checks.push('sin errores JavaScript');
    status = 'passed';
    console.log(`PASS navegador: ${checks.length} flujos. Datos: ${base}`);
} finally {
    if (browser) await browser.close();
    vite.kill('SIGTERM'); server.kill('SIGTERM');
    await writeFile(resolve(root,'tests/RESULTADOS_FRONTEND.json'),JSON.stringify({ status, checks, base },null,2));
}
