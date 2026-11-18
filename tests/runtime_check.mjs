// Unit-test the JS resource/state layer with a strict mock. This is NOT a GPU/browser test.
import fs from 'node:fs/promises';import assert from 'node:assert/strict';
let id=0,boundFbo=null,activeUnit=0,currentProgram=null;const attachments=new Map(),textures=new Map(),buffers=new Map();let drawCalls=0,submittedIndices=0;const uniformWrites=new Map();
const constants=new Map();const mk=()=>({id:++id});const methods={NO_ERROR:0,
 createFramebuffer:()=>{const f=mk();attachments.set(f,new Set());return f;},bindFramebuffer:(_,f)=>boundFbo=f,
 activeTexture:u=>activeUnit=u-gl.TEXTURE0,bindTexture:(_,t)=>textures.set(activeUnit,t),framebufferTexture2D:(_,__,___,t)=>attachments.get(boundFbo).add(t),
 createProgram:()=>({...mk(),samplers:new Map(),shaders:[]}),createShader:()=>mk(),shaderSource:(s,source)=>s.source=source,attachShader:(p,s)=>p.shaders.push(s),
 linkProgram:p=>{for(const s of p.shaders)for(const m of s.source.matchAll(/uniform sampler2D (\w+)/g))p.samplers.set(m[1],0);},getShaderParameter:()=>true,getProgramParameter:()=>true,
 getUniformLocation:(p,n)=>({p,n}),uniform1i:(l,v)=>{uniformWrites.set(l.n,v);if(l.p.samplers.has(l.n))l.p.samplers.set(l.n,v);},
 uniform1f:(l,v)=>{assert(Number.isFinite(v));uniformWrites.set(l.n,v);},uniform3fv:(l,v)=>{assert(v.length===3&&Array.from(v).every(Number.isFinite));uniformWrites.set(l.n,[...v]);},
 uniformMatrix4fv:(_,__,m)=>assert(m.length===16&&Array.from(m).every(Number.isFinite),'finite matrix'),
 useProgram:p=>currentProgram=p,drawElementsInstanced:(_,count,__,___,n)=>{assert(count>0&&n>0);submittedIndices+=count*n;draw();},drawArrays:()=>draw(),
 getParameter:p=>p===gl.VERSION?'MOCK (not a GPU)':4096,getError:()=>0,isContextLost:()=>false,getExtension:()=>({}),checkFramebufferStatus:()=>gl.FRAMEBUFFER_COMPLETE,
 createTexture:mk,createBuffer:mk,createVertexArray:mk,bindBuffer:(target,b)=>buffers.set(target,b),bufferData:(target,data)=>{assert(buffers.has(target));assert(data.byteLength>0);},
};function draw(){if(boundFbo)for(const unit of currentProgram.samplers.values())assert(!attachments.get(boundFbo).has(textures.get(unit)),'texture feedback loop');drawCalls++;}
const gl=new Proxy(methods,{get(t,p){if(p in t)return t[p];if(/^[A-Z_0-9]+$/.test(p)){if(!constants.has(p))constants.set(p,100+constants.size);return constants.get(p);}return ()=>{};}});
const elements=new Map();function el(id){if(!elements.has(id))elements.set(id,{id,hidden:false,checked:true,value:'balanced',width:0,height:0,textContent:'',dataset:{},listeners:{},style:{},classList:{toggle(){}},setAttribute(){},querySelector(){return el('child-'+id)},addEventListener(n,f){this.listeners[n]=f},getContext(){return gl},setPointerCapture(){}});return elements.get(id);}
const buttons=['shore','materials','building'].map(name=>{const b=el(name);b.dataset.scene=name;return b;});
globalThis.document={hidden:false,getElementById:el,querySelectorAll:()=>buttons,addEventListener(){}};
globalThis.window=globalThis;globalThis.innerWidth=1200;globalThis.innerHeight=800;globalThis.devicePixelRatio=2;globalThis.location={search:''};globalThis.addEventListener=()=>{};globalThis.requestAnimationFrame=()=>1;globalThis.cancelAnimationFrame=()=>{};
const androidFiles=[];
const androidMock=process.env.PLEIADE_ANDROID_MOCK==='1';
let openedWorld=false;
if(androidMock){
 globalThis.PleiadeAndroid={openWorld(){openedWorld=true;},saveFile(name,mime,encoded){androidFiles.push({name,mime,bytes:Buffer.from(encoded,'base64')});}};
 globalThis.FileReader=class{
  async readAsDataURL(blob){const bytes=Buffer.from(await blob.arrayBuffer());this.result=`data:${blob.type};base64,${bytes.toString('base64')}`;this.onload();}
 };
 el('view').toBlob=callback=>callback(new Blob([new Uint8Array([137,80,78,71])],{type:'image/png'}));
}
globalThis.fetch=async path=>{const b=await fs.readFile(new URL('../dist/'+path,import.meta.url));return {ok:true,json:async()=>JSON.parse(b),arrayBuffer:async()=>b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength)};};
await import('../dist/app.js');
for(let i=0;i<100&&!globalThis.pleiade.state.ready;i++)await new Promise(r=>setTimeout(r,10));
assert(pleiade.state.ready,JSON.stringify(pleiade.state.errors));
if(androidMock){
 assert.equal(pleiade.state.quality,'eco');
 el('import-world').onclick({preventDefault(){}});assert(openedWorld);
 const initial=pleiade.world;
 pleiade.importWorldText(JSON.stringify({...initial,elapsedDays:.25}));assert.equal(pleiade.world.elapsedDays,.25);
 pleiade.importWorldText('invalid JSON');assert.equal(pleiade.world.elapsedDays,.25);
 el('report').onclick();el('export-world').onclick();el('capture').onclick();pleiade.render();
 await new Promise(resolve=>setImmediate(resolve));
 const report=androidFiles.find(file=>file.name.endsWith('-informe.json'));
 const worldExport=androidFiles.find(file=>file.name.startsWith('Pleiade-mundo-'));
 const snapshot=androidFiles.find(file=>file.name.endsWith('.png'));
 const {viewer_version}=JSON.parse(await fs.readFile(new URL('../dist/assets/manifest.json',import.meta.url),'utf8'));
 assert.match(viewer_version,/^\d\.\d{3}\.\d{2}\.\d{3}$/);
 assert.equal(JSON.parse(report.bytes).viewer,viewer_version);
 assert.equal(JSON.parse(worldExport.bytes).elapsedDays,.25);
 assert.equal(snapshot.mime,'image/png');assert.equal(snapshot.bytes[0],137);
 pleiade.applyWorld(initial);
 console.log('PASS: Android file bridge, valid and invalid world import, PNG, JSON and eco startup. Mock only.');
}
for(const quality of ['eco','balanced','detail']){pleiade.setQuality(quality);for(const scene of ['shore','materials','building']){pleiade.useScene(scene);pleiade.render();}}
pleiade.setQuality('detail');assert.equal(pleiade.report().render.shadow,2048);assert.equal(pleiade.report().render.shadowDepthMiB,16);
pleiade.state.shadowSoft=false;pleiade.render();assert.equal(uniformWrites.get('uShadowSoft'),false);pleiade.state.shadowSoft=true;pleiade.render();assert.equal(uniformWrites.get('uShadowSoft'),true);
const canvas=el('view');const oldYaw=pleiade.camera.yaw;
canvas.onpointerdown({pointerId:1,clientX:100,clientY:100});canvas.onpointermove({pointerId:1,clientX:140,clientY:100});assert.notEqual(pleiade.camera.yaw,oldYaw);
canvas.onpointerdown({pointerId:2,clientX:200,clientY:100});const oldDistance=pleiade.camera.distance;canvas.onpointermove({pointerId:2,clientX:250,clientY:100});assert(pleiade.camera.distance<oldDistance);
canvas.onpointercancel({pointerId:1});canvas.onpointercancel({pointerId:2});
globalThis.innerWidth=800;globalThis.innerHeight=1280;pleiade.render();assert(canvas.height>canvas.width);assert.equal(pleiade.state.errors.length,0);assert(drawCalls>20);
console.log('PASS: JS initialization, 9 scene/quality combinations, framebuffer feedback checks, finite matrices, pointer orbit/pinch/cancel and portrait resizing. Mock only; no WebGL GPU validation.');

