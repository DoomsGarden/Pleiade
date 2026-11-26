import {channel,channelWaterExtent,waterBase} from './watershed.js?v=0.000.00.027';

const station=24;
const halfSpan=5.48;
const halfApproach=9.50;
const width=2.28;
const supportWidth=width+.32;
const supportLength=.90;
const supportAt=5.31;
const decking=.115;
const beamDepth=.30;
const crest=.043;
const minimumClearance=.26;
const wood=27,stone=28;
const plankTint=[.79,.73,.65],beamTint=[.65,.60,.55],stoneTint=[.80,.81,.78];

// The crossing frame uses the local normal of the river centreline. All
// geometry and clearance measurements use the same points in that frame.
export function createRiverBridge(mode,watershed,meshIds,{floodRise=.30,style='timber'}={}){
    if(mode==='lake')return null;
    if(mode!=='river'&&mode!=='falls')throw new RangeError('Tramo sin cauce');
    if(!Number.isFinite(floodRise)||floodRise<0)throw new RangeError('Cota de crecida inválida');
    if(style!=='timber'&&style!=='roman')throw new RangeError('Estilo de puente desconocido');
    const center=[channel(station),station];
    const slope=(channel(station+.05)-channel(station-.05))/.10;
    const normal=Math.hypot(1,slope);
    const axis=[1/normal,-slope/normal];
    const across=[-axis[1],axis[0]];
    const point=(u,y,v=0)=>[center[0]+u*axis[0]+v*across[0],y,center[1]+u*axis[1]+v*across[1]];
    const surface=(u,v=0)=>{
        const [x,,z]=point(u,0,v);
        return watershed.height(x,z);
    };
    if(style==='roman')return createRomanBridge(mode,center,axis,across,point,surface,floodRise);
    const designWater=Math.max(...[-1,1].flatMap(side=>[-1,1].map(edge=>
        waterBase(mode,point(side*halfSpan,0,edge*supportWidth/2)[2])
    )))+crest+floodRise;

    const supports=[-1,1].map(side=>{
        const u=side*supportAt;
        const samples=[-.5,0,.5].flatMap(i=>[-.5,0,.5].map(j=>{
            const along=u+i*supportLength,across=j*supportWidth;
            const [x,,z]=point(along,0,across);
            return {height:surface(along,across),edge:Math.abs(x-channel(z))-channelWaterExtent(z,mode)};
        }));
        const ground=Math.min(...samples.map(sample=>sample.height));
        const high=Math.max(...samples.map(sample=>sample.height));
        const waterOffset=Math.min(...samples.map(sample=>sample.edge));
        return {side,u,ground,bottom:ground-.18,high,waterOffset};
    });
    const entrances=[-1,1].map(side=>{
        const u=side*halfApproach;
        const height=surface(u)+.015;
        const low=Math.min(...[-width/2,0,width/2].map(v=>surface(u,v)));
        return {side,u,height,low};
    });
    if(supports.some(s=>s.waterOffset<=.14)||entrances.some(e=>e.low<designWater+.08))return null;

    const deckY=Math.max(
        designWater+minimumClearance+decking+beamDepth,
        ...supports.map(s=>s.high+.045+decking+beamDepth),
        ...entrances.map(e=>e.height)
    );
    const underside=deckY-decking-beamDepth;
    const approaches=entrances.map(e=>({
        side:e.side,inner:point(e.side*halfSpan,deckY),outer:point(e.u,e.height),
        grade:Math.abs(deckY-e.height)/(halfApproach-halfSpan),ground:e.low,
    }));
    if(approaches.some(a=>a.grade>.24))return null;
    const objects=[];
    function box(u,y,v,length,height,depth,tint,pattern=wood,pitch=0){
        const c=Math.cos(pitch),s=Math.sin(pitch);
        const [x,,z]=point(u,y,v);
        objects.push({mesh:meshIds.box,instance:[
            axis[0]*length*c,length*s,axis[1]*length*c,0,
            -axis[0]*height*s,height*c,-axis[1]*height*s,0,
            across[0]*depth,0,across[1]*depth,0,
            x,y,z,1,...tint,1,.86,0,pattern,0,
        ]});
    }
    function topBetween(a,ay,b,by,depth,thickness,tint,v=0){
        const pitch=Math.atan2(by-ay,b-a);
        box((a+b)/2+Math.sin(pitch)*thickness/2,(ay+by)/2-Math.cos(pitch)*thickness/2,
            v,Math.hypot(b-a,by-ay),thickness,depth,tint,wood,pitch);
    }
    for(const support of supports){
        const {u,bottom}=support;
        const top=underside+.035;
        const cap=.135;
        box(u,(bottom+top-cap)/2,0,supportLength-.05,top-cap-bottom,supportWidth-.04,stoneTint,stone);
        box(u,top-cap/2,0,supportLength,cap,supportWidth,[.85,.85,.82],stone);
        support.center=point(u,(bottom+top)/2);
        support.top=top;
    }
    // Longitudinal beams, crossmembers and a continuous backing under the planks.
    for(const v of [-.86,0,.86])
        box(0,underside+beamDepth/2,v,2*halfSpan,beamDepth,.225,beamTint);
    for(const u of [-4.72,0,4.72])
        box(u,deckY-decking-.145,0,.20,.19,width+.12,beamTint);
    box(0,deckY-decking+.028,0,2*halfSpan,.056,width,beamTint);
    const boards=28,step=2*halfSpan/boards;
    for(let i=0;i<boards;i++){
        const wear=.93+.12*Math.sin(i*2.37)+.035*Math.cos(i*.93);
        const gap=(i===0||i===boards-1)?.007:.012;
        box(-halfSpan+(i+.5)*step,deckY-.044,0,step-gap,.088,width,
            plankTint.map(tint=>tint*wear));
    }
    for(const v of [-width/2+.065,width/2-.065]){
        for(let i=0;i<6;i++){
            const u=-halfSpan+.15+i*(2*halfSpan-.30)/5;
            box(u,deckY+.45,v,.125,.93,.125,beamTint);
        }
        box(0,deckY+.91,v,2*halfSpan,.10,.16,plankTint);
        box(0,deckY+.49,v,2*halfSpan-.08,.082,.095,beamTint);
    }
    for(const {side,u,height} of entrances){
        const inside=side*halfSpan;
        const [a,ay,b,by]=side<0?[u,height,inside,deckY]:[inside,deckY,u,height];
        const yAt=t=>ay+(by-ay)*t;
        const uAt=t=>a+(b-a)*t;
        topBetween(a,ay-.06,b,by-.06,width,.19,beamTint);
        for(let i=0;i<8;i++)
            topBetween(uAt(i/8)+.005,yAt(i/8),uAt((i+1)/8)-.005,yAt((i+1)/8),
                width,.08,plankTint.map(tint=>tint*(.96+.075*Math.sin(i*2.2+side))));
        // Timber legs join the underside to solid, dry ground wherever the
        // ramp is high enough to span it. Near the landing the beam is buried.
        const normalY=(b-a)/Math.hypot(b-a,by-ay);
        for(const t of [.18,.57]){
            const footing=inside+(u-inside)*t;
            const top=deckY+(height-deckY)*t-.06-.19/normalY+.025;
            for(const v of [-.76,.76]){
                const bottom=surface(footing,v)-.10;
                if(top-bottom>.12)
                    box(footing,(top+bottom)/2,v,.18,top-bottom,.18,beamTint);
            }
        }
        for(const v of [-width/2+.065,width/2-.065]){
            const end=inside+(u-inside)*.79;
            const yEnd=deckY+(height-deckY)*.79;
            box(end,yEnd+.41,v,.12,.82,.12,beamTint);
            if(side<0)topBetween(end,yEnd+.88,inside,deckY+.96,.14,.092,plankTint,v);
            else topBetween(inside,deckY+.96,end,yEnd+.88,.14,.092,plankTint,v);
        }
    }
    return {objects,focus:point(0,deckY+.28),center,axis,span:2*halfSpan,width,deckY,designWater,
        clearance:underside-designWater,floodRise,supports,approaches};
}

