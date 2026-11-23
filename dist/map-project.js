import {biomes, copyPrototype, hash, terrainPieces, terrainProfile} from './biomes.js';

export const mapProfiles = {
    walk: {label:'Primera persona', span:112, relief:1, trees:20, buildings:5, units:0, speed:4.5},
    urban: {label:'Isométrica urbana', span:156, relief:.26, trees:14, buildings:14, units:0, speed:7},
    strategy: {label:'Estrategia', span:156, relief:.68, trees:26, buildings:5, units:5, speed:5.5},
};
const propTypes = ['tree', 'house', 'rock'];
const clamp = (x, limit) => Math.max(-limit, Math.min(limit, x));
const gap = 24;
const roadHalf = 2.4;

export function newMap(profile = 'urban', seed = 12345) {
    const {span, relief, trees, buildings} = mapProfiles[profile];
    return {
        format:'pleiade-map', version:1, name:'Mi mapa', profile, seed, biome:'forest',
        span, relief, trees, buildings, shadows:true, quality:'balanced',
        start:[3.8, 0], edits:[], props:[], removed:[]
    };
}
export function readMap(value) {
    const w = typeof value === 'string' ? JSON.parse(value) : value;
    const valid = (n,a,b,integer=false) => Number.isFinite(n) && n >= a && n <= b && (!integer || Number.isInteger(n));
    const check = (ok, label) => { if (!ok) throw new Error('Mapa inválido · ' + label); };
    check(w?.format === 'pleiade-map' && w.version === 1, 'formato');
    check(typeof w.name === 'string' && w.name.trim().length > 0 && w.name.length <= 72, 'nombre');
    check(Object.hasOwn(mapProfiles,w.profile) && Object.hasOwn(biomes,w.biome), 'perfil o bioma');
    check(valid(w.seed,0,4294967295,true) && valid(w.span,64,180,true) && valid(w.relief,0,2), 'terreno');
    check(valid(w.trees,0,45,true) && valid(w.buildings,0,24,true), 'población procedural');
    check(typeof w.shadows === 'boolean' && ['eco','balanced','detail'].includes(w.quality), 'renderizado');
    const bound = w.span / 2 - 1;
    const point = (x,z) => valid(x,-bound,bound) && valid(z,-bound,bound);
    check(Array.isArray(w.start) && w.start.length === 2 && point(...w.start), 'punto de inicio');
    check(Array.isArray(w.edits) && w.edits.length <= 160 && w.edits.every(e => e && point(e.x,e.z) &&
          valid(e.radius,3,18) && valid(e.delta,-2,2)), 'pinceladas');
    check(Array.isArray(w.props) && w.props.length <= 160 && w.props.every(e => e && point(e.x,e.z) &&
          propTypes.includes(e.type) && valid(e.scale,.5,2)), 'objetos colocados');
    check(Array.isArray(w.removed) && w.removed.length <= 80 &&
          w.removed.every(id => typeof id === 'string' && /^(tree|house):\d+$/.test(id)), 'objetos retirados');
    return structuredClone(w);
}

const model = (mesh,x,y,z,sx,sy,sz,tint,layer=0) => ({mesh,instance:[
    sx,0,0,0, 0,sy,0,0, 0,0,sz,0, x,y,z,1, ...tint,1,.85,0,layer,0
]});
const gridDistance = x => Math.abs(x - Math.round(x/gap)*gap);
const smooth = t => (t=Math.max(0,Math.min(1,t)),t*t*(3-2*t));

function streetMesh(extent, height) {
    const vertices=[],indices=[];
    const addStrip=(begin,end,lane,vertical)=>{
        if(end<=begin)return;
        const count=Math.ceil((end-begin)/2);
        const first=vertices.length/8;
        for(let i=0;i<=count;i++){
            const along=begin+(end-begin)*i/count;
            for(const side of [-roadHalf,roadHalf]){
                const x=vertical?lane+side:along,z=vertical?along:lane+side;
                vertices.push(x,height(x,z)+.055,z,0,1,0,x*.25,z*.25);
            }
            if(i<count){
                const a=first+i*2;
                indices.push(...(vertical?[a,a+2,a+1,a+1,a+2,a+3]:[a,a+1,a+2,a+1,a+3,a+2]));
            }
        }
    };
    const lanes=[];
    for(let i=-Math.floor((extent-roadHalf)/gap);i<=Math.floor((extent-roadHalf)/gap);i++)lanes.push(i*gap);
    for(const lane of lanes){
        addStrip(-extent,extent,lane,true);
        let start=-extent;
        for(const crossing of lanes){addStrip(start,crossing-roadHalf,lane,false);start=crossing+roadHalf;}
        addStrip(start,extent,lane,false);
    }
    return {id:'editor-roads',vertices:new Float32Array(vertices),indices:new Uint32Array(indices)};
}