pleiade.useScene('shore');
pleiade.setQuality('balanced');submittedIndices=0;pleiade.render();
const balancedIndices=submittedIndices;
const inspectionCamera=pleiade.camera;
pleiade.setQuality('eco');submittedIndices=0;pleiade.render();
assert(submittedIndices<balancedIndices*.65,'eco must submit substantially fewer foliage indices');
assert.equal(pleiade.camera,inspectionCamera,'quality changes preserve the viewing position');
Object.assign(globalThis,{innerWidth:412,innerHeight:915,devicePixelRatio:2.625});
pleiade.render();assert.equal(canvas.height,640);assert(canvas.width<canvas.height);
Object.assign(globalThis,{innerWidth:1536,innerHeight:960,devicePixelRatio:1});
pleiade.render();assert.equal(canvas.width,640);assert(canvas.height<canvas.width);
console.log('PASS: light foliage and stable camera with phone portrait and tablet landscape sizing.');

const original=JSON.parse(JSON.stringify(pleiade.world));
pleiade.applyWorld({...original,elapsedDays:0});pleiade.render();assert.equal(uniformWrites.get('uDaylight'),0);const night=uniformWrites.get('uLightColor');
pleiade.applyWorld({...original,elapsedDays:.5});pleiade.render();assert.equal(uniformWrites.get('uDaylight'),1);assert(uniformWrites.get('uLightColor')[0]>night[0]);
assert.throws(()=>pleiade.applyWorld({...original,version:999}));assert.equal(pleiade.world.version,1);
el('look-sky').onclick();pleiade.render();el('look-sky').onclick();pleiade.render();
console.log('PASS: world restore, rejected import preserves active world, day/night lighting uniforms and sky camera.');

