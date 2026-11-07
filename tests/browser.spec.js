import {test,expect} from '@playwright/test';
import {open} from 'node:fs/promises';
test('actual Vita WASM runs, presents pixels and responds to input',async({page})=>{
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto('http://127.0.0.1:8080');
  await expect(page.locator('#isolation')).toContainText('READY');
  await page.locator('#launch').click();
  await expect(page.locator('#runtime-state')).toContainText('RUNNING',{timeout:60000});
  await page.waitForFunction(()=>globalThis.vitaBrowserHost.frames>10);
  const square=()=>page.evaluate(()=>{
    const c=document.querySelector('#display'),ctx=c.getContext('2d'),d=ctx.getImageData(0,240,960,1).data;
    for(let x=0;x<960;x++)if(d[x*4]===255&&d[x*4+1]===255&&d[x*4+2]===255)return x;
    return -1;
  });
  const before=await square();expect(before).toBeGreaterThan(0);
  await page.locator('#display').focus();await page.keyboard.down('ArrowRight');
  await expect.poll(square,{timeout:10000}).toBeGreaterThan(before+15);
  await page.keyboard.up('ArrowRight');
  await page.locator('#resolution').selectOption('1080');
  await expect(page.locator('#display')).toHaveAttribute('width','1920');
  await page.waitForFunction(()=>document.querySelector('#frame-size').textContent.includes('1920'));
  await page.screenshot({path:'test-results/runtime-desktop.png',fullPage:true});
  await page.locator('#display').focus();await page.keyboard.down('Enter');
  await expect(page.locator('#runtime-state')).toContainText('EXITED',{timeout:15000});
  await page.keyboard.up('Enter');
  expect(errors).toEqual([]);
});
test('mobile controls support simultaneous pointers and release on cancellation',async({page})=>{
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.setViewportSize({width:390,height:844});await page.goto('http://127.0.0.1:8080');
  const cdp=await page.context().newCDPSession(page);
  await cdp.send('Emulation.setTouchEmulationEnabled',{enabled:true,maxTouchPoints:5});
  await page.locator('#touch-toggle').check();
  await page.locator('#screen').scrollIntoViewIfNeeded();
  // Hide the launch overlay so the canvas is the active work surface.
  await page.evaluate(()=>document.querySelector('#screen-empty').hidden=true);
  const right=page.locator('[data-pad="RIGHT"]'),cross=page.locator('[data-pad="CROSS"]');
  const a=await right.boundingBox(),b=await cross.boundingBox();
  await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:a.x+a.width/2,y:a.y+a.height/2,id:11},{x:b.x+b.width/2,y:b.y+b.height/2,id:12}]});
  await expect.poll(()=>page.evaluate(()=>globalThis.vitaBrowserHost.input()[0])).toBe(32|16384);
  await cdp.send('Input.dispatchTouchEvent',{type:'touchCancel',touchPoints:[]});
  await expect.poll(()=>page.evaluate(()=>globalThis.vitaBrowserHost.input()[0])).toBe(0);
  await page.screenshot({path:'test-results/runtime-mobile.png',fullPage:true});
  expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBe(true);
  expect(errors).toEqual([]);
});
test('controller disconnect and focus loss reset all input',async({page})=>{
  await page.goto('http://127.0.0.1:8080');
  await page.evaluate(()=>{
    globalThis.testPad={connected:true,mapping:'standard',index:0,id:'Test standard pad',axes:[1,0,-1,0],buttons:Array.from({length:16},(_,i)=>({pressed:i===0}))};
    Object.defineProperty(navigator,'getGamepads',{value:()=>[globalThis.testPad],configurable:true});
  });
  await expect.poll(()=>page.evaluate(()=>globalThis.vitaBrowserHost.input().slice(0,5))).toEqual([16384,255,128,1,128]);
  await page.evaluate(()=>{globalThis.testPad=null;window.dispatchEvent(new Event('gamepaddisconnected'));});
  await expect.poll(()=>page.evaluate(()=>globalThis.vitaBrowserHost.input().slice(0,5))).toEqual([0,128,128,128,128]);
  await page.locator('#display').focus();await page.keyboard.down('ArrowLeft');
  await expect.poll(()=>page.evaluate(()=>globalThis.vitaBrowserHost.input()[0])).toBe(128);
  await page.evaluate(()=>window.dispatchEvent(new Event('blur')));
  await expect.poll(()=>page.evaluate(()=>globalThis.vitaBrowserHost.input().slice(0,5))).toEqual([0,128,128,128,128]);
});
test('recompiled sample frame matches the upstream native reference hash',async({page})=>{
  await page.goto('http://127.0.0.1:8080');
  const result=await page.evaluate(async()=>{
    let done;const expectedFrame=new Promise(resolve=>done=resolve);let frame=0;
    const previous=globalThis.vitaBrowserHost.present;
    globalThis.vitaBrowserHost.input=()=>[0,128,128,128,128,0,0,0];
    globalThis.vitaBrowserHost.present=(heap,ptr,w,h,pitch)=>{
      previous(heap,ptr,w,h,pitch);if(++frame!==120)return;
      const header=new TextEncoder().encode(`P6\n${w} ${h}\n255\n`),ppm=new Uint8Array(header.length+w*h*3);ppm.set(header);
      for(let y=0;y<h;y++)for(let x=0;x<w;x++){
        const i=ptr+(y*pitch+x)*4,o=header.length+(y*w+x)*3;
        ppm[o]=heap[i];ppm[o+1]=heap[i+1];ppm[o+2]=heap[i+2];
      }
      crypto.subtle.digest('SHA-256',ppm).then(hash=>done(Array.from(new Uint8Array(hash),b=>b.toString(16).padStart(2,'0')).join('')));
    };
    const {default:create}=await import('/runtime/fbdemo.js');
    const module=await create({noInitialRun:true,print:()=>{},printErr:()=>{},locateFile:name=>`/runtime/${name}`});
    module.callMain(['/module.elf','--window','--virtual-clock','--frames','240','--press','RIGHT@10-70','--press','DOWN@30-60','--press','START@200']);
    return await expectedFrame;
  });
  expect(result).toBe('115c519b351b744c71187361eadd4e772e14c4d00025f6a5d51d769a34b3ef1d');
});
test('retail module advances from the autosave notice to the main menu',async({page})=>{
  test.setTimeout(150000);
  const errors=[],startupMovies=[];
  page.on('pageerror',error=>errors.push(error.message));
  page.on('request',request=>{
    if(/\/FILE_NGP\/MOVIE\/(OMEGA_LOGO|PM021_NA)\.mp4$/.test(request.url()))startupMovies.push(request.url());
  });
  await page.goto('http://127.0.0.1:8080');
  await expect(page.locator('#retail-option')).toBeEnabled();
  await page.locator('#module-select').selectOption('opw3');
  await page.locator('#launch').click();
  await expect(page.locator('#runtime-state')).toContainText('RUNNING · RETAIL DEVELOPMENT BUILD',{timeout:60000});
  await page.waitForFunction(()=>{
    const ctx=document.querySelector('#display').getContext('2d');
    const [red,green,blue]=ctx.getImageData(250,190,1,1).data;
    return red>180&&green>160&&blue>120;
  },undefined,{timeout:60000});
  await expect(page.locator('#runtime-label')).toContainText('OPW3 · DEVELOPMENT BUILD');
  await page.locator('#display').screenshot({path:'test-results/retail-autosave.png'});
  await page.locator('#display').focus();
  let setup=true;
  await expect.poll(async()=>{
    const frame=await page.evaluate(()=>{
      const ctx=document.querySelector('#display').getContext('2d');
      const data=ctx.getImageData(0,0,960,544).data;
      const redAt=(x,y)=>data[(y*960+x)*4];
      let redMenuPixels=0;
      // Legend Log's selected banner and the map background to its right.
      for(let y=25;y<85;y++)for(let x=25;x<240;x++){
        const at=(y*960+x)*4;
        if(data[at]>150&&data[at+1]<75&&data[at+2]<75)redMenuPixels++;
      }
      return {menu:redMenuPixels>750&&redAt(750,200)>120,
        splash:redAt(60,60)>100&&redAt(915,490)>100};
    });
    if(frame.menu)return true;
    if(frame.splash)setup=false;
    await page.keyboard.press(setup?'k':'Enter',{delay:110});
    return false;
  },{timeout:110000,intervals:[1400]}).toBe(true);
  expect(startupMovies).toEqual([]);
  expect(errors).toEqual([]);
  await page.locator('#display').screenshot({path:'test-results/retail-menu.png'});
});

