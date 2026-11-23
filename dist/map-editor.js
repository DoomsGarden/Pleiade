import {actorModel,createMap,findRoute,limitPoint,mapProfiles,moveOnMap,newMap,passable,readMap} from './map-project.js';

const storageKey='pleiade-editor-map-v1';
const byId=id=>document.getElementById(id);
const plus=(a,b)=>a.map((x,i)=>x+b[i]);
const scale=(a,n)=>a.map(x=>x*n);

export function installMapEditor({canvas,assets,mount,leave,cameraFrame,updated,download}) {
    const panel=byId('map-editor'), bar=byId('map-controls'), status=byId('map-status');
    const inputs=Object.fromEntries(['name','profile','seed','biome','span','relief','trees','buildings','quality','shadows']
        .map(key=>[key,byId('map-'+key)]));
    let project=newMap(), map, active=false, playing=false;
    let actor, units=[], selected=-1, camera, pointer, undo=[];
    const keys=new Set();
    try { const stored=localStorage.getItem(storageKey);if(stored)project=readMap(stored); } catch {}
    const instructions={
        walk:'WASD para caminar. Arrastra para mirar.',
        urban:'WASD para recorrer la ciudad. La cámara sigue al jugador.',
        strategy:'Selecciona una unidad y pulsa un destino. Arrastra o usa WASD para desplazar la vista.',
    };
    const keyNames={KeyW:'up',ArrowUp:'up',KeyS:'down',ArrowDown:'down',KeyA:'left',ArrowLeft:'left',KeyD:'right',ArrowRight:'right'};
    function say(message) { status.textContent=message; }
    function sync() {
        for(const [key,input] of Object.entries(inputs)){
            if(key==='shadows')input.checked=project[key];else input.value=project[key];
        }
        if(map)say(map.placed.length+' objetos · '+project.span+' × '+project.span+' m');
        byId('map-undo').disabled=!undo.length;
    }
    function save() {
        try {localStorage.setItem(storageKey,JSON.stringify(project));}
        catch {say('El navegador no pudo guardar. Exporta el mapa para conservarlo.');}
    }
    function locate(start) {
        if(passable(map,...start))return [...start];
        for(let radius=1;radius<=24;radius++)for(let i=0;i<12;i++){
            const a=i*Math.PI/6;
            const point=limitPoint(map,[start[0]+radius*Math.cos(a),start[1]+radius*Math.sin(a)]);
            if(passable(map,...point))return point;
        }
        return [...start];
    }
    function initializeActors() {
        actor=locate(project.start);
        selected=-1;
        units=Array.from({length:mapProfiles[project.profile].units},(_,i)=>
            ({point:locate([actor[0]+(i%3-1)*2.2,actor[1]+Math.floor(i/3)*2.2]), route:[]}));
    }
    function refreshActors() {
        map.objects.length=map.staticCount;
        function marker(point,tint) {
            const [x,z]=point;
            map.objects.push(actorModel(assets.meshIds.box,x,map.height(x,z)+.72,z,.65,1.44,.65,tint));
        }
        if(playing && project.profile==='strategy')
            units.forEach((unit,i)=>marker(unit.point,i===selected?[.92,.73,.26]:[.17,.56,.76]));
        else if(playing && project.profile==='urban')marker(actor,[.79,.34,.12]);
        else if(!playing){
            const [x,z]=project.start;
            map.objects.push(actorModel(assets.meshIds.box,x,map.height(x,z)+.53,z,.25,1.06,.25,[.9,.75,.20]));
        }
        updated();
    }
    function resetView() {
        const span=project.span;
        camera={target:[...project.start],yaw:Math.PI/4,pitch:playing&&project.profile==='walk'?-.08:project.profile==='urban'?Math.atan(Math.SQRT1_2):.7,
            distance:Math.max(38,span*.67),half:span*.40};
        if(playing&&project.profile==='urban')camera.half=span*.21;
    }
    function rebuild() {
        map=createMap(project,assets);
        map.staticCount=map.objects.length;
        initializeActors();
        mount(map,project);
        refreshActors();
        sync();
    }
    function replace(next,reset=false) {
        const validated=readMap(next);
        undo.push(project);
        if(undo.length>24)undo.shift();
        project=validated;
        if(reset)resetView();
        rebuild();
        save();
    }
    function open() {
        active=true;playing=false;resetView();
        document.body.classList.add('map-workspace');
        byId('map-toggle').textContent='Salir del editor';
        byId('map-toggle').setAttribute('aria-expanded','true');
        panel.hidden=false;bar.hidden=true;
        rebuild();
    }
    function deactivate() {
        if(!active)return;
        active=false;playing=false;pointer=null;keys.clear();
        panel.hidden=bar.hidden=true;byId('map-aim').hidden=true;
        document.body.classList.remove('map-workspace');
        byId('map-toggle').textContent='Editor de mapas';
        byId('map-toggle').setAttribute('aria-expanded','false');
    }
    function changeMode(play) {
        playing=play;panel.hidden=play;bar.hidden=!play;keys.clear();
        byId('map-instructions').textContent=instructions[project.profile];
        byId('map-directions').hidden=project.profile==='strategy'&&!('ontouchstart' in window);
        byId('map-aim').hidden=!play||project.profile!=='walk';
        initializeActors();resetView();refreshActors();
        if(play)canvas.focus();
    }
    function frame() {
        if(playing && project.profile==='walk'){
            const [x,z]=actor;
            const eye=[x,map.height(x,z)+1.7,z];
            const cos=Math.cos(camera.pitch);
            return {eye,target:plus(eye,[Math.sin(camera.yaw)*cos,Math.sin(camera.pitch),Math.cos(camera.yaw)*cos]),fov:66};
        }
        const [x,z]=playing&&project.profile==='urban'?actor:camera.target;
        const target=[x,map.height(x,z),z], cos=Math.cos(camera.pitch);
        const eye=plus(target,scale([Math.sin(camera.yaw)*cos,Math.sin(camera.pitch),Math.cos(camera.yaw)*cos],camera.distance));
        return {eye,target,half:camera.half,fov:53};
    }
    function pointAt(clientX,clientY) {
        const r=canvas.getBoundingClientRect(),c=cameraFrame();
        if(!r.width||!r.height)return;
        const x=(2*(clientX-r.left)/r.width-1)/c.p[0],y=(1-2*(clientY-r.top)/r.height)/c.p[5];
        const offset=plus(scale(c.right,x),scale(c.up,y));
        const origin=c.ortho?plus(c.eye,offset):c.eye;
        const raw=c.ortho?c.forward:plus(c.forward,offset);
        const direction=scale(raw,1/Math.hypot(...raw));
        const at=t=>plus(origin,scale(direction,t));
        let previous=null;
        for(let t=.1;t<=249;t+=.7){
            const p=at(t);
            if(Math.abs(p[0])>map.extent||Math.abs(p[2])>map.extent){previous=null;continue;}
            if(p[1]<=map.height(p[0],p[2]) && previous!==null){
                let a=previous,b=t;
                for(let n=0;n<10;n++){
                    const m=(a+b)/2,q=at(m);
                    if(q[1]>map.height(q[0],q[2]))a=m;else b=m;
                }
                const hit=at((a+b)/2);
                return limitPoint(map,[hit[0],hit[2]]);
            }
            previous=p[1]>map.height(p[0],p[2])?t:null;
        }
    }
    function handlePoint(point) {
        if(!point)return;
        const [x,z]=point;
        if(playing){
            if(project.profile!=='strategy')return;
            const picked=units.findIndex(u=>Math.hypot(x-u.point[0],z-u.point[1])<1.6);
            if(picked>=0){
                selected=picked;refreshActors();
                byId('map-instructions').textContent='Unidad seleccionada. Pulsa un destino.';
            }else if(selected>=0){
                const u=units[selected], route=findRoute(map,u.point,point);
                if(route.length)u.route=route;
                byId('map-instructions').textContent=route.length?'Orden de movimiento recibida.':'Destino inaccesible.';
            }
            return;
        }
        const tool=byId('map-tool').value, next=structuredClone(project);
        if(tool==='raise'||tool==='lower')next.edits.push({x,z,radius:byId('map-brush').valueAsNumber,delta:tool==='raise'?1.35:-1.35});
        else if(tool==='start'){
            if(!passable(map,x,z)){say('Elige un punto libre para iniciar.');return;}
            next.start=[x,z];
        }else if(tool==='erase'){
            const prop=map.placed.filter(p=>Math.hypot(x-p.x,z-p.z)<p.radius+1.8)
                .sort((a,b)=>Math.hypot(x-a.x,z-a.z)-Math.hypot(x-b.x,z-b.z))[0];
            if(!prop){say('Pulsa cerca de un objeto para retirarlo.');return;}
            if(typeof prop.id==='number')next.props.splice(prop.id,1);else next.removed.push(prop.id);
        }else if(['tree','house','rock'].includes(tool)){
            const footprint=tool==='house'?4.6:tool==='rock'?.7:.5;
            if(!passable(map,x,z,footprint)||Math.hypot(x-project.start[0],z-project.start[1])<footprint+.8){
                say('Busca espacio libre para colocarlo.');return;
            }
            next.props.push({type:tool,x,z,scale:1});
        }else return;
        replace(next);
    }
    canvas.addEventListener('pointerdown',e=>{
        if(!active || pointer || (e.button!==0&&e.button!==2))return;
        canvas.setPointerCapture(e.pointerId);
        if(playing)canvas.focus();
        pointer={id:e.pointerId,x:e.clientX,y:e.clientY,startX:e.clientX,startY:e.clientY,button:e.button,moved:false};
    });
    canvas.addEventListener('pointermove',e=>{
        if(!active||pointer?.id!==e.pointerId)return;
        const dx=e.clientX-pointer.x,dy=e.clientY-pointer.y;
        pointer.x=e.clientX;pointer.y=e.clientY;
        if(!pointer.moved&&Math.hypot(e.clientX-pointer.startX,e.clientY-pointer.startY)<5)return;
        pointer.moved=true;
        if(playing&&project.profile==='walk'){
            camera.yaw-=dx*.006;
            camera.pitch=Math.max(-1.3,Math.min(1.3,camera.pitch-dy*.006));
        }else if(pointer.button===2 && !(playing&&project.profile==='urban')){
            camera.yaw-=dx*.006;
            camera.pitch=Math.max(.32,Math.min(1.28,camera.pitch+dy*.006));
        }else{
            const c=cameraFrame(), ratio=2*camera.half/Math.max(1,canvas.clientHeight);
            camera.target=limitPoint(map,[camera.target[0]-dx*ratio*c.right[0]+dy*ratio*c.up[0],
                                          camera.target[1]-dx*ratio*c.right[2]+dy*ratio*c.up[2]]);
        }
    });
    canvas.addEventListener('pointerup',e=>{
        if(!active||pointer?.id!==e.pointerId)return;
        const click=!pointer.moved&&pointer.button===0;pointer=null;
        if(click)try{handlePoint(pointAt(e.clientX,e.clientY));}catch(error){say(error.message);}
    });
    for(const type of ['pointercancel','lostpointercapture'])canvas.addEventListener(type,()=>{pointer=null;});
    canvas.addEventListener('contextmenu',e=>{if(active)e.preventDefault();});
    canvas.addEventListener('wheel',e=>{
        if(!active)return;
        e.preventDefault();
        camera.half=Math.min(project.span*.8,Math.max(7,camera.half*Math.exp(e.deltaY*.001)));
    },{passive:false});
    window.addEventListener('keydown',e=>{
        if(!active||!playing||!keyNames[e.code]||e.target?.matches?.('input, textarea, select'))return;
        e.preventDefault();keys.add(keyNames[e.code]);
    });
    window.addEventListener('keyup',e=>keys.delete(keyNames[e.code]));
    window.addEventListener('blur',()=>{keys.clear();pointer=null;});
    document.querySelectorAll('[data-map-move]').forEach(button=>{
        const key=button.dataset.mapMove;
        button.onpointerdown=e=>{button.setPointerCapture(e.pointerId);keys.add(key);};
        button.onpointerup=button.onpointercancel=button.onlostpointercapture=()=>keys.delete(key);
    });
    function update(seconds) {
        if(!active||!playing)return;
        const dt=Math.min(seconds,.06), right=Number(keys.has('right'))-Number(keys.has('left')),
            front=Number(keys.has('up'))-Number(keys.has('down'));
        if(right||front){
            const sin=Math.sin(camera.yaw),cos=Math.cos(camera.yaw),walk=project.profile==='walk';
            const velocity=mapProfiles[project.profile].speed*dt/Math.hypot(front,right);
            const dx=velocity*(front*sin*(walk?1:-1)-right*cos*(walk?1:-1));
            const dz=velocity*(front*cos*(walk?1:-1)+right*sin*(walk?1:-1));
            if(project.profile==='strategy')camera.target=limitPoint(map,[camera.target[0]+dx*3,camera.target[1]+dz*3]);
            else{
                const next=moveOnMap(map,actor,dx,dz);
                if(next!==actor){actor=next;if(!walk)refreshActors();}
            }
        }
        if(project.profile!=='strategy')return;
        let moved=false;
        for(const unit of units){
            if(!unit.route.length)continue;
            const [x,z]=unit.point,[nx,nz]=unit.route[0],d=Math.hypot(nx-x,nz-z),step=dt*mapProfiles.strategy.speed;
            if(d<=step){unit.point=unit.route.shift();moved=true;}
            else if(step>0){unit.point=[x+(nx-x)*step/d,z+(nz-z)*step/d];moved=true;}
        }
        if(moved)refreshActors();
    }
    inputs.profile.onchange=()=>{
        const defaults=mapProfiles[inputs.profile.value];
        for(const key of ['span','relief','trees','buildings'])inputs[key].value=defaults[key];
    };
    byId('map-form').onsubmit=e=>{
        e.preventDefault();
        try {
            const fields=Object.fromEntries(Object.entries(inputs).map(([key,input])=>
                [key,key==='shadows'?input.checked:input.type==='number'||input.type==='range'?input.valueAsNumber:input.value]));
            replace({...project,...fields},true);
        }catch(error){say(error.message);}
    };
    byId('map-toggle').onclick=()=>active?leave():open();
    byId('map-play').onclick=()=>changeMode(true);
    byId('map-edit').onclick=()=>changeMode(false);
    byId('map-undo').onclick=()=>{if(undo.length){project=undo.pop();resetView();rebuild();save();}};
    byId('map-export').onclick=()=>download(new Blob([JSON.stringify(project,null,2)],{type:'application/json'}),
        'Pleiade-'+project.name.replace(/[^A-Za-z0-9_-]+/g,'-')+'.json');
    byId('map-import').onchange=async e=>{
        const file=e.target.files[0];if(!file)return;
        try{if(file.size>512000)throw new Error('El archivo supera el límite del editor.');replace(readMap(await file.text()),true);}
        catch(error){say(error.message);}finally{e.target.value='';}
    };
    return {frame,update,deactivate,resetView,get map(){return map;}};
}
