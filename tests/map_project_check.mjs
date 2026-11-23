import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createMap,findRoute,moveOnMap,newMap,passable,readMap} from '../dist/map-project.js';

const assets=JSON.parse(readFileSync(new URL('../dist/assets/scene.json',import.meta.url),'utf8'));
const project=readMap(newMap('urban',31415));
const map=createMap(project,assets);
const regenerated=createMap(readMap(JSON.stringify(project)),assets);
assert.deepEqual(regenerated.placed,map.placed);
assert.deepEqual(regenerated.meshes,map.meshes);
assert.ok(map.meshes.some(mesh=>mesh.id==='editor-roads'));
const fields=createMap(newMap('walk',project.seed),assets);
const strategy=createMap(newMap('strategy',project.seed),assets);
assert.ok(!fields.meshes.some(mesh=>mesh.id==='editor-roads'));
assert.notEqual(fields.extent,map.extent);
assert.notDeepEqual(strategy.placed,map.placed);
assert.notDeepEqual(createMap(newMap('urban',31416),assets).placed,map.placed);
assert.throws(()=>readMap({...project,seed:-1}),/terreno/);
assert.throws(()=>readMap({...project,profile:'invalid'}),/perfil/);

const removed=map.placed[0].id;
const cut=createMap({...project,removed:[removed]},assets);
assert.deepEqual(cut.placed,map.placed.filter(prop=>prop.id!==removed));
const modified=createMap({...project,
    edits:[{x:2,z:0,radius:6,delta:1.5}],
    props:[{type:'rock',x:0,z:4,scale:1}]},assets);
assert.ok(modified.height(2,0)-map.height(2,0)>1.1);
assert.equal(modified.height(-10,0),map.height(-10,0));
assert.ok(!passable(modified,0,4));
assert.ok(!passable(map,map.extent,0));

const obstacle={extent:16,placed:[{x:0,z:0,radius:3}],height:()=>0};
assert.deepEqual(moveOnMap(obstacle,[0,-4],0,1),[0,-4]);
assert.deepEqual(moveOnMap(obstacle,[0,-5],1,0),[1,-5]);
assert.deepEqual(findRoute(obstacle,[0,-8],[0,0]),[]);
const route=findRoute(obstacle,[0,-8],[0,8]);
assert.ok(route.length>1);
assert.deepEqual(route.at(-1),[0,8]);
let point=[0,-8];
for (const dest of route) {
    for(let i=1;i<=10;i++)
        assert.ok(passable(obstacle,point[0]+(dest[0]-point[0])*i/10,point[1]+(dest[1]-point[1])*i/10));
    point=dest;
}
assert.deepEqual(findRoute(obstacle,[-8,0],[-4,0]),[[-4,0]]);
console.log('PASS: deterministic terrain and objects, map profiles, smooth terrain edits, collision, and strategy routes.');
