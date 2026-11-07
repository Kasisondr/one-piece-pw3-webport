// Headless retail bring-up. app0 reads use the local bounded-range backend.
import {chromium} from '@playwright/test';
import {writeFile,mkdir} from 'node:fs/promises';
const browser=await chromium.launch({headless:true});
try{
  const page=await browser.newPage();const errors=[],assetReads=[];
  page.on('response',r=>{if(r.url().includes('/__assets/') && r.request().method()==='GET')assetReads.push({path:new URL(r.url()).pathname,status:r.status(),range:r.headers()['content-range']});});
  page.on('pageerror',e=>errors.push(e.message));
  await page.goto('http://127.0.0.1:8080');
  const result=await page.evaluate(async()=>{
    const logs=[];let finish;let presentedFrames=0,nonBlackFrame=false,lastFrame=null;
    const previous=globalThis.vitaBrowserHost.present;
    globalThis.vitaBrowserHost.present=(heap,ptr,w,h,pitch)=>{
      previous(heap,ptr,w,h,pitch);presentedFrames++;
      lastFrame={width:w,height:h};
      if(!nonBlackFrame)for(let y=0;y<h;y++){
        let found=false;
        for(let x=0;x<w;x++){const at=ptr+(y*pitch+x)*4;if(heap[at]||heap[at+1]||heap[at+2]){found=true;break;}}
        if(found){nonBlackFrame=true;break;}
      }
    };
    const result=new Promise(resolve=>finish=resolve);
    const timer=setTimeout(()=>finish({reason:'timeout',logs}),90000);
    try{
      const {default:create}=await import('/runtime/opw3.js');
      const module=await create({noInitialRun:true,print:s=>logs.push(s),printErr:s=>logs.push(s),
        onAbort:s=>finish({reason:'abort',detail:String(s),logs}),onExit:code=>finish({reason:'exit',code,logs}),
        locateFile:name=>`/runtime/${name}`});
      module.FS.mkdirTree('/app0');module.FS.mkdirTree('/saves');
      module.callMain(['/module.elf','--window','--frames','1200','--virtual-clock','--mount','app0=/app0','--mount','ux0=/saves']);
    }catch(e){finish({reason:'error',detail:String(e),logs});}
    const out=await result;clearTimeout(timer);return {...out,presentedFrames,nonBlackFrame,lastFrame};
  });
  result.errors=errors;result.gamePlayable=false;result.assetsMounted=assetReads.some(r=>r.status===206);result.assetReads=assetReads;
  await page.screenshot({path:'generated/opw3/browser-frame.png',fullPage:true});
  await mkdir('generated/opw3',{recursive:true});
  await writeFile('generated/opw3/browser-boot.json',JSON.stringify(result,null,2));
  console.log(JSON.stringify({...result,assetReads:assetReads.length},null,2));
}finally{await browser.close();}
