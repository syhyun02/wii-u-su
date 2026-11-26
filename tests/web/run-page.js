// Opens web/index.html in headless Chromium, runs a file and prints the log.
// usage: (scripts/serve-web.py 8080 &) ; node tests/web/run-page.js <game file> <screenshot.png>
const { chromium } = require('playwright');
(async () => {
  const browser = await chromium.launch({ args: ['--enable-features=SharedArrayBuffer'] });
  const page = await browser.newPage();
  page.on('console', (m) => console.log('[console]', m.text()));
  page.on('pageerror', (e) => console.log('[pageerror]', e.message));
  await page.goto('http://localhost:8080/?seconds=120');
  console.log('crossOriginIsolated =', await page.evaluate(() => self.crossOriginIsolated));
  await page.setInputFiles('#file', process.argv[2]);
  await page.click('#start');
  const t0 = Date.now();
  while (Date.now() - t0 < 180000) {
    const text = await page.$eval('#log', (e) => e.innerText);
    if (/HB: done|exited with code|abort|RuntimeError/.test(text)) break;
    await page.waitForTimeout(1000);
  }
  await page.waitForTimeout(2000);
  const text = await page.$eval('#log', (e) => e.innerText);
  console.log(text.split('\n').filter((l) => !/^\[headless\] t=/.test(l)).slice(-25).join('\n'));
  await page.screenshot({ path: process.argv[3], fullPage: true });
  await browser.close();
})();
