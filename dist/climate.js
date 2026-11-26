export const seasons=['auto','spring','summer','autumn','winter'];
export const weatherModes=['clear','rain','snow'];
const lerp=(a,b,t)=>a+(b-a)*t;
const profiles=[
    {leaves:.70,autumn:.04,frost:0,growth:1.16},
    {leaves:1,autumn:0,frost:0,growth:1},
    {leaves:.55,autumn:.90,frost:0,growth:.98},
    {leaves:.03,autumn:.80,frost:.50,growth:.88}
];

export function climateAt(season,weather,days,cloudCover=0){
    const phase=((days/96+.32)%1+1)%1*4;
    const index=season==='auto'?Math.floor(phase):Math.max(0,seasons.indexOf(season)-1);
    const a=profiles[index],b=profiles[(index+1)%4];
    const t=season==='auto'?(phase%1)**2*(3-2*(phase%1)):0;
    const leaves=lerp(a.leaves,b.leaves,t);
    const autumn=lerp(a.autumn,b.autumn,t);
    const cloudLoad=Math.max(0,Math.min(1,(cloudCover-.40)/.55));
    const precipitation=weather==='clear'?0:cloudLoad*cloudLoad*(3-2*cloudLoad);
    const snowfall=weather==='snow'?.78*Math.sqrt(precipitation)+.20*precipitation:0;
    const frost=Math.max(lerp(a.frost,b.frost,t),snowfall);
    return {leaves,autumn,frost,precipitation,growth:lerp(a.growth,b.growth,t),wet:weather==='rain'?.66*precipitation:0,
            wind:lerp(.62,weather==='rain'?1.8:.72,precipitation),
            fall:autumn>.40&&leaves>.09};
}
