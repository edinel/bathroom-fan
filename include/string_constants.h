

// ---------------------------------------------------------------------------
// HTML + CSS (inline here; move to string_constants.h if it gets large)
// ---------------------------------------------------------------------------
static const char INDEX_HTML[] = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Bathroom Fan</title>
  <style>
    body { font-family: sans-serif; max-width: 480px; margin: 40px auto; padding: 0 16px; background: #f4f4f4; }
    h1   { font-size: 1.4em; color: #333; }
    .card { background: white; border-radius: 8px; padding: 16px; margin-bottom: 16px; box-shadow: 0 1px 4px rgba(0,0,0,0.1); }
    .stat { display: flex; justify-content: space-between; padding: 6px 0; border-bottom: 1px solid #eee; }
    .stat:last-child { border-bottom: none; }
    .label { color: #666; }
    .value { font-weight: bold; color: #222; }
    .mode-bar { display: flex; gap: 8px; margin-bottom: 12px; }
    .mode-bar button { flex: 1; padding: 10px; border: none; border-radius: 6px; cursor: pointer; font-size: 1em; }
    #btn-auto   { background: %AUTO_ACTIVE%;   color: white; }
    #btn-manual { background: %MANUAL_ACTIVE%; color: white; }
    .speed-buttons { display: flex; gap: 8px; flex-wrap: wrap; }
    .speed-buttons button { flex: 1; min-width: 60px; padding: 10px; border: none; border-radius: 6px;
                            background: #607d8b; color: white; cursor: pointer; font-size: 0.95em; }
    .speed-buttons button:hover { background: #455a64; }
    #manual-controls { display: %MANUAL_DISPLAY%; }
    #status-dot { display: inline-block; width: 10px; height: 10px; border-radius: 50%;
                  background: #4caf50; margin-right: 6px; }
  </style>
</head>
<body>
  <h1><span id="status-dot"></span>Bathroom Fan</h1>

  <div class="card">
    <div class="stat"><span class="label">Humidity</span>    <span class="value" id="humidity">%HUMIDITY%</span></div>
    <div class="stat"><span class="label">Temperature</span> <span class="value" id="temperature">%TEMPERATURE%</span></div>
    <div class="stat"><span class="label">Fan Speed</span>   <span class="value" id="duty">%DUTY%</span></div>
    <div class="stat"><span class="label">Fan RPM</span>     <span class="value" id="rpm">%RPM%</span></div>
  </div>

  <div class="card">
    <div class="mode-bar">
      <button id="btn-auto"   onclick="setMode('auto')">Auto</button>
      <button id="btn-manual" onclick="setMode('manual')">Manual</button>
    </div>
    <div id="manual-controls">
      <p style="color:#666; margin:0 0 8px">Set fan speed:</p>
      <div class="speed-buttons">
        <button onclick="setSpeed(0)">Off</button>
        <button onclick="setSpeed(25)">25%</button>
        <button onclick="setSpeed(50)">50%</button>
        <button onclick="setSpeed(75)">75%</button>
        <button onclick="setSpeed(100)">100%</button>
      </div>
    </div>
  </div>

  <script>
    function setMode(mode) {
      fetch('/update?mode=' + mode).then(() => refreshStatus());
    }
    function setSpeed(pct) {
      fetch('/update?duty=' + pct).then(() => refreshStatus());
    }
    function refreshStatus() {
      fetch('/status')
        .then(r => r.json())
        .then(d => {
          document.getElementById('humidity').textContent    = d.humidity.toFixed(1) + ' %';
          document.getElementById('temperature').textContent = d.temperature.toFixed(1) + ' C';
          document.getElementById('duty').textContent        = d.duty + ' %';
          document.getElementById('rpm').textContent         = d.rpm + ' RPM';
          const manual = d.manual;
          document.getElementById('btn-auto').style.background   = manual ? '#90a4ae' : '#4caf50';
          document.getElementById('btn-manual').style.background = manual ? '#f44336' : '#90a4ae';
          document.getElementById('manual-controls').style.display = manual ? 'block' : 'none';
        })
        .catch(() => { document.getElementById('status-dot').style.background = '#f44336'; });
    }
    setInterval(refreshStatus, 2000);
    refreshStatus();
  </script>
</body>
</html>
)rawhtml";
