// ============================================================
// frontend/js/bundle.js
// All JS merged — no ES modules, works with file://
// Owner: Simran Negi
// ============================================================

// CONFIG
const API_BASE  = 'http://localhost:8080/api';
const DEMO_MODE = false;

// MOCK DATA
let MOCK_WEBSITES = [
    { website_id:1, user_id:1, url:'https://google.com', name:'Google', check_interval_min:5, is_active:true, current_status:'UP', last_http_code:200, last_response_ms:118, ssl_expiry_days:365, last_checked_at:new Date(Date.now()-3*60000).toISOString(), uptime_pct_7d:99.97, avg_response_ms_7d:124, total_checks_7d:2016, downtime_incidents_7d:0, alerts_7d:0, ssl_warning:false },
    { website_id:2, user_id:1, url:'https://github.com', name:'GitHub', check_interval_min:10, is_active:true, current_status:'UP', last_http_code:200, last_response_ms:243, ssl_expiry_days:200, last_checked_at:new Date(Date.now()-8*60000).toISOString(), uptime_pct_7d:99.85, avg_response_ms_7d:251, total_checks_7d:1008, downtime_incidents_7d:1, alerts_7d:1, ssl_warning:false },
    { website_id:3, user_id:1, url:'https://stackoverflow.com', name:'Stack Overflow', check_interval_min:15, is_active:true, current_status:'DOWN', last_http_code:503, last_response_ms:0, ssl_expiry_days:180, last_checked_at:new Date(Date.now()-12*60000).toISOString(), uptime_pct_7d:97.20, avg_response_ms_7d:318, total_checks_7d:672, downtime_incidents_7d:20, alerts_7d:5, ssl_warning:false },
    { website_id:4, user_id:1, url:'https://wikipedia.org', name:'Wikipedia', check_interval_min:10, is_active:true, current_status:'UP', last_http_code:200, last_response_ms:321, ssl_expiry_days:90, last_checked_at:new Date(Date.now()-6*60000).toISOString(), uptime_pct_7d:99.90, avg_response_ms_7d:309, total_checks_7d:1008, downtime_incidents_7d:1, alerts_7d:1, ssl_warning:false },
    { website_id:5, user_id:1, url:'https://openai.com', name:'OpenAI', check_interval_min:5, is_active:true, current_status:'TIMEOUT', last_http_code:0, last_response_ms:30000, ssl_expiry_days:120, last_checked_at:new Date(Date.now()-4*60000).toISOString(), uptime_pct_7d:94.10, avg_response_ms_7d:780, total_checks_7d:2016, downtime_incidents_7d:119, alerts_7d:8, ssl_warning:false },
    { website_id:6, user_id:1, url:'https://example-test.xyz', name:'Test Down Site', check_interval_min:5, is_active:true, current_status:'DOWN', last_http_code:0, last_response_ms:0, ssl_expiry_days:12, last_checked_at:new Date(Date.now()-2*60000).toISOString(), uptime_pct_7d:0.00, avg_response_ms_7d:0, total_checks_7d:2016, downtime_incidents_7d:2016, alerts_7d:2016, ssl_warning:true }
];

const MOCK_SUMMARY = { user_id:1, total_sites:6, sites_up:3, sites_down:2, sites_unknown:0, overall_uptime_pct:98.50, alerts_last_24h:7, alerts_last_7d:15 };

const MOCK_UPTIME = [
    { website_id:1, name:'Google',        uptime_pct:99.97, avg_response_ms:124 },
    { website_id:2, name:'GitHub',         uptime_pct:99.85, avg_response_ms:251 },
    { website_id:4, name:'Wikipedia',      uptime_pct:99.90, avg_response_ms:309 },
    { website_id:3, name:'Stack Overflow', uptime_pct:97.20, avg_response_ms:318 },
    { website_id:5, name:'OpenAI',         uptime_pct:94.10, avg_response_ms:780 },
    { website_id:6, name:'Test Down Site', uptime_pct:0.00,  avg_response_ms:0   }
];

const MOCK_ALERTS = [
    { alert_id:1, website_id:3, website_name:'Stack Overflow', alert_type:'DOWN',     message:'Stack Overflow returned HTTP 503',   sent_at:new Date(Date.now()-10*60000).toISOString(), badge_class:'danger',    acknowledged:false },
    { alert_id:2, website_id:6, website_name:'Test Down Site', alert_type:'DOWN',     message:'example-test.xyz is unreachable',    sent_at:new Date(Date.now()-18*60000).toISOString(), badge_class:'danger',    acknowledged:false },
    { alert_id:3, website_id:6, website_name:'Test Down Site', alert_type:'SSL_EXPIRY',message:'SSL certificate expires in 12 days',sent_at:new Date(Date.now()-25*60000).toISOString(), badge_class:'warning',   acknowledged:false },
    { alert_id:4, website_id:5, website_name:'OpenAI',         alert_type:'TIMEOUT',  message:'openai.com timed out after 30s',     sent_at:new Date(Date.now()-35*60000).toISOString(), badge_class:'secondary', acknowledged:true  },
    { alert_id:5, website_id:4, website_name:'Wikipedia',      alert_type:'DOWN',     message:'wikipedia.org returned HTTP 503',    sent_at:new Date(Date.now()-2*3600000).toISOString(),badge_class:'danger',    acknowledged:true  }
];

