const { app } = require('electron');
app.disableHardwareAcceleration();
app.whenReady().then(async () => {
  try {
    const { probeAddon } = await import('./addon-probe.mjs');
    console.log('ADDON_RESULT=' + JSON.stringify(await probeAddon()));
    app.exit(0);
  } catch (error) { console.error(error.stack); app.exit(1); }
});
