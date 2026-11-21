// Phone control page served by the ESP32.
#pragma once

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Matrix Panel</title>
<style>
:root{--bg:#101012;--card:#1c1c20;--line:#2c2c33;--fg:#eee;--mut:#9a9aa3;--acc:#4da3ff}
*{box-sizing:border-box}
body{margin:0;font:15px system-ui,-apple-system,sans-serif;background:var(--bg);color:var(--fg)}
header{display:flex;align-items:center;gap:12px;padding:14px 16px 6px}
header h1{font-size:19px;margin:0;flex:1}
.top{padding:0 16px}
nav{display:flex;gap:6px;padding:10px 16px;overflow-x:auto;position:sticky;top:0;background:var(--bg);z-index:2}
button{background:#2a2a30;color:var(--fg);border:0;border-radius:10px;padding:10px 14px;font:inherit;cursor:pointer}
button.on{background:var(--acc);color:#001}
nav button{flex:0 0 auto}
section{display:none;padding:0 16px 32px}
section.show{display:block}
.card{background:var(--card);border-radius:14px;padding:14px;margin-bottom:12px}
.card h3{margin:0 0 6px;font-size:16px}
.fx{display:grid;grid-template-columns:repeat(auto-fill,minmax(100px,1fr));gap:8px}
.fx button{padding:14px 8px}
label{display:block;color:var(--mut);font-size:13px;margin:12px 0 6px}
label.chk{display:flex;align-items:center;gap:8px;color:var(--fg);font-size:15px}
input[type=range]{width:100%}
input[type=text],input[type=password],input[type=number],select{width:100%;padding:10px;border-radius:10px;border:1px solid var(--line);background:#0a0a0b;color:var(--fg);font:16px system-ui}
input[type=color]{width:52px;height:40px;border:0;background:none;padding:0}
input[type=checkbox]{width:20px;height:20px}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.row select{flex:1;width:auto}
#pix{display:grid;grid-template-columns:repeat(16,1fr);gap:1px;background:#333;touch-action:none;user-select:none;width:100%;max-width:440px;aspect-ratio:1;margin:0 auto}
#pix div{background:#000}
.sw{width:30px;height:30px;border-radius:50%;border:2px solid #444;padding:0}
small{color:var(--mut)}
#fxlist label:first-child{margin-top:0}
.plgrid{display:grid;grid-template-columns:1fr 1fr;gap:0 12px}
.plgrid label.chk{margin:8px 0}
.plgrid input:disabled+span{color:var(--mut)}
</style></head><body>
<header><h1>Matrix Panel</h1><button id="pwr">On</button></header>
<div class="top"><div class="card"><label style="margin-top:0">Brightness</label><input type="range" id="bri" min="1" max="255"></div></div>
<nav id="tabs"><button data-t="fx">Effects</button><button data-t="pl">Playlist</button><button data-t="clock">Clock</button><button data-t="text">Text</button><button data-t="art">Pixel Art</button><button data-t="set">Settings</button></nav>

<section id="fx">
 <div class="card"><div id="fxlist"></div></div>
 <div class="card">
  <label style="margin-top:0">Speed</label><input type="range" class="speed" min="1" max="100">
  <label>Color (Solid and Breathe)</label><input type="color" id="color">
 </div>
</section>

<section id="pl">
 <div class="card">
  <label class="chk" style="margin-top:0"><input type="checkbox" id="auto"> Play the playlist</label>
  <p><small id="plnow"></small></p>
  <label>Show each one for (seconds)</label><input type="number" id="cycle" min="5" max="3600">
  <label class="chk"><input type="checkbox" id="shuffle"> Shuffle</label>
 </div>
 <div class="card"><h3>In the playlist</h3>
  <div class="row"><button id="plAll">Select all</button><button id="plNone">Clear</button></div>
  <div class="plgrid" id="plItems"></div>
  <p><small>Choosing something on another tab pauses the playlist. Messages always finish scrolling before the next item.</small></p>
 </div>
</section>

<section id="clock"><div class="card">
 <h3>Clock</h3><small id="timeinfo"></small>
 <label>Color</label><div class="row"><input type="color" class="tcolor"><label class="chk"><input type="checkbox" class="rainbow"> Rainbow</label></div>
 <label class="chk"><input type="checkbox" id="h24"> 24-hour time</label>
 <label>Time zone</label><select id="tz"></select>
</div></section>

<section id="text"><div class="card">
 <label style="margin-top:0">Message</label>
 <div class="row"><input type="text" id="msg" maxlength="200" style="flex:1;width:auto"><button id="send">Show</button></div>
 <label>Color</label><div class="row"><input type="color" class="tcolor"><label class="chk"><input type="checkbox" class="rainbow"> Rainbow</label></div>
 <label>Scroll speed</label><input type="range" class="speed" min="1" max="100">
</div></section>

<section id="art"><div class="card">
 <div id="pix"></div>
 <div class="row" style="margin-top:12px"><input type="color" id="pen" value="#ff0040"><div class="row" id="pal"></div></div>
 <div class="row" style="margin-top:10px"><button data-tool="pen" class="on">Draw</button><button data-tool="erase">Erase</button><button data-tool="fill">Fill</button><button id="clr">Clear</button></div>
 <label>Start from</label>
 <div class="row"><select id="preset"><option value="">Choose a picture...</option></select><button id="upload">Photo...</button><input type="file" id="file" accept="image/*" hidden></div>
 <label>Saved pictures (kept when unplugged)</label>
 <div class="row"><select id="slot"></select><button id="load">Load</button><button id="save">Save</button></div>
 <p><small>Your drawing shows on the panel as you draw.</small></p>
</div></section>

<section id="set">
 <div class="card"><h3>Wi-Fi</h3><small id="net"></small>
  <label>Network</label><div class="row"><select id="ssids"><option value="">Tap Scan...</option></select><button id="scan">Scan</button></div>
  <input type="text" id="ssid" placeholder="or type the network name" style="margin-top:8px">
  <label>Password</label><input type="password" id="pass">
  <p><button id="wsave">Save and restart</button></p>
 </div>
 <div class="card"><h3>Panel layout</h3>
  <small>Tap "Test pattern". The <b style="color:#f44">red</b> line should run along the top, the <b style="color:#4f4">green</b> line down the left side, with a white dot in the top-left corner. If not, change these until it is. (Set Rotation to Upright first.)</small>
  <label>First LED (where the data wire enters, seen from the front)</label>
  <select id="corner"><option value="0">Top left</option><option value="1">Top right</option><option value="2">Bottom left</option><option value="3">Bottom right</option></select>
  <label>LEDs are wired in</label>
  <select id="vertical"><option value="0">Rows (left-right)</option><option value="1">Columns (up-down)</option></select>
  <label class="chk"><input type="checkbox" id="serp"> Zigzag wiring (most 16x16 panels)</label>
  <p><button id="test">Test pattern</button></p>
 </div>
 <div class="card"><h3>Rotation</h3>
  <small>Turn the picture to match how the panel is hung.</small>
  <label>Rotate picture</label>
  <select id="rot"><option value="0">Upright</option><option value="1">90&deg; clockwise</option><option value="2">180&deg; (upside down)</option><option value="3">90&deg; counter-clockwise</option></select>
  <p><button id="rotnext">&#8635; Rotate 90&deg;</button></p>
 </div>
</section>

<script>
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
// Must match the Effect enum in the firmware.
const FX=["Solid","Silk","Plasma","Fire","Matrix","Twinkle","Life","Aurora","Lava Lamp","Ocean","Sunset","Breathe","Starfield","Fireworks","Rain","Bounce","Spiral","Ripples","Snake","Halloween","Snow","Chase","Hearts","Garden","Rocket","Fish","Day & Night","Cat & Mouse","Snowman","Dream","Hatching","UFO","Rainbow Walk","Volcano","Fetch","Turbo Road","Brick Buster","One Punch","Web Swing","Iron Flight","Sky Hero","Bat Signal","Star Shield","Green Smash","Spark Spider","Tiny Titan","Storm Hammer","Arrow Ace","Bark & Blaster","Clouds","Fireflies","Koi Pond","Falling Leaves","Candle","Campfire","Rainy Window","Moonrise","Jellyfish","Ink in Water","Tide Pool","Kaleidoscope","Galaxy","Sand Garden","Hourglass","Breathing Guide"];
const GROUPS=[["Classic",[0,1,2,3,4,5,6]],["Calm",[7,8,9,10,11,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64]],["Lively",[12,13,14,15,16,17,18]],["Seasonal",[19,20,21,22]],["Stories",[23,24,25,26,27,28,30,31,32,33,34]],["Heroes",[37,38,39,40,41,42,43,44,45,46,47,48]],["Arcade",[35,36]],["Surprise",[29]]];
const PL=[[200,"Clock"],[201,"Message"]].concat(FX.map((n,i)=>[i,n]),[0,1,2,3,4,5,6,7].map(i=>[210+i,"Picture "+(i+1)]));
const TZ=[["US Eastern","EST5EDT,M3.2.0,M11.1.0"],["US Central","CST6CDT,M3.2.0,M11.1.0"],["US Mountain","MST7MDT,M3.2.0,M11.1.0"],["US Arizona","MST7"],["US Pacific","PST8PDT,M3.2.0,M11.1.0"],["US Alaska","AKST9AKDT,M3.2.0,M11.1.0"],["US Hawaii","HST10"],["Atlantic (Halifax)","AST4ADT,M3.2.0,M11.1.0"],["Newfoundland","NST3:30NDT,M3.2.0,M11.1.0"],["UK / Ireland","GMT0BST,M3.5.0/1,M10.5.0"],["Central Europe","CET-1CEST,M3.5.0,M10.5.0/3"],["Eastern Europe","EET-2EEST,M3.5.0/3,M10.5.0/4"],["India","IST-5:30"],["China / Singapore","CST-8"],["Japan / Korea","JST-9"],["Sydney / Melbourne","AEST-10AEDT,M10.1.0,M4.1.0/3"],["New Zealand","NZST-12NZDT,M9.5.0,M4.1.0/3"],["UTC","UTC0"]];
let S={};

function post(url,o){return fetch(url,{method:'POST',body:new URLSearchParams(o)})}
async function set(o){const r=await post('/api/set',o);S=await r.json();render()}
async function refresh(){try{S=await (await fetch('/api/state')).json();render()}catch(e){}}

// Sliders send while dragging, at most every 120 ms.
function throttled(fn){let t=0,last;return v=>{last=v;if(t)return;fn(last);t=setTimeout(()=>{t=0;fn(last)},120)}}
const sendBri=throttled(v=>set({bri:v})),sendSpeed=throttled(v=>set({speed:v}));

function render(){
  $('#pwr').textContent=S.on?'On':'Off';$('#pwr').classList.toggle('on',!!S.on);
  if(document.activeElement!==$('#bri'))$('#bri').value=S.bri;
  $$('.speed').forEach(e=>{if(document.activeElement!==e)e.value=S.speed});
  $$('#fxlist button').forEach(b=>b.classList.toggle('on',S.mode==0&&S.effect==+b.dataset.fx));
  $('#color').value=S.color;$('#auto').checked=!!S.auto;$('#shuffle').checked=!!S.shuffle;
  if(document.activeElement!==$('#cycle'))$('#cycle').value=S.cycle;
  $$('#plItems input').forEach(e=>{const v=+e.value;e.checked=S.pl.includes(v);
    if(v>=210){const empty=!(S.slots>>(v-210)&1);e.disabled=empty;e.nextElementSibling.textContent='Picture '+(v-209)+(empty?' (empty)':'')}});
  const now=S.mode==0?FX[S.effect]:S.mode==1?'Clock':S.mode==2?'Message':S.slot==255?'Your drawing':'Picture '+(S.slot+1);
  $('#plnow').textContent=(S.auto?'Playing. Now showing: ':'Paused. Showing: ')+now;
  $$('.tcolor').forEach(e=>e.value=S.tcolor);$$('.rainbow').forEach(e=>e.checked=!!S.rainbow);
  $('#h24').checked=!!S.h24;$('#tz').value=S.tz;
  if(document.activeElement!==$('#msg'))$('#msg').value=S.text;
  $('#timeinfo').textContent=S.time?'Panel time: '+S.time:'Time not set yet. Reload this page to send your phone\'s time.';
  $('#net').textContent=S.net;
  $('#corner').value=S.corner;$('#rot').value=S.rot||0;$('#vertical').value=S.vertical?1:0;$('#serp').checked=!!S.serp;
}

// Tabs: opening Clock or Text shows it on the panel.
function tab(t,quiet){
  $$('nav button').forEach(b=>b.classList.toggle('on',b.dataset.t==t));
  $$('section').forEach(s=>s.classList.toggle('show',s.id==t));
  // While the playlist plays, just browsing tabs doesn't interrupt it.
  if(quiet||S.auto)return;
  if(t=='fx'&&S.mode!=0)set({mode:0});
  if(t=='clock')set({mode:1});
  if(t=='text')set({mode:2});
  if(t=='art')sendPix();
}
$$('nav button').forEach(b=>b.onclick=()=>tab(b.dataset.t));

$('#pwr').onclick=()=>set({on:S.on?0:1});
$('#bri').oninput=e=>sendBri(e.target.value);
$$('.speed').forEach(e=>e.oninput=()=>sendSpeed(e.value));
GROUPS.forEach(([g,ids])=>{
  const h=document.createElement('label');h.textContent=g;$('#fxlist').append(h);
  const grid=document.createElement('div');grid.className='fx';
  ids.forEach(i=>{const b=document.createElement('button');b.textContent=FX[i];b.dataset.fx=i;b.onclick=()=>set({mode:0,effect:i});grid.append(b)});
  $('#fxlist').append(grid)});
$('#color').onchange=e=>set(S.mode==0&&S.effect==11?{color:e.target.value}:{mode:0,effect:0,color:e.target.value});
$('#auto').onchange=e=>set({auto:e.target.checked?1:0});
$('#cycle').onchange=e=>set({cycle:e.target.value});
$('#shuffle').onchange=e=>set({shuffle:e.target.checked?1:0});
PL.forEach(([v,n])=>{const l=document.createElement('label');l.className='chk';l.innerHTML='<input type="checkbox" value="'+v+'"><span>'+n+'</span>';$('#plItems').append(l)});
$('#plItems').onchange=()=>set({pl:$$('#plItems input:checked').map(e=>e.value).join(',')});
$('#plAll').onclick=()=>set({pl:$$('#plItems input:not(:disabled)').map(e=>e.value).join(',')});
$('#plNone').onclick=()=>set({pl:''});
$$('.tcolor').forEach(e=>e.onchange=()=>set({tcolor:e.value,rainbow:0}));
$$('.rainbow').forEach(e=>e.onchange=()=>set({rainbow:e.checked?1:0}));
$('#h24').onchange=e=>set({h24:e.target.checked?1:0});
TZ.forEach(([n,v])=>$('#tz').add(new Option(n,v)));
$('#tz').onchange=e=>set({tz:e.target.value});
$('#send').onclick=()=>set({mode:2,text:$('#msg').value});
$('#msg').onkeydown=e=>{if(e.key=='Enter')$('#send').click()};

// ---- Pixel art editor ----
const px=Array(256).fill('#000000'),cells=[];
for(let i=0;i<256;i++){const d=document.createElement('div');d.dataset.i=i;cells.push(d);$('#pix').append(d)}
function draw(){px.forEach((c,i)=>cells[i].style.background=c)}
let tool='pen',down=false;
$$('[data-tool]').forEach(b=>b.onclick=()=>{tool=b.dataset.tool;$$('[data-tool]').forEach(x=>x.classList.toggle('on',x==b))});
['#ff0040','#ff8800','#ffdd00','#22dd44','#00bbff','#3344ff','#aa44ff','#ffffff'].forEach(c=>{const b=document.createElement('button');b.className='sw';b.style.background=c;b.onclick=()=>{$('#pen').value=c;if(tool=='erase')$('[data-tool=pen]').click()};$('#pal').append(b)});
function floodFill(i,to){const from=px[i];if(from==to)return;const q=[i];while(q.length){const k=q.pop();if(px[k]!=from)continue;px[k]=to;const x=k%16,y=k>>4;if(x>0)q.push(k-1);if(x<15)q.push(k+1);if(y>0)q.push(k-16);if(y<15)q.push(k+16)}}
function paintAt(e){
  const el=document.elementFromPoint(e.clientX,e.clientY);
  if(!el||el.parentNode!==$('#pix'))return;
  const i=+el.dataset.i;
  if(tool=='fill'){floodFill(i,$('#pen').value);draw()}
  else{px[i]=tool=='erase'?'#000000':$('#pen').value;cells[i].style.background=px[i]}
  sendPix();
}
$('#pix').onpointerdown=e=>{down=true;$('#pix').setPointerCapture(e.pointerId);paintAt(e)};
$('#pix').onpointermove=e=>{if(down&&tool!='fill')paintAt(e)};
$('#pix').onpointerup=$('#pix').onpointercancel=()=>{down=false};
const hex=()=>px.map(c=>c.slice(1)).join('');
const sendPix=throttled(()=>post('/api/pixels',{d:hex()}));
function setAll(arr){arr.forEach((c,i)=>px[i]=c);draw();sendPix()}
$('#clr').onclick=()=>setAll(Array(256).fill('#000000'));

const PAL={R:'#ff1030',Y:'#ffcc00',K:'#000000',W:'#ffffff',G:'#30d050',O:'#ff6a00',S:'#f2d6a0',B:'#2060ff'};
const PRESETS={
 Heart:["","","...RRR....RRR...","..RRRRR..RRRRR..",".RRRRRRRRRRRRRR.",".RRRRRRRRRRRRRR.",".RRRRRRRRRRRRRR.","..RRRRRRRRRRRR..","...RRRRRRRRRR...","....RRRRRRRR....",".....RRRRRR.....","......RRRR......",".......RR......."],
 Smiley:[".....YYYYYY.....","...YYYYYYYYYY...","..YYYYYYYYYYYY..",".YYYYYYYYYYYYYY.",".YYYKKYYYYKKYYY.",".YYYKKYYYYKKYYY.","YYYYYYYYYYYYYYYY","YYYYYYYYYYYYYYYY","YYKYYYYYYYYYYKYY","YYYKYYYYYYYYKYYY",".YYYKKKKKKKKYYY.",".YYYYYYYYYYYYYY.","..YYYYYYYYYYYY..","...YYYYYYYYYY...",".....YYYYYY....."],
 Pumpkin:[".......GG.......","......GG........","...OOOOOOOOOO...","..OOOOOOOOOOOO..",".OOOOOOOOOOOOOO.",".OOKKKOOOOKKKOO.",".OOOKKOOOOKKOOO.","OOOOOOOKKOOOOOOO","OOOOOOOOOOOOOOOO","OOKOOOOOOOOOOKOO","OOKKKOKKKKOKKKOO",".OOKKKKKKKKKKOO.",".OOOOKKOOKKOOOO.","..OOOOOOOOOOOO..","...OOOOOOOOOO..."],
 Invader:["","","","","....G.....G.....",".....G...G......","....GGGGGGG.....","...GG.GGG.GG....","..GGGGGGGGGGG...","..G.GGGGGGG.G...","..G.G.....G.G...",".....GG.GG......"],
 Mushroom:["",".....RRRRRR.....","...RRRWWRRRRR...","..RRWWWWRRRRRR..","..RRWWWWRRWWRR..",".RRRRWWRRWWWWRR.",".RRRRRRRRWWWWRR.",".RWWRRRRRRWWRRR.",".RWWRRRRRRRRRRR.","..RRRRRRRRRRRR..",".....SSSSSS.....","....SSKSSKSS....","....SSKSSKSS....","....SSSSSSSS....",".....SSSSSS....."],
 Ghost:["",".....WWWWWW.....","....WWWWWWWW....","...WWWWWWWWWW...","...WWBBWWWBBWW..","..WWWBBWWWBBWWW.","..WWWWWWWWWWWWW.","..WWWWWWWWWWWWW.","..WWWWWKKKWWWWW.","..WWWWWKKKWWWWW.","..WWWWWWWWWWWWW.","..WWWWWWWWWWWWW.","..WW.WWW.WWW.WW.","..W...W...W...W."]
};
Object.keys(PRESETS).forEach(n=>$('#preset').add(new Option(n,n)));
$('#preset').onchange=e=>{const p=PRESETS[e.target.value];if(!p)return;const out=[];for(let y=0;y<16;y++){const r=(p[y]||'').padEnd(16,'.');for(let x=0;x<16;x++)out.push(PAL[r[x]]||'#000000')}setAll(out);e.target.value=''};

$('#upload').onclick=()=>$('#file').click();
$('#file').onchange=e=>{const f=e.target.files[0];if(!f)return;const img=new Image();img.onload=()=>{
  const c=document.createElement('canvas');c.width=c.height=16;const g=c.getContext('2d');
  g.fillStyle='#000';g.fillRect(0,0,16,16);g.imageSmoothingQuality='high';
  const s=Math.min(img.width,img.height);g.drawImage(img,(img.width-s)/2,(img.height-s)/2,s,s,0,0,16,16);
  const d=g.getImageData(0,0,16,16).data,out=[];
  for(let i=0;i<256;i++)out.push('#'+[d[i*4],d[i*4+1],d[i*4+2]].map(v=>v.toString(16).padStart(2,'0')).join(''));
  setAll(out);e.target.value=''};img.src=URL.createObjectURL(f)};

for(let i=0;i<8;i++)$('#slot').add(new Option('Picture '+(i+1),i));
$('#load').onclick=async()=>{const s=$('#slot').value,t=await (await fetch('/api/image?slot='+s)).text();
  if(!t){alert('That slot is empty. Draw something and tap Save.');return}
  for(let i=0;i<256;i++)px[i]='#'+t.substr(i*6,6);draw();set({mode:3,slot:s})};
$('#save').onclick=async()=>{const r=await post('/api/save',{slot:$('#slot').value,d:hex()});S=await r.json();render();alert('Saved.')};

// ---- Settings ----
$('#scan').onclick=async()=>{$('#scan').textContent='Scanning...';try{const l=await (await fetch('/api/scan')).json();
  $('#ssids').innerHTML='<option value="">Choose network...</option>';l.forEach(n=>$('#ssids').add(new Option(n,n)))}finally{$('#scan').textContent='Scan'}};
$('#ssids').onchange=e=>{if(e.target.value)$('#ssid').value=e.target.value};
$('#wsave').onclick=async()=>{const ssid=$('#ssid').value.trim();if(!ssid){alert('Choose or type a network name.');return}
  await post('/api/wifi',{ssid,pass:$('#pass').value});
  alert('Saved. The panel is restarting and will scroll its new address. Reconnect your phone to your normal Wi-Fi and open http://matrix.local')};
function testPattern(){const out=[];for(let i=0;i<256;i++){const x=i%16,y=i>>4;out.push(i==0?'#ffffff':y==0?'#ff0000':x==0?'#00ff00':'#000000')}setAll(out)}
$('#test').onclick=testPattern;
$('#corner').onchange=e=>set({corner:e.target.value}).then(testPattern);
$('#rot').onchange=e=>set({rot:e.target.value});
$('#rotnext').onclick=()=>set({rot:((+S.rot||0)+1)%4});
$('#vertical').onchange=e=>set({vertical:e.target.value}).then(testPattern);
$('#serp').onchange=e=>set({serp:e.target.checked?1:0}).then(testPattern);

draw();
// Open on the tab matching what the panel is showing, without changing it.
refresh().then(()=>{if(!S.time)set({epoch:Math.floor(Date.now()/1000)})}).then(()=>tab(S.auto?'pl':['fx','clock','text','art'][S.mode]||'fx',true));
setInterval(()=>{if(!document.hidden)refresh()},5000);
</script></body></html>)rawliteral";
