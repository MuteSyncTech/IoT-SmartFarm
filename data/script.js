// ────────── Konfigurasi ThingSpeak ──────────
const channelID = '3399921';          // Menggunakan clientId Anda
const readAPIKey = 'WD4IS1013P562QN1'; // API Key untuk Membaca Data
const writeAPIKey = '1VI22AD11A0Y925L'; // API Key untuk Menulis Data (Opsional)

// ────────── Control Aktuator (Local Server / ESP) ──────────
async function sendPump(state) {
  try {
    await fetch(`/relay?device=pump&state=${state}`);
  } catch (error) {
    console.error('pump control failed:', error);
  }
}

async function sendFan(state) {
  try {
    await fetch(`/relay?device=fan&state=${state}`);
  } catch (error) {
    console.error('fan control failed:', error);
  }
}
async function setPumpMode(mode)
{
  try {
    await fetch(`/mode?device=pump&mode=${mode}`);
  } catch (error) {
    console.error('pump mode failed:', error);
  }
}

async function setFanMode(mode)
{
  try {
    await fetch(`/mode?device=fan&mode=${mode}`);
  } catch (error) {
    console.error('fan mode failed:', error);
  }
}

// ────────── Data Fetching dari ThingSpeak ──────────
async function fetchData() {
  try {
    const url = `https://api.thingspeak.com/channels/${channelID}/feeds/last.json?api_key=${readAPIKey}`;
    const res = await fetch(url);
    const json = await res.json();

    const temperature = parseFloat(json.field1) || 0; 
    const humidity = parseFloat(json.field2) || 0;    
    const moisture = parseFloat(json.field3) || 0;    

    // --- TAMBAHKAN 2 BARIS INI UNTUK UPDATE TEKS DI STAT CARD ATAS ---
    document.getElementById('temp-display').innerHTML = `${temperature.toFixed(1)}<span class="unit">°C</span>`;
    document.getElementById('hum-display').innerHTML = `${humidity.toFixed(0)}<span class="unit">%</span>`;
    // -----------------------------------------------------------------

    updateGauge(moisture);
    updateChart(temperature, humidity);
    console.log('Data sukses diambil dari ThingSpeak:', json);
  } catch (e) {
    console.error('Fetch dari ThingSpeak gagal:', e);
  }
}

// Interval 15 detik sekali agar aman dari pembatasan (rate-limiting) ThingSpeak
setInterval(fetchData, 15000); 
window.onload = fetchData;

// ────────── Speedometer-style Gauge (Kelembapan Tanah) ──────────
const gaugeCanvas = document.getElementById('radialGauge');
let gaugeChart;
if (gaugeCanvas) {
  const ctx = gaugeCanvas.getContext('2d');
  gaugeChart = new Chart(ctx, {
    type: 'doughnut',
    data: {
      datasets: [{
        data: [0, 100],
        backgroundColor: ['#007bff','#eee'],
        needleValue: 0
      }]
    },
    options: {
      rotation: 270,
      circumference: 180,
      cutout: '60%',
      responsive: false,
      plugins: { legend: { display: false } }
    },
    plugins: [{
      id: 'gauge-needle',
      afterDraw(chart) {
        const {ctx, chartArea: {width, height}, config: {data}} = chart;
        let value = data.datasets[0].needleValue;
        const total = data.datasets[0].data.reduce((a,b)=>a+b,0);
        const angle = Math.PI * (1 + value/total);
        const cx = width/2;
        const cy = height;
        ctx.save();
        ctx.translate(cx, cy);
        ctx.rotate(angle);
        ctx.beginPath();
        ctx.moveTo(0, -5);
        ctx.lineTo((height - 20), 0);
        ctx.lineTo(0, 5);
        ctx.fillStyle = '#444';
        ctx.fill();
        ctx.restore();
        
        ctx.beginPath();
        ctx.arc(cx, cy, 5, 0, 2*Math.PI);
        ctx.fill();
        
        ctx.font = '600 16px "Plus Jakarta Sans", sans-serif';
        ctx.fillStyle = '#18181b';
        ctx.textAlign = 'center';
        ctx.fillText(value.toFixed(1) + '%', cx, cy - 30);
      }
    }]
  });
}

function updateGauge(val) {
  if (!gaugeChart) return;
  val = Number(val);
  if (val > 100) val = 100;
  if (val < 0) val = 0;
  
  gaugeChart.data.datasets[0].data[0] = val;
  gaugeChart.data.datasets[0].data[1] = 100 - val;
  gaugeChart.data.datasets[0].needleValue = val;
  gaugeChart.update();
}

// ────────── Line Chart: Temperature & Humidity ──────────
const lineCtx = document.getElementById('lineChart');
let lineChart;
if (lineCtx) {
  const ctx = lineCtx.getContext('2d');
  lineChart = new Chart(ctx, {
    type: 'line',
    data: { labels: [], datasets: [
      { label: 'Suhu (°C)', borderColor: '#ef4444', data: [], fill: false, tension: 0.2 },
      { label: 'Kelembapan (%)', borderColor: '#3b82f6', data: [], fill: false, tension: 0.2 }
    ]},
    options: { 
      responsive: true, 
      scales: { 
        x: { display: false }, 
        y: { beginAtZero: true } 
      } 
    }
  });
}

function updateChart(temp, hum) {
  if (!lineChart) return;
  const t = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  lineChart.data.labels.push(t);
  lineChart.data.datasets[0].data.push(temp);
  lineChart.data.datasets[1].data.push(hum);
  
  if (lineChart.data.labels.length > 20) {
    lineChart.data.labels.shift();
    lineChart.data.datasets.forEach(ds => ds.data.shift());
  }
  lineChart.update();
}