let MOCK_SYSTEM_HEALTH = { id:1, cpu_usage_pct:23.4, mem_usage_pct:41.2, active_threads:8, queue_depth:3, recorded_at:new Date().toISOString(), cpu_status:'healthy', mem_status:'healthy' };

const MOCK_SSL_WARNINGS = [
    { website_id:6, name:'Test Down Site', url:'https://example-test.xyz', days_remaining:12, urgency:'critical' },
    { website_id:4, name:'Wikipedia',      url:'https://wikipedia.org',     days_remaining:90, urgency:'medium'   }
];

function generateMockHistory() {
    const h = [];
    for (let i = 59; i >= 0; i--) {
        h.push({ cpu_usage_pct: Math.round((15 + Math.random()*30)*10)/10, mem_usage_pct: Math.round((38 + Math.random()*10)*10)/10, recorded_at: new Date(Date.now()-i*30000).toISOString() });
    }
    return h;
}

// Live fluctuation in demo mode (only when DEMO_MODE)
if (DEMO_MODE) setInterval(() => {
    MOCK_SYSTEM_HEALTH.cpu_usage_pct = Math.round((15 + Math.random()*35)*10)/10;
    MOCK_SYSTEM_HEALTH.mem_usage_pct = Math.round((38 + Math.random()*12)*10)/10;
    MOCK_SYSTEM_HEALTH.recorded_at   = new Date().toISOString();
}, 5000);

// HTTP HELPER
async function http(method, path, body=null) {
    const opts = { method, headers:{'Content-Type':'application/json'}, credentials:'include' };
    if (body) opts.body = JSON.stringify(body);
    const res  = await fetch(API_BASE + path, opts);
    const ct   = res.headers.get('content-type') || '';
    const data = ct.includes('application/json') ? await res.json() : null;
    if (res.status === 401) {
        const err = new Error(data?.error || 'Session expired');
        err.code = 'UNAUTH';
        throw err;
    }
    if (!res.ok) throw new Error(data?.error || 'HTTP ' + res.status);
    return data;
}

// API FUNCTIONS
async function apiGetWebsites()      { return DEMO_MODE ? MOCK_WEBSITES         : await http('GET','/websites'); }
async function apiGetAlerts()        { return DEMO_MODE ? MOCK_ALERTS           : await http('GET','/alerts'); }
async function apiGetSummary()       { return DEMO_MODE ? MOCK_SUMMARY          : await http('GET','/analytics/summary'); }
async function apiGetUptimeStats()   { return DEMO_MODE ? MOCK_UPTIME           : await http('GET','/analytics/uptime'); }
async function apiGetSystemHealth()  { return DEMO_MODE ? MOCK_SYSTEM_HEALTH    : await http('GET','/system-metrics'); }
async function apiGetSystemHistory() { return DEMO_MODE ? generateMockHistory() : await http('GET','/system-metrics/history'); }
async function apiGetSSLWarnings()   { return DEMO_MODE ? MOCK_SSL_WARNINGS     : await http('GET','/websites/ssl-warnings'); }

async function apiLogin(username, password) {
    if (DEMO_MODE) return { id:1, username, role:'user' };
    const res = await http('POST', '/users/login', { username, password });
    return res.user || res;
}

async function apiRegister(username, email, password, confirmPassword) {
    if (DEMO_MODE) return { id:Date.now(), username, email, role:'user' };
    const res = await http('POST', '/users/register', { username, email, password, confirmPassword });
    return res.user || res;
}

async function apiForgotPassword(email) {
    if (DEMO_MODE) return { message:'If that email is registered, a temporary password has been generated and sent to the account owner.' };
    return http('POST', '/users/forgot-password', { email });
}

async function apiAddWebsite(data) {
    if (DEMO_MODE) {
        const s = { website_id:Date.now(), user_id:1, url:data.url, name:data.name, check_interval_min:parseInt(data.check_interval_min)||5, is_active:true, current_status:'UNKNOWN', last_http_code:0, last_response_ms:0, ssl_expiry_days:-1, last_checked_at:null, uptime_pct_7d:0, avg_response_ms_7d:0, total_checks_7d:0, downtime_incidents_7d:0, alerts_7d:0, ssl_warning:false };
        MOCK_WEBSITES.push(s); return s;
    }
    return http('POST', '/websites', data);
}

async function apiDeleteWebsite(id) {
    if (DEMO_MODE) { MOCK_WEBSITES = MOCK_WEBSITES.filter(w => w.website_id !== id); return; }
    return http('DELETE', '/websites/' + id);
}

async function apiChangePassword(newPassword, confirmPassword) {
    if (DEMO_MODE) {
        if (newPassword !== confirmPassword) throw new Error('New passwords do not match');
        if (newPassword.length < 6) throw new Error('New password must be at least 6 characters');
        return { message: 'Password updated successfully' };
    }
    return http('POST', '/users/change-password', { newPassword, confirmPassword });
}

// APP STATE
const state = { user:null, websites:[], alerts:[], summary:{}, systemHealth:{}, refreshInterval:null };

// CHARTS
let responseChart=null, statusChart=null, cpuChart=null;
let detailsDoughnutChart=null, detailsAlertsChart=null;

function uptimeColor(pct) {
    if (pct >= 99) return '#2ecc71';
    if (pct >= 95) return '#f39c12';
    return '#e74c3c';
}

