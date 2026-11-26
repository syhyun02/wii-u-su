// Runs the Cemu WebAssembly module off the page's main thread.
// Messages in:  {type:'start', files:[File...] (with relativePath), entry:'<path inside /game>', args:[...]}
// Messages out: {type:'log', text}, {type:'exit', code}, {type:'error', text}
importScripts('cemu-web.js');

let started = false;
self.onmessage = async (e) => {
  const msg = e.data;
  if (msg.type !== 'start' || started) return;
  started = true;
  const post = (type, text) => self.postMessage({ type, text });
  try {
    const Module = await createCemu({
      print: (t) => post('log', t),
      printErr: (t) => post('log', t),
      locateFile: (p) => p,
      mainScriptUrlOrBlob: 'cemu-web.js', // pthread workers must load the module, not this script
      onExit: (code) => self.postMessage({ type: 'exit', code }),
      onAbort: (what) => post('error', 'abort: ' + what),
    });
    const FS = Module.FS;
    // the game: mounted in place (WORKERFS reads the File objects lazily, in chunks)
    FS.mkdir('/game');
    FS.mount(Module.WORKERFS, { blobs: msg.files.map((f) => ({ name: f.path, data: f.file })) }, '/game');
    // emulator home (settings, mlc01, saves): kept in IndexedDB
    FS.mkdir('/cemu');
    FS.mount(Module.IDBFS, {}, '/cemu');
    await new Promise((res) => FS.syncfs(true, () => res()));
    setInterval(() => FS.syncfs(false, () => {}), 10000);
    Module.ENV.CEMU_HOME = '/cemu/home';
    Module.ENV.CEMU_DATA = '/cemu-data';
    if (msg.runSeconds) Module.ENV.CEMU_RUN_SECONDS = String(msg.runSeconds);
    post('log', `[web] starting ${msg.entry}`);
    Module.callMain(['-g', '/game/' + msg.entry, '--forward-console-logging', ...(msg.args || [])]);
  } catch (err) {
    post('error', String(err && err.stack || err));
  }
};