function createRomanBridge(mode,center,axis,across,point,surface,floodRise){
    const radius=5.14;
    const ringThickness=.43;
    const footEnd=radius+.92;
    const deckWidth=2.96;
    const sideWall=deckWidth/2;
    const designWater=Math.max(...[-footEnd,footEnd].flatMap(u=>[-sideWall,sideWall].map(v=>
        waterBase(mode,point(u,0,v)[2]))))+crest+floodRise;
    const supports=[-1,1].map(side=>{
        const u=side*(radius+footEnd)/2;
        const samples=Array.from({length:5},(_,i)=>side*(radius+(footEnd-radius)*i/4))
            .flatMap(along=>[-sideWall,-sideWall/2,0,sideWall/2,sideWall].map(v=>{
                const [x,,z]=point(along,0,v);
                return {height:surface(along,v),offset:Math.abs(x-channel(z))-channelWaterExtent(z,mode)};
            }));
        const ground=Math.min(...samples.map(sample=>sample.height));
        return {side,u,ground,bottom:ground-.22,high:Math.max(...samples.map(sample=>sample.height)),
            waterOffset:Math.min(...samples.map(sample=>sample.offset))};
    });
    if(supports.some(support=>support.waterOffset<=.14))return null;

    const springY=Math.max(designWater+.18,...supports.map(support=>support.high+.08));
    const deckY=springY+radius+ringThickness+.38;
    const deckAt=u=>deckY-.115*(Math.hypot(u,.58)-.58);
    const approaches=[];
    const ramps=[];
    const lanes=[-sideWall,0,sideWall];
    const rampGrade=.24-1e-6;
    for(const side of [-1,1]){
        let chosen;
        const inner=side*footEnd;
        for(let end=25;end<=44;end+=.75){
            const outer=side*end;
            const length=end-footEnd;
            const samples=Math.ceil(length/1.1);
            const sections=Array.from({length:samples+1},(_,i)=>{
                const t=i/samples,u=inner+(outer-inner)*t;
                const ground=lanes.map(v=>surface(u,v));
                return {u,bottom:Math.min(...ground)-.18,ground};
            });
            const entrance=sections[samples];
            // A landing is far beyond the modelled flood section. Fit it to
            // the dry bank even when its terrain is lower than the river.
            const waterOffset=Math.min(...lanes.map(v=>{
                const [x,,z]=point(outer,0,v);
                return Math.abs(x-channel(z))-channelWaterExtent(z,mode);
            }));
            if(waterOffset<=.14)continue;
            const innerY=deckAt(inner);
            const heights=entrance.ground.map(y=>y+.025);
            if(heights.some(y=>Math.abs(innerY-y)/length>rampGrade))continue;
            // Profile each side and the centreline into the existing ground.
            // Spreading local high points in both directions limits the grade.
            const step=length/samples,limit=rampGrade*step;
            const tracks=heights.map((height,lane)=>{
                const ys=sections.map((section,i)=>
                    Math.max(innerY+(height-innerY)*i/samples,section.ground[lane]+.025));
                for(let i=1;i<=samples;i++)ys[i]=Math.max(ys[i],ys[i-1]-limit);
                for(let i=samples-1;i>=0;i--)ys[i]=Math.max(ys[i],ys[i+1]-limit);
                return ys;
            });
            if(tracks.some((ys,lane)=>ys[0]!==innerY||ys[samples]>heights[lane]+1e-6))continue;
            let grade=0;
            for(const ys of tracks)for(let i=1;i<=samples;i++)grade=Math.max(grade,Math.abs(ys[i]-ys[i-1])/step);
            for(const [i,section] of sections.entries())section.top=tracks.map(ys=>ys[i]);
            chosen={side,inner:point(inner,innerY),outer:point(outer,entrance.top[1]),grade,
                ground:Math.min(...entrance.ground),waterOffset};
            ramps.push(side<0?sections.reverse():sections);
            break;
        }
        if(!chosen)return null;
        approaches.push(chosen);
    }

    const objects=[],meshes=[];
    function mesh(id,tint,pattern,draw){
        const positions=[],normals=[],triangles=[];
        function face(a,b,c,d){
            const ab=b.map((value,i)=>value-a[i]),ac=c.map((value,i)=>value-a[i]);
            const n=[ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]];
            const start=positions.length;
            positions.push(a,b,c,d);normals.push(n,n,n,n);
            triangles.push(start,start+1,start+2,start,start+2,start+3);
        }
        function strip(sections,left,right){
            const corners=({u,bottom,top})=>{
                const [topLeft,topRight]=Array.isArray(top)?top:[top,top];
                return [[u,bottom,left],[u,bottom,right],[u,topLeft,left],[u,topRight,right]];
            };
            let [al,ar,at,ap]=corners(sections[0]);
            face(al,ar,ap,at);
            for(const section of sections.slice(1)){
                const [bl,br,bt,bp]=corners(section);
                face(at,ap,bp,bt);face(al,bl,br,ar);
                face(ar,br,bp,ap);face(al,at,bt,bl);
                [al,ar,at,ap]=[bl,br,bt,bp];
            }
            face(ar,al,at,ap);
        }
        draw(face,strip);
        const low=[0,1,2].map(i=>Math.min(...positions.map(p=>p[i])));
        const high=[0,1,2].map(i=>Math.max(...positions.map(p=>p[i])));
        const middle=low.map((value,i)=>(value+high[i])/2);
        const size=low.map((value,i)=>high[i]-value);
        const [length,height,depth]=size;
        const vertices=new Float32Array(positions.length*8);
        for(const [i,position] of positions.entries()){
            const n=normals[i].map((value,j)=>value*size[j]);
            const magnitude=Math.hypot(...n);
            vertices.set([
                ...position.map((value,j)=>(value-middle[j])/size[j]),
                ...n.map(value=>value/magnitude),position[0]*.35,position[1]*.35,
            ],i*8);
        }
        meshes.push({id,vertices,indices:new Uint32Array(triangles)});
        objects.push({mesh:id,instance:[
            axis[0]*length,0,axis[1]*length,0,
            0,height,0,0,
            across[0]*depth,0,across[1]*depth,0,
            ...point(...middle),1,...tint,1,.89,0,pattern,0,
        ]});
    }

    const barrel=[];
    const slices=40;
    for(let i=0;i<=slices;i++){
        const angle=i*Math.PI/slices,u=-radius*Math.cos(angle);
        barrel.push({u,bottom:springY+radius*Math.sin(angle),top:deckAt(u)});
    }
    const body=[{u:-footEnd,bottom:springY,top:deckAt(-footEnd)},...barrel,
        {u:footEnd,bottom:springY,top:deckAt(footEnd)}];
    mesh(70,[.76,.73,.68],stone,(_,strip)=>{
        strip(body,-sideWall,sideWall);
        for(const support of supports){
            const a=support.side<0?-footEnd:radius,b=support.side<0?-radius:footEnd;
            support.top=springY;
            support.center=point(support.u,(support.bottom+support.top)/2);
            strip([a,b].map(u=>({u,bottom:support.bottom,top:support.top})),-sideWall,sideWall);
        }
    });
    // The arch barrel remains continuous behind the separate face stones.
    // Small radial joints expose its darker backing across the two façades.
    mesh(71,[.88,.85,.79],31,(face)=>{
        const stones=19,angleStep=Math.PI/stones;
        const ringPoint=(angle,r,v)=>[-r*Math.cos(angle),springY+r*Math.sin(angle),v];
        for(const side of [-1,1])for(let i=0;i<stones;i++){
            const start=i*angleStep+.003,end=(i+1)*angleStep-.003;
            const angles=[start,(start+end)/2,end];
            const v=side*(sideWall+.045),back=side*(sideWall-.012);
            const inside=angles.map(angle=>ringPoint(angle,radius+.024,v));
            const outside=angles.map(angle=>ringPoint(angle,radius+ringThickness,v));
            for(let j=0;j<2;j++){
                const front=[inside[j],inside[j+1],outside[j+1],outside[j]];
                face(...(side>0?front:front.reverse()));
                const lip=[inside[j],inside[j+1],[...inside[j+1].slice(0,2),back],[...inside[j].slice(0,2),back]];
                face(...(side>0?lip.reverse():lip));
                const shoulder=[outside[j],outside[j+1],[...outside[j+1].slice(0,2),back],[...outside[j].slice(0,2),back]];
                face(...(side<0?shoulder.reverse():shoulder));
            }
            for(const j of [0,2]){
                const edge=[inside[j],outside[j],[...outside[j].slice(0,2),back],[...inside[j].slice(0,2),back]];
                face(...(side*(j===0?1:-1)>0?edge:edge.reverse()));
            }
        }
    });
    mesh(72,[.83,.79,.72],stone,(_,strip)=>{
        const parapet=Array.from({length:13},(_,i)=>{
            const u=(i/6-1)*footEnd;
            return {u,bottom:deckAt(u)-.045,top:deckAt(u)+.76};
        });
        for(const side of [-1,1]){
            const edge=side*(sideWall-.11);
            strip(parapet,edge-.105,edge+.105);
            strip(parapet.map(({u,top})=>({u,bottom:top-.035,top:top+.08})),edge-.13,edge+.13);
        }
    });
    ramps.forEach((ramp,i)=>mesh(73+i,[.76,.73,.68],stone,(_,strip)=>{
        strip(ramp.map(section=>({...section,top:section.top.slice(0,2)})),-sideWall,0);
        strip(ramp.map(section=>({...section,top:section.top.slice(1)})),0,sideWall);
    }));
    const [left,right]=ramps;
    return {objects,meshes,style:'roman',focus:point(0,springY+radius*.48),center,axis,span:radius*2,width:deckWidth,
        deckY,designWater,clearance:springY-designWater,floodRise,supports,approaches,arch:{radius,springY},
        footprint:{minU:left[0].u,maxU:right.at(-1).u,halfWidth:sideWall+.045}};
}