function renderUptimeBars(stats, containerId) {
    const c = document.getElementById(containerId || 'uptime-bar-list');
    if (!c) return;
    c.innerHTML = stats.map(s => {
        const pct = parseFloat(s.uptime_pct) || 0;
        const col = uptimeColor(pct);
        return '<div class="uptime-row">' +
            '<div class="uptime-site-name" title="' + escHtml(s.name) + '">' + escHtml(s.name) + '</div>' +
            '<div class="uptime-bar-track"><div class="uptime-bar-fill" style="width:' + pct + '%;background:' + col + '"></div></div>' +
            '<div class="uptime-pct" style="color:' + col + '">' + Number(pct).toFixed(2) + '%</div>' +
            '</div>';
    }).join('');
}

function renderResponseChart(stats) {
    const canvas = document.getElementById('chart-response');
    if (!canvas || typeof Chart === 'undefined') return;
    if (responseChart) { responseChart.destroy(); responseChart=null; }
    const labels = stats.map(s => s.name);
    const data   = stats.map(s => s.avg_response_ms || 0);
    const colors = data.map(v => v===0 ? 'rgba(74,85,104,0.5)' : v<300 ? 'rgba(46,204,113,0.7)' : v<600 ? 'rgba(243,156,18,0.7)' : 'rgba(231,76,60,0.7)');
    responseChart = new Chart(canvas, {
        type:'bar',
        data:{ labels, datasets:[{ label:'Avg Response (ms)', data, backgroundColor:colors, borderRadius:4 }] },
        options:{ indexAxis:'y', responsive:true, maintainAspectRatio:false, plugins:{legend:{display:false}}, scales:{ x:{ticks:{color:'#8892b0'},grid:{color:'rgba(46,50,80,0.5)'}}, y:{ticks:{color:'#8892b0'},grid:{display:false}} } }
    });
}

function renderStatusChart(summary) {
    const canvas = document.getElementById('chart-status');
    if (!canvas || typeof Chart === 'undefined') return;
    if (statusChart) { statusChart.destroy(); statusChart=null; }
    statusChart = new Chart(canvas, {
        type:'doughnut',
        data:{ labels:['UP','DOWN','UNKNOWN'], datasets:[{ data:[summary.sites_up||0, summary.sites_down||0, summary.sites_unknown||0], backgroundColor:['rgba(46,204,113,0.8)','rgba(231,76,60,0.8)','rgba(74,85,104,0.5)'], borderColor:['#2ecc71','#e74c3c','#4a5568'], borderWidth:2, hoverOffset:6 }] },
        options:{ responsive:true, maintainAspectRatio:false, cutout:'65%', plugins:{ legend:{ position:'bottom', labels:{ color:'#8892b0', padding:14, font:{size:11} } } } }
    });
}

function renderCpuChart(history) {
    const canvas = document.getElementById('chart-cpu');
    if (!canvas || typeof Chart === 'undefined') return;
    if (cpuChart) { cpuChart.destroy(); cpuChart=null; }
    const labels  = history.map(r => { const d=new Date(r.recorded_at); return String(d.getHours()).padStart(2,'0')+':'+String(d.getMinutes()).padStart(2,'0'); });
    const cpuData = history.map(r => r.cpu_usage_pct);
    const memData = history.map(r => r.mem_usage_pct);
    cpuChart = new Chart(canvas, {
        type:'line',
        data:{ labels, datasets:[
            { label:'CPU %', data:cpuData, borderColor:'#4f8ef7', backgroundColor:'rgba(79,142,247,0.08)', fill:true, tension:0.4, pointRadius:0, borderWidth:2 },
            { label:'RAM %', data:memData, borderColor:'#9b59b6', backgroundColor:'rgba(155,89,182,0.06)', fill:true, tension:0.4, pointRadius:0, borderWidth:2 }
        ]},
        options:{ responsive:true, maintainAspectRatio:false, interaction:{intersect:false,mode:'index'}, plugins:{legend:{position:'bottom',labels:{color:'#8892b0',font:{size:11},padding:12}}}, scales:{ x:{ticks:{color:'#4a5568',font:{size:10},maxTicksLimit:8,maxRotation:0},grid:{color:'rgba(46,50,80,0.4)'}}, y:{min:0,max:100,ticks:{color:'#8892b0',font:{size:10},callback:function(v){return v+'%'}},grid:{color:'rgba(46,50,80,0.4)'}} } }
    });
}

function renderGauge(elementId, pct, status) {
    const wrapper = document.getElementById(elementId);
    if (!wrapper) return;
    const r=32, circ=2*Math.PI*r, offset=circ-(pct/100)*circ;
    const col = status==='critical' ? '#e74c3c' : status==='warning' ? '#f39c12' : '#2ecc71';
    wrapper.innerHTML = '<svg width="80" height="80" viewBox="0 0 80 80" style="transform:rotate(-90deg)"><circle fill="none" stroke="#1a1d27" stroke-width="8" cx="40" cy="40" r="' + r + '"/><circle fill="none" stroke="' + col + '" stroke-width="8" stroke-linecap="round" cx="40" cy="40" r="' + r + '" stroke-dasharray="' + circ.toFixed(2) + '" stroke-dashoffset="' + offset.toFixed(2) + '" style="transition:stroke-dashoffset 0.8s ease"/></svg><div class="gauge-text">' + Number(pct).toFixed(0) + '%</div>';
}