test('browser save files survive a reload and reject a second writer',async({page,context})=>{
  await page.goto('http://127.0.0.1:8080');
  const checkSave=async action=>{
    const {default:create}=await import('/runtime/io-probe.js');
    const module=await create({noInitialRun:true,locateFile:name=>'/runtime/'+name});
    const {mountSaves}=await import('/saves.js');
    const fs=module.FS;
    let storage;
    try{storage=await mountSaves(module,()=>{});}
    catch(error){if(action==='lock')return error.message;throw error;}
    const file=storage.data+'/app0000.dat';
    if(action==='write'){
      fs.writeFile(file,new Uint8Array([1,2,3,255]));
      await storage.flush();
    }else if(action==='read')return Array.from(fs.readFile(file));
    else return '';
  };
  await page.evaluate(checkSave,'write');
  const other=await context.newPage();
  await other.goto('http://127.0.0.1:8080');
  const lockError=await other.evaluate(checkSave,'lock');
  expect(lockError).toContain('Close the other Pirate Warriors 3 session');
  await other.close();
  await page.reload();
  const restored=await page.evaluate(checkSave,'read');
  expect(restored).toEqual([1,2,3,255]);
});

test('WASM streams game asset ranges with native byte parity, EOF and 64-bit seeks',async({page})=>{
  const reads=[];
  page.on('response',r=>{if(r.url().includes('/__assets/')&&r.request().method()==='GET')reads.push({status:r.status(),length:Number(r.headers()['content-length'])});});
  await page.goto('http://127.0.0.1:8080');
  const result=await page.evaluate(async()=>{
    const logs=[];let finish;const done=new Promise(r=>finish=r);
    const timer=setTimeout(()=>finish({reason:'timeout',logs}),20000);
    try {
      const {default:create}=await import('/runtime/io-probe.js');
      const m=await create({noInitialRun:true,print:s=>logs.push(s),printErr:s=>logs.push(s),
        onAbort:s=>finish({reason:'abort',logs,detail:String(s)}),onExit:code=>finish({reason:'exit',code,logs}),locateFile:n=>'/runtime/'+n});
      m.callMain([]);
    }catch(e){finish({reason:'error',detail:String(e),logs});}
    const result=await done;clearTimeout(timer);return result;
  });
  expect(result.reason).toBe('exit');expect(result.code).toBe(0);
  const file=await open('generated/decrypted/PCSE00638/LINKDATA_NGP/LINKDATA_NA.C','r');
  try{
    const {size}=await file.stat();const hashes=[];
    for(const [offset,length] of [[0,32],[262140,32],[size-17,17]]){
      const bytes=Buffer.alloc(length);await file.read(bytes,0,length,offset);
      let hash=2166136261;for(const b of bytes)hash=Math.imul(hash^b,16777619)>>>0;
      hashes.push(hash.toString(16).padStart(8,'0').toUpperCase());
    }
    expect(result.logs).toContain(`WEB_IO_OK ${size} ${hashes.join(' ')}`);
    expect(reads.length).toBeGreaterThan(2);
    expect(reads.every(r=>r.status===206&&r.length>0&&r.length<=262144)).toBe(true);
    expect(reads.reduce((n,r)=>n+r.length,0)).toBeLessThan(2*1024*1024);
  }finally{await file.close();}
});

test('status dashboard distinguishes the first browser battle from full browser playability',async({page})=>{
  await page.route('**/dump-report.json',route=>route.fulfill({json:{
    metadata:{TITLE_ID:'PCSE00638',APP_VER:'01.00'},
    probes:{'eboot.bin':'unrecognized'},files:130,bytes:2943607640,wasmBuilt:true,
    gamePlayable:false,browserCombat:{verified:true},
    summary:'Chromium runs the opening Shells Town battle at 960 × 544.',
    blocker:'Episode completion and later battles still need verification.'
  }}));
  await page.goto('http://127.0.0.1:8080');
  await expect(page.locator('.intro-card strong')).toContainText('Shells Town combat');
  await expect(page.locator('.status-pill')).toContainText('BROWSER COMBAT VERIFIED');
  await expect(page.locator('#milestone')).toContainText('opening Shells Town battle');
  await expect(page.locator('#blocker')).toContainText('later battles still need verification');
});
