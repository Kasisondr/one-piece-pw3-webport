export const BUTTON = Object.freeze({SELECT:1,L3:2,R3:4,START:8,UP:16,RIGHT:32,DOWN:64,LEFT:128,L:256,R:512,TRIANGLE:4096,CIRCLE:8192,CROSS:16384,SQUARE:32768});
export const DEFAULT_KEYS = Object.freeze({
  ArrowUp:'UP', ArrowRight:'RIGHT', ArrowDown:'DOWN', ArrowLeft:'LEFT',
  KeyW:'UP', KeyA:'LEFT', KeyS:'DOWN', KeyD:'RIGHT',
  Space:'CROSS', KeyK:'CROSS',
  KeyJ:'SQUARE', KeyI:'TRIANGLE', KeyL:'CIRCLE', KeyF:'CIRCLE',
  KeyE:'R', ShiftLeft:'R',
  KeyQ:'L', ControlLeft:'L',
  Tab:'SELECT', KeyM:'SELECT', ShiftRight:'SELECT',
  Enter:'START', Escape:'START'
});
export function deadzone(value, zone=.16) {
  value = Number.isFinite(value) ? Math.max(-1,Math.min(1,value)) : 0;
  return Math.abs(value)<=zone ? 0 : Math.sign(value)*(Math.abs(value)-zone)/(1-zone);
}
export function gamepadState(pad, zone=.16) {
  const state={buttons:0,axes:[0,0,0,0]};
  if(!pad?.connected || pad.mapping!=='standard') return state;
  const map=['CROSS','CIRCLE','SQUARE','TRIANGLE','L','R','L','R','SELECT','START','L3','R3','UP','DOWN','LEFT','RIGHT'];
  map.forEach((name,i)=>{if(pad.buttons[i]?.pressed) state.buttons|=BUTTON[name];});
  state.axes=state.axes.map((_,i)=>deadzone(pad.axes[i]??0,zone));
  return state;
}
export class Input {
  constructor(canvas,root=document) {
    this.canvas=canvas;this.root=root;this.keys=new Set();this.pointers=new Map();this.touch=null;
    this.mouse=[0,0];this.focused=true;this.remap=null;this.zone=.16;this.padIndex='auto';this.pointerLook=matchMedia('(pointer:fine)').matches;
    try {this.mapping={...DEFAULT_KEYS,...JSON.parse(localStorage.getItem('vita.keys')||'{}')};}catch{this.mapping={...DEFAULT_KEYS};}
    root.addEventListener('keydown',e=>{
      if(/INPUT|SELECT|TEXTAREA/.test(e.target?.tagName)||e.target?.isContentEditable)return;
      if(this.remap){e.preventDefault();if(e.code!=='Escape'){this.mapping[e.code]=this.remap;localStorage.setItem('vita.keys',JSON.stringify(this.mapping));}this.remap=null;return;}
      if(this.mapping[e.code] || ['KeyW','KeyA','KeyS','KeyD'].includes(e.code)){
        if(e.target?.closest?.('button')) return;
        e.preventDefault();this.keys.add(e.code);
      }
    });
    root.addEventListener('keyup',e=>this.keys.delete(e.code));
    window.addEventListener('blur',()=>{this.focused=false;this.clear();});
    window.addEventListener('focus',()=>this.focused=true);
    root.addEventListener('visibilitychange',()=>{if(document.hidden)this.clear();});
    canvas.addEventListener('contextmenu',e=>e.preventDefault());
    canvas.addEventListener('pointerdown',e=>{
      canvas.focus();
      if(e.pointerType==='mouse'){
        if(this.pointerLook && root.pointerLockElement!==canvas && e.button===0){
          try{canvas.requestPointerLock();}catch{}
        }
        let btn = 0;
        if(e.button===0) btn = BUTTON.SQUARE;
        else if(e.button===2) btn = BUTTON.TRIANGLE;
        else if(e.button===1) btn = BUTTON.CIRCLE;
        if(btn){this.pointers.set(e.pointerId,{buttons:btn});return;}
      }
      canvas.setPointerCapture(e.pointerId);
      this.touchAt(e);
    });
    canvas.addEventListener('pointermove',e=>{
      if(root.pointerLockElement===canvas){
        this.mouse[0]=Math.max(-1,Math.min(1,this.mouse[0]+e.movementX*.025));
        this.mouse[1]=Math.max(-1,Math.min(1,this.mouse[1]+e.movementY*.025));
      }
      else if(canvas.hasPointerCapture(e.pointerId)&&!this.pointerLook)this.touchAt(e);
    });
    const up=e=>{this.pointers.delete(e.pointerId);if(this.touch?.id===e.pointerId)this.touch=null;};
    ['pointerup','pointercancel','lostpointercapture'].forEach(type=>canvas.addEventListener(type,up));
  }
  clear(){this.keys.clear();this.pointers.clear();this.mouse=[0,0];this.touch=null;this.root.querySelectorAll('[data-pad].held').forEach(el=>el.classList.remove('held'));this.root.querySelectorAll('.stick').forEach(el=>{el.style.setProperty('--x','0px');el.style.setProperty('--y','0px');});}
  touchAt(e){
    const r=this.canvas.getBoundingClientRect(),ratio=960/544;
    let w=r.width,h=w/ratio;if(h>r.height){h=r.height;w=h*ratio;}
    const x=(e.clientX-r.left-(r.width-w)/2)/w,y=(e.clientY-r.top-(r.height-h)/2)/h;
    if(x>=0&&x<=1&&y>=0&&y<=1)this.touch={id:e.pointerId,x:Math.min(1919,Math.floor(x*1920)),y:Math.min(1087,Math.floor(y*1088))};
    else this.touch=null;
  }
  bindButton(el,name){
    el.addEventListener('pointerdown',e=>{e.preventDefault();el.setPointerCapture(e.pointerId);this.pointers.set(e.pointerId,{buttons:BUTTON[name]});el.classList.add('held');});
    const release=e=>{this.pointers.delete(e.pointerId);el.classList.remove('held');};
    ['pointerup','pointercancel','lostpointercapture'].forEach(t=>el.addEventListener(t,release));
  }
  bindStick(el,right=false){
    const move=e=>{if(!el.hasPointerCapture(e.pointerId))return;const r=el.getBoundingClientRect();let x=(e.clientX-r.left-r.width/2)/(r.width*.35),y=(e.clientY-r.top-r.height/2)/(r.height*.35);const len=Math.hypot(x,y);if(len>1){x/=len;y/=len;}this.pointers.set(e.pointerId,{axes:right?[0,0,x,y]:[x,y,0,0]});el.style.setProperty('--x',`${x*25}px`);el.style.setProperty('--y',`${y*25}px`);};
    el.addEventListener('pointerdown',e=>{e.preventDefault();el.setPointerCapture(e.pointerId);move(e);});
    el.addEventListener('pointermove',move);
    const release=e=>{this.pointers.delete(e.pointerId);el.style.setProperty('--x','0px');el.style.setProperty('--y','0px');};
    ['pointerup','pointercancel','lostpointercapture'].forEach(t=>el.addEventListener(t,release));
  }
  snapshot(){
    if(!this.focused||document.hidden)return [0,128,128,128,128,0,0,0];
    let buttons=0,axes=[0,0,0,0];
    for(const key of this.keys)buttons|=BUTTON[this.mapping[key]]||0;
    axes[0]=Number(this.keys.has('KeyD'))-Number(this.keys.has('KeyA'));
    axes[1]=Number(this.keys.has('KeyS'))-Number(this.keys.has('KeyW'));
    const pads=Array.from(navigator.getGamepads?.()||[]).filter(Boolean);
    const pad=this.padIndex==='auto'?pads.find(p=>p.connected&&p.mapping==='standard'):pads.find(p=>p.index===Number(this.padIndex));
    const g=gamepadState(pad,this.zone);buttons|=g.buttons;
    const merge=a=>a.forEach((v,i)=>{if(Math.abs(v)>Math.abs(axes[i]))axes[i]=v;});
    merge(g.axes);merge([0,0,...this.mouse]);
    for(const s of this.pointers.values()){buttons|=s.buttons||0;if(s.axes)merge(s.axes);}
    this.mouse=this.mouse.map(x=>Math.abs(x)<.01?0:x*.8);
    // Circular stick range: diagonal WASD cannot exceed a stick's radius.
    for(let i=0;i<4;i+=2){const n=Math.hypot(axes[i],axes[i+1]);if(n>1){axes[i]/=n;axes[i+1]/=n;}}
    return [buttons,...axes.map(x=>Math.max(0,Math.min(255,Math.round(128+x*127)))),this.touch?1:0,this.touch?.x||0,this.touch?.y||0];
  }
}