// HELPERS
function setEl(id, val) { const e=document.getElementById(id); if(e) e.textContent=val; }
function escHtml(s) { return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;'); }
function timeAgo(iso) {
    if (!iso) return 'never';
    const d = Math.floor((Date.now()-new Date(iso))/1000);
    if (d<60) return d+'s ago';
    if (d<3600) return Math.floor(d/60)+'m ago';
    if (d<86400) return Math.floor(d/3600)+'h ago';
    return Math.floor(d/86400)+'d ago';
}
function showToast(msg, type) {
    type = type || 'info';
    const c = document.getElementById('toast-container');
    if (!c) return;
    const icons = { success:'✓', error:'✗', info:'ℹ' };
    const t = document.createElement('div');
    t.className = 'toast ' + type;
    t.innerHTML = '<span>' + (icons[type]||'ℹ') + '</span><span>' + escHtml(msg) + '</span>';
    c.appendChild(t);
    setTimeout(function(){ t.remove(); }, 4000);
}

// RENDERING
function renderStatsBar(s) {
    setEl('stat-total',  s.total_sites != null ? s.total_sites : '--');
    setEl('stat-up',     s.sites_up    != null ? s.sites_up    : '--');
    setEl('stat-down',   s.sites_down  != null ? s.sites_down  : '--');
    setEl('stat-uptime', s.overall_uptime_pct != null ? Number(s.overall_uptime_pct).toFixed(1)+'%' : '--');
    setEl('stat-alerts', s.alerts_last_24h != null ? s.alerts_last_24h : '--');
}

function websiteCardHTML(w) {
    const status  = (w.current_status || 'UNKNOWN').toLowerCase();
    const uptime  = parseFloat(w.uptime_pct_7d || 0);
    const checked = w.last_checked_at ? timeAgo(w.last_checked_at) : 'Never';
    var sslHTML = '';
    if (w.ssl_expiry_days >= 0) {
        const cls = w.ssl_warning ? (w.ssl_expiry_days<=7 ? 'critical' : 'warning') : 'ok';
        sslHTML = '<span class="ssl-badge ' + cls + '">&#x1F512; SSL ' + w.ssl_expiry_days + 'd</span>';
    }
    return '<div class="website-card status-' + status + '">' +
        '<div class="card-header">' +
            '<div>' +
                '<div class="card-name">' + escHtml(w.name) + '</div>' +
                '<div class="card-url">' + escHtml(w.url) + '</div>' +
            '</div>' +
            '<span class="status-badge ' + status + '"><span class="status-dot"></span>' + (w.current_status||'UNKNOWN') + '</span>' +
        '</div>' +
        '<div class="card-metrics">' +
            '<div class="metric-item"><div class="metric-val" style="color:' + uptimeColor(uptime) + '">' + Number(uptime).toFixed(1) + '%</div><div class="metric-lbl">Uptime</div></div>' +
            '<div class="metric-item"><div class="metric-val">' + (w.last_response_ms>0 ? w.last_response_ms+'ms' : '—') + '</div><div class="metric-lbl">Response</div></div>' +
            '<div class="metric-item"><div class="metric-val">' + (w.last_http_code||'—') + '</div><div class="metric-lbl">HTTP</div></div>' +
        '</div>' +
        '<div class="card-footer"><span>Checked ' + checked + '</span>' + sslHTML + '</div>' +
        '<div class="card-actions">' +
            '<button class="btn-sm btn-details-site" data-id="' + w.website_id + '">Details</button>' +
            '<button class="btn-sm danger btn-delete-site" data-id="' + w.website_id + '" data-name="' + escHtml(w.name) + '">Delete</button>' +
        '</div>' +
    '</div>';
}

function renderWebsiteCards(websites, containerId) {
    const c = document.getElementById(containerId || 'website-cards');
    if (!c) return;
    if (!websites.length) {
        c.innerHTML = '<div class="empty-state" style="grid-column:1/-1"><div class="empty-icon">&#x1F310;</div><p>No websites yet.<br>Click "Add Website" to start.</p></div>';
        return;
    }
    c.innerHTML = websites.map(function(w){ return websiteCardHTML(w); }).join('');
    c.querySelectorAll('.btn-delete-site').forEach(function(btn) {
        btn.addEventListener('click', function(){ confirmDelete(parseInt(btn.dataset.id), btn.dataset.name); });
    });
    c.querySelectorAll('.btn-details-site').forEach(function(btn) {
        btn.addEventListener('click', function(){ openSiteDetails(parseInt(btn.dataset.id)); });
    });
}

function renderAlertsTable(alerts, tbodyId) {
    const t = document.getElementById(tbodyId);
    if (!t) return;
    if (!alerts.length) {
        t.innerHTML = '<tr><td colspan="5" style="text-align:center;color:var(--text-muted);padding:24px">No alerts &#x1F389;</td></tr>';
        return;
    }
    t.innerHTML = alerts.map(function(a) {
        return '<tr>' +
            '<td>' + escHtml(a.website_name||'—') + '</td>' +
            '<td><span class="alert-badge ' + (a.badge_class||'info') + '">' + a.alert_type + '</span></td>' +
            '<td>' + escHtml(a.message||'—') + '</td>' +
            '<td>' + timeAgo(a.sent_at) + '</td>' +
            '<td>' + (a.acknowledged ? '<span style="color:var(--text-muted)">&#x2713; Ack</span>' : '<span style="color:var(--accent-yellow)">Pending</span>') + '</td>' +
        '</tr>';
    }).join('');
}

function renderSystemHealthWidget(h) {
    if (!h || h.cpu_usage_pct == null) return;
    renderGauge('gauge-cpu', h.cpu_usage_pct, h.cpu_status||'healthy');
    renderGauge('gauge-mem', h.mem_usage_pct, h.mem_status||'healthy');
    setEl('health-threads',  h.active_threads != null ? h.active_threads : '--');
    setEl('health-queue',    h.queue_depth    != null ? h.queue_depth    : '--');
    setEl('health-recorded', h.recorded_at ? timeAgo(h.recorded_at) : '--');
}

function renderSSLWarnings(list) {
    const c = document.getElementById('ssl-warnings-list');
    if (!c) return;
    if (!list.length) {
        c.innerHTML = '<div class="empty-state"><div class="empty-icon">&#x2705;</div><p>All SSL certificates are healthy.</p></div>';
        return;
    }
    c.innerHTML = list.map(function(s) {
        return '<div style="display:flex;align-items:center;justify-content:space-between;padding:10px 0;border-bottom:1px solid var(--border-color)">' +
            '<div><div style="font-size:0.85rem;color:var(--text-primary)">' + escHtml(s.name) + '</div><div style="font-size:0.75rem;color:var(--text-muted)">' + escHtml(s.url) + '</div></div>' +
            '<span class="ssl-badge ' + (s.urgency==='critical'?'critical':'warning') + '">&#x1F512; ' + s.days_remaining + 'd remaining</span>' +
        '</div>';
    }).join('');
}

// WEBSITE DETAILS MODAL
function openSiteDetails(websiteId) {
    const w = state.websites.find(function(x){ return x.website_id === websiteId; });
    if (!w) return;
    const modal = document.getElementById('modal-site-details');
    if (!modal) return;

    setEl('details-modal-title', 'Details — ' + w.name);

    // Status banner colour
    const status = (w.current_status || 'UNKNOWN').toLowerCase();
    const banner = document.getElementById('details-status-banner');
    if (banner) {
        var bg = 'rgba(74,85,104,0.08)', br = 'rgba(74,85,104,0.25)';
        if (status==='up')   { bg='rgba(46,204,113,0.08)';  br='rgba(46,204,113,0.25)'; }
        if (status==='down') { bg='rgba(231,76,60,0.08)';   br='rgba(231,76,60,0.25)';  }
        banner.style.background   = bg;
        banner.style.borderColor  = br;
    }
    const badge = document.getElementById('details-status-badge');
    if (badge) {
        badge.className   = 'status-badge ' + status;
        badge.innerHTML   = '<span class="status-dot"></span>' + (w.current_status||'UNKNOWN');
    }
    setEl('details-url',          w.url);
    setEl('details-last-checked', w.last_checked_at ? 'Last checked ' + timeAgo(w.last_checked_at) : 'Never checked');

    // Metrics
    var uptime = parseFloat(w.uptime_pct_7d || 0);
    var uptimeEl = document.getElementById('details-uptime');
    if (uptimeEl) { uptimeEl.textContent = Number(uptime).toFixed(2)+'%'; uptimeEl.style.color = uptimeColor(uptime); }

    setEl('details-response', w.avg_response_ms_7d > 0 ? w.avg_response_ms_7d+'ms' : '—');

    var httpEl = document.getElementById('details-http');
    if (httpEl) {
        httpEl.textContent = w.last_http_code || '—';
        httpEl.style.color = w.last_http_code===200 ? '#2ecc71' : w.last_http_code>0 ? '#f39c12' : '#e74c3c';
    }
    var sslEl = document.getElementById('details-ssl');
    if (sslEl) {
        if (w.ssl_expiry_days >= 0) {
            sslEl.textContent = w.ssl_expiry_days + 'd';
            sslEl.style.color = w.ssl_warning ? (w.ssl_expiry_days<=7 ? '#e74c3c' : '#f39c12') : '#2ecc71';
        } else { sslEl.textContent='N/A'; sslEl.style.color='var(--text-muted)'; }
    }

    // Doughnut: uptime vs downtime
    var dCtx = document.getElementById('details-doughnut-chart');
    if (dCtx && typeof Chart !== 'undefined') {
        if (detailsDoughnutChart) { detailsDoughnutChart.destroy(); detailsDoughnutChart=null; }
        var upChecks   = Math.round((uptime/100) * (w.total_checks_7d||0));
        var downChecks = (w.total_checks_7d||0) - upChecks;
        detailsDoughnutChart = new Chart(dCtx, {
            type:'doughnut',
            data:{ labels:['Up','Down / Timeout'], datasets:[{ data:[upChecks, downChecks], backgroundColor:['rgba(46,204,113,0.8)','rgba(231,76,60,0.7)'], borderColor:['#2ecc71','#e74c3c'], borderWidth:2, hoverOffset:5 }] },
            options:{ responsive:true, maintainAspectRatio:false, cutout:'62%', plugins:{ legend:{position:'bottom',labels:{color:'#8892b0',padding:12,font:{size:11}}}, tooltip:{callbacks:{label:function(ctx){ return ' '+ctx.label+': '+ctx.parsed+' checks'; }}} } }
        });
    }

    // Bar: alerts by type
    var siteAlerts = state.alerts.filter(function(a){ return a.website_id===websiteId || a.website_name===w.name; });
    var alertCounts = { DOWN:0, TIMEOUT:0, SSL_EXPIRY:0, KEYWORD_MISSING:0 };
    siteAlerts.forEach(function(a){ if (alertCounts[a.alert_type]!==undefined) alertCounts[a.alert_type]++; });
    var aCtx = document.getElementById('details-alerts-chart');
    if (aCtx && typeof Chart !== 'undefined') {
        if (detailsAlertsChart) { detailsAlertsChart.destroy(); detailsAlertsChart=null; }
        detailsAlertsChart = new Chart(aCtx, {
            type:'bar',
            data:{ labels:Object.keys(alertCounts), datasets:[{ label:'Alerts (7d)', data:Object.values(alertCounts), backgroundColor:['rgba(231,76,60,0.75)','rgba(243,156,18,0.75)','rgba(155,89,182,0.75)','rgba(74,85,104,0.75)'], borderRadius:4 }] },
            options:{ responsive:true, maintainAspectRatio:false, plugins:{legend:{display:false}}, scales:{ x:{ticks:{color:'#8892b0',font:{size:10}},grid:{display:false}}, y:{ticks:{color:'#8892b0',font:{size:10},stepSize:1},grid:{color:'rgba(46,50,80,0.4)'},beginAtZero:true} } }
        });
    }

    // Stats grid
    var grid = document.getElementById('details-stats-grid');
    if (grid) {
        var rows = [
            ['Check Interval',        w.check_interval_min ? 'Every '+w.check_interval_min+' min' : '—'],
            ['Total Checks (7d)',      w.total_checks_7d || '0'],
            ['Downtime Incidents (7d)',w.downtime_incidents_7d || '0'],
            ['Alerts Fired (7d)',      w.alerts_7d || '0'],
            ['Last Response Time',     w.last_response_ms > 0 ? w.last_response_ms+' ms' : '—'],
            ['SSL Days Remaining',     w.ssl_expiry_days >= 0 ? w.ssl_expiry_days+' days' : 'No SSL / Unknown'],
            ['SSL Warning',            w.ssl_warning ? '⚠️ Yes — expiring soon' : '✅ No'],
            ['Current Status',         w.current_status || 'UNKNOWN']
        ];
        grid.innerHTML = rows.map(function(r) {
            return '<div style="display:flex;justify-content:space-between;padding:6px 0;border-bottom:1px solid var(--border-color)">' +
                '<span style="color:var(--text-muted)">' + r[0] + '</span>' +
                '<span style="color:var(--text-primary);font-weight:500">' + escHtml(String(r[1])) + '</span>' +
            '</div>';
        }).join('');
    }

    modal.classList.add('open');
}

// NAVIGATION
function navigateTo(pageId) {
    document.querySelectorAll('.nav-item').forEach(function(i){ i.classList.remove('active'); });
    document.querySelectorAll('.page').forEach(function(p){ p.classList.remove('active'); });
    var nav  = document.querySelector('.nav-item[data-page="' + pageId + '"]');
    var page = document.getElementById('page-' + pageId);
    if (nav)  nav.classList.add('active');
    if (page) page.classList.add('active');
    if (pageId==='overview')  renderOverview();
    if (pageId==='websites')  renderWebsiteCards(state.websites, 'all-website-cards');
    if (pageId==='alerts')    renderAlertsTable(state.alerts, 'alerts-full-table');
    if (pageId==='analytics') renderAnalyticsPage();
    if (pageId==='system')    renderSystemPage();
}

function renderOverview() {
    renderStatsBar(state.summary);
    renderWebsiteCards(state.websites.slice(0,6), 'website-cards');
    apiGetUptimeStats().then(function(s){ renderUptimeBars(s,'uptime-bar-list'); renderResponseChart(s); });
    renderStatusChart(state.summary);
    renderAlertsTable(state.alerts.slice(0,5), 'alerts-recent-table');
    renderSystemHealthWidget(state.systemHealth);
}

async function renderAnalyticsPage() {
    var stats = await apiGetUptimeStats();
    renderUptimeBars(stats, 'analytics-uptime-bar-list');
    renderResponseChart(stats);
}

async function renderSystemPage() {
    renderSystemHealthWidget(state.systemHealth);
    var history = await apiGetSystemHistory();
    renderCpuChart(history);
    var ssl = await apiGetSSLWarnings();
    renderSSLWarnings(ssl);
}

// DATA LOADING
async function loadAllData() {
    try {
        var results = await Promise.all([apiGetWebsites(), apiGetAlerts(), apiGetSummary(), apiGetSystemHealth()]);
        state.websites    = results[0];
        state.alerts      = results[1];
        state.summary     = results[2];
        state.systemHealth= results[3];
        navigateTo('overview');
    } catch(e) {
        if (e.code === 'UNAUTH') { handleSessionExpired(); return; }
        showToast('Load error: ' + e.message, 'error');
    }
}

function handleSessionExpired() {
    sessionStorage.removeItem('wn_user');
    if (state.refreshInterval) { clearInterval(state.refreshInterval); state.refreshInterval = null; }
    state.user = null;
    showLogin();
    showToast('Your session expired. Please sign in again.', 'info');
}

function startAutoRefresh() {
    if (state.refreshInterval) clearInterval(state.refreshInterval);
    state.refreshInterval = setInterval(async function(){
        try {
            var r = await Promise.all([apiGetWebsites(), apiGetAlerts(), apiGetSummary(), apiGetSystemHealth()]);
            state.websites=r[0]; state.alerts=r[1]; state.summary=r[2]; state.systemHealth=r[3];
            var active = document.querySelector('.page.active');
            if (active) {
                var id = active.id.replace('page-','');
                if (id==='overview') renderOverview();
                if (id==='websites') renderWebsiteCards(state.websites, 'all-website-cards');
                if (id==='system')   renderSystemPage();
            }
        } catch(e) { if (e.code === 'UNAUTH') handleSessionExpired(); }
    }, 30000);
}

// DELETE
async function confirmDelete(id, name) {
    if (!confirm('Delete "' + name + '"? This cannot be undone.')) return;
    try {
        await apiDeleteWebsite(id);
        state.websites = await apiGetWebsites();
        renderWebsiteCards(state.websites, 'all-website-cards');
        renderWebsiteCards(state.websites.slice(0,6), 'website-cards');
        showToast('"' + name + '" deleted', 'info');
    } catch(e) { showToast('Delete failed: ' + e.message, 'error'); }
}

// AUTH SCREENS
function showLogin() {
    document.getElementById('login-screen').style.display    = 'flex';
    document.getElementById('register-screen').style.display = 'none';
    document.getElementById('forgot-screen').style.display   = 'none';
    document.getElementById('app-screen').style.display      = 'none';
}

function showRegister() {
    document.getElementById('login-screen').style.display    = 'none';
    document.getElementById('register-screen').style.display = 'flex';
    document.getElementById('forgot-screen').style.display   = 'none';
    document.getElementById('app-screen').style.display      = 'none';
    var e1 = document.getElementById('register-error');
    var e2 = document.getElementById('register-success');
    if (e1) { e1.textContent=''; e1.style.display='none'; }
    if (e2) { e2.textContent=''; e2.style.display='none'; }
}

function showForgot() {
    document.getElementById('login-screen').style.display    = 'none';
    document.getElementById('register-screen').style.display = 'none';
    document.getElementById('forgot-screen').style.display   = 'flex';
    document.getElementById('app-screen').style.display      = 'none';
    var errEl = document.getElementById('forgot-error');
    var resEl = document.getElementById('forgot-result');
    var form  = document.getElementById('forgot-form');
    if (errEl) { errEl.textContent=''; errEl.style.display='none'; }
    if (resEl) { resEl.innerHTML='';   resEl.style.display='none'; }
    if (form)  { form.style.display='block'; form.reset(); }
}

function showDashboard() {
    document.getElementById('login-screen').style.display    = 'none';
    document.getElementById('register-screen').style.display = 'none';
    document.getElementById('forgot-screen').style.display   = 'none';
    document.getElementById('app-screen').style.display      = 'grid';
    setEl('topbar-username', state.user && state.user.username ? state.user.username : 'User');
    loadAllData();
    startAutoRefresh();
}

// INIT
document.addEventListener('DOMContentLoaded', function() {

    // Sidebar navigation
    document.querySelectorAll('.nav-item[data-page]').forEach(function(item){
        item.addEventListener('click', function(){ navigateTo(item.dataset.page); });
    });

    // Generic modal open
    document.querySelectorAll('[data-open-modal]').forEach(function(btn){
        btn.addEventListener('click', function(){
            var m = document.getElementById(btn.dataset.openModal);
            if (m) m.classList.add('open');
        });
    });

    // Generic modal close
    document.querySelectorAll('[data-close-modal]').forEach(function(btn){
        btn.addEventListener('click', function(){
            var m = document.getElementById(btn.dataset.closeModal);
            if (m) m.classList.remove('open');
        });
    });

    // Click outside modal to close
    document.querySelectorAll('.modal-overlay').forEach(function(o){
        o.addEventListener('click', function(e){ if(e.target===o) o.classList.remove('open'); });
    });

    // ── Login form ────────────────────────────────────────────
    var loginForm = document.getElementById('login-form');
    if (loginForm) {
        loginForm.addEventListener('submit', async function(e) {
            e.preventDefault();
            var username = document.getElementById('login-username').value.trim();
            var password = document.getElementById('login-password').value;
            var errEl = document.getElementById('login-error');
            var btn   = loginForm.querySelector('[type="submit"]');
            errEl.style.display='none';
            btn.disabled=true; btn.textContent='Signing in…';
            try {
                state.user = await apiLogin(username, password);
                sessionStorage.setItem('wn_user', JSON.stringify(state.user));
                showDashboard();
            } catch(_) {
                errEl.textContent='Invalid username or password.';
                errEl.style.display='block';
            } finally { btn.disabled=false; btn.textContent='Sign In'; }
        });
    }

    // ── Forgot Password link ──────────────────────────────────
    var showForgotBtn = document.getElementById('show-forgot');
    if (showForgotBtn) {
        showForgotBtn.addEventListener('click', function(){ showForgot(); });
    }

    // ── Back to login from forgot screen ─────────────────────
    var backToLoginBtn = document.getElementById('forgot-back-to-login');
    if (backToLoginBtn) {
        backToLoginBtn.addEventListener('click', function(){ showLogin(); });
    }

    // ── Forgot Password form ──────────────────────────────────
    var forgotForm = document.getElementById('forgot-form');
    if (forgotForm) {
        forgotForm.addEventListener('submit', async function(e) {
            e.preventDefault();
            var email  = document.getElementById('forgot-email').value.trim();
            var errEl  = document.getElementById('forgot-error');
            var resEl  = document.getElementById('forgot-result');
            var btn    = forgotForm.querySelector('[type="submit"]');
            errEl.style.display='none'; resEl.style.display='none';
            btn.disabled=true; btn.textContent='Resetting…';
            try {
                var data = await apiForgotPassword(email);
                resEl.innerHTML =
                    '<div style="color:#2ecc71;font-weight:600;margin-bottom:6px;">&#x2705; Request received</div>' +
                    '<div style="font-size:0.85rem;color:var(--text-primary)">' + escHtml(data.message || 'If that email is registered, a temporary password has been generated.') + '</div>' +
                    '<div style="margin-top:8px;font-size:0.75rem;color:var(--text-muted)">Check with the account owner for the temporary password, then sign in and change it using the &#x1F511; Change Password button.</div>';
                resEl.style.display='block';
                forgotForm.style.display='none';
            } catch(err) {
                errEl.textContent = err.message || 'Password reset failed.';
                errEl.style.display='block';
            } finally { btn.disabled=false; btn.textContent='Reset Password'; }
        });
    }

    // ── Create Account link ───────────────────────────────────
    var showRegisterBtn = document.getElementById('show-register');
    if (showRegisterBtn) {
        showRegisterBtn.addEventListener('click', function(){ showRegister(); });
    }

    // ── Back to login from register screen ───────────────────
    var showLoginBtn = document.getElementById('show-login');
    if (showLoginBtn) {
        showLoginBtn.addEventListener('click', function(){ showLogin(); });
    }

    // ── Register form ─────────────────────────────────────────
    var registerForm = document.getElementById('register-form');
    if (registerForm) {
        registerForm.addEventListener('submit', async function(e) {
            e.preventDefault();
            var username        = document.getElementById('register-username').value.trim();
            var email           = document.getElementById('register-email').value.trim();
            var password        = document.getElementById('register-password').value;
            var confirmPassword = document.getElementById('register-confirm-password').value;
            var errEl   = document.getElementById('register-error');
            var succEl  = document.getElementById('register-success');
            var btn     = registerForm.querySelector('[type="submit"]');
            errEl.style.display='none'; succEl.style.display='none';
            btn.disabled=true; btn.textContent='Creating Account…';
            try {
                state.user = await apiRegister(username, email, password, confirmPassword);
                sessionStorage.setItem('wn_user', JSON.stringify(state.user));
                showDashboard();
            } catch(err) {
                errEl.textContent = err.message || 'Registration failed.';
                errEl.style.display='block';
            } finally { btn.disabled=false; btn.textContent='Create Account'; }
        });
    }

    // ── Change Password button (topbar) ──────────────────────
    var changePwBtn = document.getElementById('btn-change-password');
    if (changePwBtn) {
        changePwBtn.addEventListener('click', function() {
            // Clear previous state
            var errEl  = document.getElementById('change-pw-error');
            var succEl = document.getElementById('change-pw-success');
            var form   = document.getElementById('form-change-password');
            if (errEl)  { errEl.textContent=''; errEl.style.display='none'; }
            if (succEl) { succEl.textContent=''; succEl.style.display='none'; }
            if (form)   { form.reset(); form.style.display='block'; }
            var modal = document.getElementById('modal-change-password');
            if (modal) modal.classList.add('open');
        });
    }

    // ── Change Password form ──────────────────────────────────
    var changePwForm = document.getElementById('form-change-password');
    if (changePwForm) {
        changePwForm.addEventListener('submit', async function(e) {
            e.preventDefault();
            var newPw   = document.getElementById('cp-new').value;
            var confirm = document.getElementById('cp-confirm').value;
            var errEl   = document.getElementById('change-pw-error');
            var succEl  = document.getElementById('change-pw-success');
            var btn     = changePwForm.querySelector('[type="submit"]');
            errEl.style.display='none'; succEl.style.display='none';
            btn.disabled=true; btn.textContent='Updating…';
            try {
                await apiChangePassword(newPw, confirm);
                succEl.textContent = '✅ Password updated successfully!';
                succEl.style.display = 'block';
                changePwForm.style.display = 'none';
                setTimeout(function() {
                    var modal = document.getElementById('modal-change-password');
                    if (modal) modal.classList.remove('open');
                }, 2000);
            } catch(err) {
                errEl.textContent = err.message || 'Password change failed.';
                errEl.style.display = 'block';
            } finally { btn.disabled=false; btn.textContent='Update Password'; }
        });
    }

    // ── Logout ────────────────────────────────────────────────
    var logoutBtn = document.getElementById('btn-logout');
    if (logoutBtn) {
        logoutBtn.addEventListener('click', async function() {
            try { if (!DEMO_MODE) await http('POST','/users/logout'); } catch(_) {}
            sessionStorage.removeItem('wn_user');
            if (state.refreshInterval) { clearInterval(state.refreshInterval); state.refreshInterval=null; }
            state.user = null;
            showLogin();
        });
    }

    // ── Add Website form ──────────────────────────────────────
    var addForm = document.getElementById('form-add-website');
    if (addForm) {
        addForm.addEventListener('submit', async function(e) {
            e.preventDefault();
            var data = Object.fromEntries(new FormData(addForm));
            var btn  = addForm.querySelector('[type="submit"]');
            btn.disabled=true; btn.textContent='Adding…';
            try {
                await apiAddWebsite(data);
                state.websites = await apiGetWebsites();
                document.getElementById('modal-add-website').classList.remove('open');
                addForm.reset();
                navigateTo('websites');
                showToast('"' + data.name + '" added', 'success');
            } catch(e) { showToast('Error: ' + e.message, 'error'); }
            finally { btn.disabled=false; btn.textContent='Add Website'; }
        });
    }

    // ── Session restore ───────────────────────────────────────
    var saved = sessionStorage.getItem('wn_user');
    if (saved) { state.user = JSON.parse(saved); showDashboard(); }
    else showLogin();
});
