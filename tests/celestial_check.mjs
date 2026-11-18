import assert from 'node:assert/strict';import fs from 'node:fs';
import {generateWorld,validateWorld,celestialFrame,starAtlas} from '../dist/celestial.js';
const a=generateWorld(12345),b=generateWorld(12345),other=generateWorld(99);
assert.deepEqual(a,b);assert.notDeepEqual(a.stars,other.stars);assert.deepEqual(validateWorld(JSON.parse(JSON.stringify(a))),a);
for(const seed of [0,1,4294967295])assert.equal(validateWorld(generateWorld(seed)).stars.length,1800);
for(const patch of [{version:2},{elapsedDays:NaN},{stars:[]},{daySeconds:0}])assert.throws(()=>validateWorld({...a,...patch}));
let first;for(let i=0;i<=240;i++){a.elapsedDays=i/240;let f=celestialFrame(a);for(const v of [f.sun,f.moon,f.light])assert(Math.abs(Math.hypot(...v)-1)<1e-12);assert(f.lightColor.every(x=>Number.isFinite(x)&&x>=0));if(!i)first=f;if(i===240){f.sun.forEach((x,j)=>assert(Math.abs(x-first.sun[j])<1e-12));assert.notDeepEqual(f.moon,first.moon);}}
assert.deepEqual(starAtlas(b),starAtlas(generateWorld(12345)));
fs.mkdirSync('tests/results/sky-04',{recursive:true});const atlas=starAtlas(b);assert(atlas instanceof Float32Array);assert.equal(atlas.byteLength,512*256*16);assert(Array.from(atlas).every(Number.isFinite));
fs.mkdirSync('tests/results/sky-05',{recursive:true});fs.writeFileSync('tests/results/sky-05/stars.rgba',new Uint8Array(atlas.buffer));
const frames=[];for(const hour of [12,18,0,3]){b.elapsedDays=hour/24;frames.push({hour,...celestialFrame(b)});}fs.writeFileSync('tests/results/sky-04/frames.json',JSON.stringify(frames));
console.log('PASS deterministic catalogues, distinct seeds, export/import, validation, 241 orbital samples, daily stellar rotation and independent lunar orbit.');

const {cloudTransmission}=await import('../dist/celestial.js');let w=generateWorld(12345);assert.equal(cloudTransmission(w,[0,1,0],0),1);let previous=1;for(let c=0;c<=1;c+=.05){const t=cloudTransmission(w,[.2,.9,.1],c);assert(t>=0&&t<=previous+1e-12);previous=t;}console.log('PASS cloud transmission bounded, clear-sky identity and monotonic coverage.');