export function createMap(w, assets) {
    const extent=w.span/2, urban=w.profile==='urban', seed=w.seed;
    const base=terrainProfile(w.biome,seed), origin=base(0,0);
    const relief=(x,z)=>{
        const lane=urban?smooth((Math.min(gridDistance(x),gridDistance(z))-roadHalf)/6):1;
        let y=(base(x,z)-origin)*w.relief*lane;
        for(const edit of w.edits)y+=edit.delta*(1-smooth(Math.hypot(x-edit.x,z-edit.z)/edit.radius));
        return y;
    };
    const ground=terrainPieces(relief,seed,Math.ceil(w.span/6),undefined,extent);
    ground.pieces.forEach((piece,i)=>{piece.id='editor-ground-'+i;});
    const height=ground.surfaceHeight, terrainStyle=biomes[w.biome];
    const meshes=[...ground.pieces], objects=ground.pieces.map(piece=>
        model(piece.id,0,0,0,1,1,1,terrainStyle.tint,40+terrainStyle.layer));
    if(urban){
        meshes.push(streetMesh(extent,height));
        objects.push(model('editor-roads',0,0,0,1,1,1,[.19,.23,.24]));
    }
    const placed=[], reserved=[], removed=new Set(w.removed), safe=extent-6;
    const random=(i,salt)=>hash(seed ^ Math.imul(i+1,salt));
    function add(prop) {
        const {type,x,z,scale}=prop;
        const y=height(x,z), turn=prop.angle??0;
        if(type==='tree'){
            const variants={forest:'birch',desert:'juniper',snow:'spruce',tropical:'palm',bamboo:'bamboo'};
            copyPrototype(objects,assets.plants[variants[w.biome]],x,y,z,scale*5,turn,1,true);
        }else if(type==='house'){
            copyPrototype(objects,assets.buildingForms.house.slice(1),x,y,z,scale,turn,1,false);
        }else{
            objects.push(model(assets.meshIds.rock,x,y+scale*.37,z,scale*1.4,scale,scale*1.2,[.89,.87,.81],31));
        }
        placed.push({...prop,radius:scale*(type==='house'?4.6:type==='rock'?.7:.5)});
    }
    const clear = (x,z,r) => Math.hypot(x-w.start[0],z-w.start[1]) >= r+2.8 &&
        !reserved.some(p=>Math.hypot(x-p.x,z-p.z)<p.radius+r+1);
    for(const type of ['house','tree']){
        const count=type==='house'?w.buildings:w.trees;
        for(let i=0;i<count;i++){
            const id=type+':'+i;
            for(let attempt=0;attempt<30;attempt++){
                const key=i+101*attempt+(type==='house'?1000:0);
                let x=(random(key,3119)*2-1)*safe,z=(random(key,5237)*2-1)*safe;
                if(urban && type==='house'){
                    x=Math.max(-safe,Math.min(safe,Math.floor(x/gap)*gap+gap*.5));
                    z=Math.max(-safe,Math.min(safe,Math.floor(z/gap)*gap+gap*.5));
                }
                const scale=(type==='house'? .82:.8)+random(key,7231)*.39;
                if((urban&&Math.min(gridDistance(x),gridDistance(z))<(type==='house'?7:3.3)) ||
                   !clear(x,z,type==='house'?5:1.2) || (type==='house' && Math.abs(height(x+2,z-2)-height(x-2,z+2))>1.4))continue;
                reserved.push({x,z,radius:scale*(type==='house'?4.6:.5)});
                if(!removed.has(id))add({type,id,x,z,scale,angle:type==='house'?0:random(key,6871)*Math.PI*2});
                break;
            }
        }
    }
    w.props.forEach((prop,i)=>add({...prop,id:i}));
    return {objects,meshes,height,extent,seed,placed};
}

export function passable(map,x,z,radius=.38) {
    return Math.abs(x)<=map.extent-radius&&Math.abs(z)<=map.extent-radius&&
        !map.placed.some(p=>Math.hypot(x-p.x,z-p.z)<p.radius+radius);
}
export function moveOnMap(map, position, dx, dz) {
    const [x,z]=position, ground=map.height(x,z);
    const valid=(a,b)=>passable(map,a,b)&&Math.abs(map.height(a,b)-ground)<=Math.hypot(a-x,b-z)*.92+.06;
    if(valid(x+dx,z+dz))return [x+dx,z+dz];
    if(dx&&valid(x+dx,z))return [x+dx,z];
    if(dz&&valid(x,z+dz))return [x,z+dz];
    return position;
}

function canTravel(map, start, end) {
    const [x,z]=start, dx=end[0]-x, dz=end[1]-z, length=Math.hypot(dx,dz);
    const steps=Math.max(1,Math.ceil(length/.45));
    let y=map.height(x,z);
    for(let i=1;i<=steps;i++){
        const a=x+dx*i/steps,b=z+dz*i/steps,h=map.height(a,b);
        if(!passable(map,a,b)||Math.abs(h-y)>length/steps*.85+.035)return false;
        y=h;
    }
    return true;
}

export function findRoute(map, start, end) {
    if(!passable(map,...end))return [];
    if(canTravel(map,start,end))return [end];
    const step=2,size=Math.ceil(map.extent/step)+1,stride=size*2+1,count=stride*stride;
    const world=index=>[(index%stride-size)*step,(Math.floor(index/stride)-size)*step];
    function anchor(point) {
        const x=Math.round(point[0]/step)+size,z=Math.round(point[1]/step)+size;
        const centre=z*stride+x;
        const nearby=[centre,centre-1,centre+1,centre-stride,centre+stride];
        return nearby.find(cell=>canTravel(map,point,world(cell)));
    }
    const from=anchor(start), goal=anchor(end);
    if(from===undefined||goal===undefined)return [];
    const previous=new Int32Array(count).fill(-1), queue=new Int32Array(count);
    let tail=1;queue[0]=from;previous[from]=from;
    for(let head=0;head<tail&&previous[goal]<0;head++){
        const node=queue[head],point=world(node);
        for(const next of [node-1,node+1,node-stride,node+stride]){
            if(next<0||next>=count||previous[next]>=0||!canTravel(map,point,world(next)))continue;
            previous[next]=node;
            queue[tail++]=next;
        }
    }
    if(previous[goal]<0)return [];
    const path=[end];
    for(let node=goal;node!==from;node=previous[node])path.push(world(node));
    path.push(world(from));
    return path.reverse();
}
export const limitPoint = (map,[x,z])=>[clamp(x,map.extent-1),clamp(z,map.extent-1)];
export const actorModel = model;
