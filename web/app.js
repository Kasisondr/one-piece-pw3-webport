import {Input,BUTTON,DEFAULT_KEYS} from './input.js';
import {mountSaves} from './saves.js';
const $=id=>document.getElementById(id);
const canvas=$('display'),context=canvas.getContext('2d',{alpha:false});
const source=document.createElement('canvas'),sourceContext=source.getContext('2d',{alpha:false});
const input=new Input(canvas);let imageData,moduleInstance,running=false,launched=false,frames=0,lastFrame=0,intervals=[],recording=null,lastReport=null,lastSnapshot=[],lastReportedSize='';
const logLines=[];
let selectedModule='fbdemo';
let saveStorage;
function log(message){logLines.push(String(message));if(logLines.length>200)logLines.shift();$('log').textContent=logLines.join('\n');}
function runtimeState(text){$('runtime-state').textContent=text;}
function outputSize(){const wide=$('resolution').value==='1080';canvas.width=wide?1920:960;canvas.height=wide?1080:544;}
function syncSharedInput(){
  const ptr=globalThis.vitaBrowserHost?._inputPtr;
  if(ptr&&moduleInstance?.HEAP32){
    const s=lastSnapshot.length?lastSnapshot:input.snapshot();
    const base=ptr>>2;
    for(let i=0;i<8;i++)moduleInstance.HEAP32[base+i]=s[i];
  }
}
function present(heap,ptr,w,h,pitch){
  if(w<1||h<1||w>4096||h>4096||pitch<w||ptr<0||ptr+pitch*h*4>heap.length){log('Invalid framebuffer rejected');return;}
  const now=performance.now();
  if(lastFrame){const dt=now-lastFrame;intervals.push(dt);if(intervals.length>240)intervals.shift();if(recording)recording.intervals.push(dt);}
  lastFrame=now;frames++;
  if(source.width!==w||source.height!==h||!imageData){source.width=w;source.height=h;imageData=sourceContext.createImageData(w,h);}
  if(pitch===w)imageData.data.set(heap.subarray(ptr,ptr+w*h*4));
  else for(let row=0;row<h;row++)imageData.data.set(heap.subarray(ptr+row*pitch*4,ptr+(row*pitch+w)*4),row*w*4);
  if(canvas.width===w&&canvas.height===h){
    context.putImageData(imageData,0,0);
  }else{
    sourceContext.putImageData(imageData,0,0);
    const scale=Math.min(canvas.width/w,canvas.height/h),dw=w*scale,dh=h*scale;
    context.fillStyle='#000';context.fillRect(0,0,canvas.width,canvas.height);context.drawImage(source,(canvas.width-dw)/2,(canvas.height-dh)/2,dw,dh);
  }
  const sizeKey=`${w}x${h}_${canvas.width}x${canvas.height}`;
  if(lastReportedSize!==sizeKey){
    lastReportedSize=sizeKey;
    $('frame-size').textContent=`${w} × ${h} → ${canvas.width} × ${canvas.height}`;
  }
  syncSharedInput();
  if(!running){running=true;runtimeState(selectedModule==='fbdemo'?'RUNNING · VITA SAMPLE':'RUNNING · RETAIL DEVELOPMENT BUILD');$('screen-empty').hidden=true;$('stop').hidden=false;}
}
globalThis.vitaBrowserHost={present,input:()=>input.snapshot(),get frames(){return frames;}};
const isolated=globalThis.crossOriginIsolated&&typeof SharedArrayBuffer!=='undefined';
$('isolation').textContent=isolated?'● SHARED MEMORY READY':'SHARED MEMORY UNAVAILABLE';
async function launch(){
  if(launched){location.reload();return;}
  if(!isolated){log('Serve through tools/serve.py on localhost or HTTPS with COOP/COEP headers.');runtimeState('BLOCKED · CROSS-ORIGIN ISOLATION REQUIRED');return;}
  $('launch').disabled=true;runtimeState('LOADING WASM…');
  try{
    selectedModule=$('module-select').value;
    const {default:createModule}=await import(`./runtime/${selectedModule}.js`);
    moduleInstance=await createModule({noInitialRun:true,print:log,printErr:log,
      onAbort:reason=>{log(`Aborted: ${reason}`);runtimeState('RUNTIME ABORTED · SEE LOG');$('stop').hidden=false;$('log').parentElement.open=true;},
      onExit:code=>{running=false;runtimeState(`EXITED · CODE ${code}`);$('stop').hidden=true;$('launch').disabled=false;$('launch').textContent='Reload runtime';$('screen-empty').hidden=false;},
      locateFile:name=>new URL(`runtime/${name}`,location.href).href});
    if(selectedModule==='opw3'){
      runtimeState('OPENING LOCAL SAVES…');
      saveStorage=await mountSaves(moduleInstance,error=>{
        log(`Save storage failed: ${error.message||error}`);
        runtimeState('SAVE STORAGE FAILED · SEE LOG');
        $('log').parentElement.open=true;
      });
    }
    launched=true;$('module-select').disabled=true;$('screen-empty').hidden=true;canvas.focus();
    if(selectedModule==='fbdemo'){
      log('Launching fbdemo: SELF → armrecomp C → Emscripten WASM. Not game code.');
      moduleInstance.callMain(['/module.elf','--window','--title','Vita browser runtime sample']);
    }else{
      log('Local assets and browser saves are ready. Press K to confirm setup prompts, then Enter at the title screen.');
      $('runtime-label').textContent='OPW3 · DEVELOPMENT BUILD';
      moduleInstance.FS.mkdirTree('/app0');
      $('stop').hidden=false;
      moduleInstance.callMain(['/module.elf','--window','--mount','app0=/app0','--mount',`ux0=${saveStorage.ux0}`,'--mount',`savedata0=${saveStorage.data}`]);
    }
  }catch(error){log(error.stack||error);runtimeState('LOAD FAILED · SEE RUNTIME LOG');$('launch').disabled=false;$('screen-empty').hidden=false;}
}
$('launch').addEventListener('click',launch);
$('module-select').addEventListener('change',()=>{
  const retail=$('module-select').value==='opw3';
  $('launch').textContent=retail?'▶ Run game build':'▶ Run Vita sample';
  $('module-note').textContent=retail?'Press K to confirm setup prompts and Enter at the title screen. The browser skips startup videos and some Legend Log clips.':'Homebrew sample for checking the browser runtime.';
});
$('stop').addEventListener('click',async()=>{
  $('stop').disabled=true;
  try{await saveStorage?.flush();location.reload();}
  catch(error){
    log(`Could not save before stopping: ${error.message||error}`);
    $('log').parentElement.open=true;
    $('stop').disabled=false;
  }
});
$('resolution').addEventListener('change',outputSize);
$('fullscreen').addEventListener('click',async()=>{try{if(document.fullscreenElement)await document.exitFullscreen();else await $('screen').requestFullscreen();}catch(e){log(`Fullscreen: ${e.message}`);}});
$('screenshot').addEventListener('click',()=>{if(!frames){runtimeState('NO FRAME TO SAVE');return;}canvas.toBlob(blob=>download(blob,'vita-sample-frame.png'));});
$('touch-toggle').checked=matchMedia('(pointer:coarse)').matches;
$('touch-controls').hidden=!$('touch-toggle').checked;
$('touch-toggle').addEventListener('change',()=>{$('touch-controls').hidden=!$('touch-toggle').checked;input.clear();});
document.querySelectorAll('[data-pad]').forEach(el=>input.bindButton(el,el.dataset.pad));
input.bindStick($('left-stick'));input.bindStick($('right-stick'),true);
$('mouse-toggle').checked=input.pointerLook;
$('mouse-toggle').addEventListener('change',()=>{input.pointerLook=$('mouse-toggle').checked;if(!input.pointerLook&&document.pointerLockElement)document.exitPointerLock();input.clear();});
$('lock-mouse').addEventListener('click',async()=>{
  try{input.pointerLook=true;$('mouse-toggle').checked=true;canvas.focus();await canvas.requestPointerLock();}catch(e){log(`Pointer capture: ${e.message}`);}
});
$('deadzone').addEventListener('input',()=>{input.zone=Number($('deadzone').value)/100;$('deadzone-value').textContent=`${$('deadzone').value}%`;});
$('controller').addEventListener('change',()=>{input.padIndex=$('controller').value;input.clear();});
let padSignature='';
function updatePads(){
  const pads=Array.from(navigator.getGamepads?.()||[]).filter(Boolean),signature=pads.map(p=>p.index+p.id+p.mapping).join('|');
  if(signature===padSignature)return;padSignature=signature;
  const previous=$('controller').value;$('controller').replaceChildren(new Option('Auto · first standard gamepad','auto'));
  for(const pad of pads){const opt=new Option(`${pad.index+1} · ${pad.id}${pad.mapping==='standard'?'':' (unsupported mapping)'}`,String(pad.index));opt.disabled=pad.mapping!=='standard';$('controller').add(opt);}
  $('controller').value=Array.from($('controller').options).some(o=>o.value===previous)?previous:'auto';input.padIndex=$('controller').value;
  $('pad-status').textContent=pads.length?`${pads.length} CONTROLLER${pads.length>1?'S':''} · SINGLE PLAYER HOST`:'NO CONTROLLER CONNECTED';
}
window.addEventListener('gamepadconnected',updatePads);window.addEventListener('gamepaddisconnected',updatePads);
const labels={
  SQUARE:'□ Attack (LMB / J)',
  CROSS:'× Jump / Confirm (Space / K)',
  TRIANGLE:'△ Heavy / Charge (RMB / I)',
  CIRCLE:'○ Musou Special (MMB / L / F)',
  R:'R Dash / Dodge (Shift / E)',
  L:'L Lock-on / Reset (Ctrl / Q)',
  START:'Start / Pause (Esc / Enter)',
  SELECT:'Select / Map (Tab / M)',
  UP:'D-pad ↑ / W',
  DOWN:'D-pad ↓ / S',
  LEFT:'D-pad ← / A',
  RIGHT:'D-pad → / D'
};
function bindings(){
  $('bindings').replaceChildren();
  for(const [name,label] of Object.entries(labels)){
    const row=document.createElement('div');row.className='binding';const text=document.createElement('span');text.textContent=label;
    const button=document.createElement('button');button.textContent=Object.entries(input.mapping).filter(([,v])=>v===name).map(([k])=>k.replace('Key','').replace('Arrow','')).join(' / ')||'Set key';
    button.addEventListener('click',()=>{input.remap=name;button.blur();$('rebind-status').textContent=`Press a key for ${label}. Escape cancels.`;});row.append(text,button);$('bindings').append(row);
  }
}
document.addEventListener('keydown',e=>{
  // Input's listener has already processed a remap; refresh only after it.
  if(e.code==='Escape')input.remap=null;
  if($('rebind-status').textContent.startsWith('Press a key')){queueMicrotask(()=>{if(!input.remap){bindings();$('rebind-status').textContent='Bindings saved in this browser.';}});}
});
$('reset-keys').addEventListener('click',()=>{input.mapping={...DEFAULT_KEYS};localStorage.removeItem('vita.keys');input.remap=null;bindings();});bindings();
for(const name of Object.keys(BUTTON)){const el=document.createElement('span');el.textContent=name;el.dataset.bit=BUTTON[name];$('button-monitor').append(el);}
function quantile(a,q){if(!a.length)return null;const s=[...a].sort((x,y)=>x-y);return s[Math.min(s.length-1,Math.floor((s.length-1)*q))];}
let lastUi=0;
function tick(now){
  lastSnapshot=input.snapshot();
  syncSharedInput();
  updatePads();
  if(now-lastUi>80){
    lastUi=now;
    for(const el of $('button-monitor').children)el.classList.toggle('active',!!(lastSnapshot[0]&Number(el.dataset.bit)));
    ['left','right'].forEach((name,i)=>{$(`axis-${name}`).querySelector('i').style.transform=`translate(${(lastSnapshot[1+i*2]-128)/127*28}px,${(lastSnapshot[2+i*2]-128)/127*28}px)`;});
    $('input-values').textContent=`L ${lastSnapshot[1]},${lastSnapshot[2]} · R ${lastSnapshot[3]},${lastSnapshot[4]}${lastSnapshot[5]?' · TOUCH':''}`;
    const live=running&&now-lastFrame<1500;
    $('fps').textContent=live&&intervals.length?(1000/(intervals.reduce((a,b)=>a+b,0)/intervals.length)).toFixed(1):'—';
    $('p95').textContent=live&&intervals.length?quantile(intervals,.95).toFixed(1):'—';
  }
  if(recording&&now-recording.started>=10000){
    const elapsed=now-recording.started,a=recording.intervals;
    lastReport={schema:1,module:selectedModule,gamePlayable:false,timestamp:new Date().toISOString(),durationMs:elapsed,frames:a.length,averagePresentedFps:a.length*1000/elapsed,p95IntervalMs:quantile(a,.95),p99IntervalMs:quantile(a,.99),intervalsMs:a,presentation:[canvas.width,canvas.height],crossOriginIsolated,notes:'Module presentation timing only; does not establish retail gameplay performance.'};
    recording=null;$('benchmark').disabled=false;$('export').disabled=false;$('benchmark-status').textContent=`Captured ${a.length} intervals. Export JSON to compare runs.`;
  }
  requestAnimationFrame(tick);
}
requestAnimationFrame(tick);
document.addEventListener('visibilitychange',()=>{lastFrame=0;if(recording){recording=null;$('benchmark').disabled=false;$('benchmark-status').textContent='Recording cancelled: tab lost visibility.';}});
$('benchmark').addEventListener('click',()=>{if(!running){$('benchmark-status').textContent='Run the Vita sample before recording.';return;}recording={started:performance.now(),intervals:[]};$('benchmark').disabled=true;$('benchmark-status').textContent='Recording 10 seconds of presented frames. Keep this tab visible.';});
function download(blob,name){const url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
$('export').addEventListener('click',()=>{if(lastReport)download(new Blob([JSON.stringify(lastReport,null,2)],{type:'application/json'}),'vita-sample-timing.json');});
try{
  const response=await fetch('./dump-report.json');if(!response.ok)throw Error(`HTTP ${response.status}`);const r=await response.json();
  const entries=[['Title ID',r.metadata.TITLE_ID||'Unknown'],['Version',r.metadata.APP_VER||'Unknown'],['Files',r.files],['Data size',`${(r.bytes/1024**3).toFixed(2)} GiB`],['Executable',r.probes['eboot.bin']]];
  for(const [key,value] of entries){const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=key;dd.textContent=value;$('dump-facts').append(dt,dd);}
  $('blocker').textContent=r.blocker;$('milestone').textContent=r.summary||'The browser runtime sample is built. Game translation and bring-up remain in progress.';
  if(r.browserCombat?.verified){
    $('milestone-badge').textContent='● BROWSER COMBAT VERIFIED';
    $('battle-readiness').textContent='Chromium verified';
  }
  $('retail-option').disabled=!r.wasmBuilt;
}catch(e){$('blocker').textContent=`Inspection report unavailable. Run python tools/inspect_dump.py. ${e.message}`;}
log('Browser host ready. See the port status for verified milestones.');
