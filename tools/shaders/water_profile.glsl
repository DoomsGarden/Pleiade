float channelCenter(float z){
 return -3.3+3.1*sin(z*.056)+1.3*sin(z*.133+.9);
}
float plungePool(float z){
 float downstream=-z-2.5;
 return smoothstep(0.0,3.8,downstream)*(1.0-smoothstep(4.0,13.5,downstream));
}
float channelWaterExtent(float z,int mode){
 return 4.52+(mode==2?2.4*plungePool(z):0.0);
}
float waterLevel(float z,int mode){
 return mode==0?0.0:.44+z*.010+(mode==2?3.65*smoothstep(-2.5,.85,z):0.0);
}