for(const altitude of [2,900,1300,1700,3200,3450,3650,6000]){pleiade.setAtmosphere({clouds:.85,altitude,fov:8});pleiade.render();assert.equal(uniformWrites.get('uCloudAmount'),.85);assert.equal(uniformWrites.get('uSkyObserver')[1],altitude);}
pleiade.setAtmosphere();console.log('PASS: atmospheric uniforms at 8 altitudes and optical zoom; mock only.');
pleiade.setQuality('balanced');pleiade.setAtmosphere({clouds:.8,altitude:2});pleiade.render();const built=pleiade.report().cloudCache.updates;pleiade.render();assert.equal(pleiade.report().cloudCache.updates,built,'reuse stationary cloud map');
pleiade.camera.yaw+=.3;pleiade.render();assert.equal(pleiade.report().cloudCache.updates,built,'rotation reuses panorama');
pleiade.setAtmosphere({clouds:.8,altitude:1200});pleiade.render();assert.equal(pleiade.report().cloudCache.updates,built+1,'altitude invalidates');
pleiade.setAtmosphere({clouds:.8,altitude:1200,reuse:false});pleiade.render();assert.equal(pleiade.report().cloudCache.updates,built+1,'reference mode does not build cache');assert.equal(uniformWrites.get('uCloudCached'),false);
pleiade.setAtmosphere({clouds:0});pleiade.render();assert.equal(pleiade.report().cloudCache.updates,built+1,'clear sky bypasses cache');
pleiade.setAtmosphere({clouds:.8});pleiade.render();let count=pleiade.report().cloudCache.updates;pleiade.world.elapsedDays+=.3/pleiade.world.daySeconds;pleiade.render();assert.equal(pleiade.report().cloudCache.updates,count+1,'world clock invalidates');assert.equal(pleiade.report().cloudCache.width,256);
console.log('PASS cloud-cache reuse, camera rotation, altitude/time invalidation, reference and disabled modes.');
