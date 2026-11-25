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
export function createRiverBridge(mode,watershed,meshIds,{floodRise=.30}={}){
    if(mode==='lake')return null;
    if(mode!=='river'&&mode!=='falls')throw new RangeError('Tramo sin cauce');
    if(!Number.isFinite(floodRise)||floodRise<0)throw new RangeError('Cota de crecida inválida');
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
