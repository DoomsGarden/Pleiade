float channelCenter(float z){
 return -3.3+3.1*sin(z*.056)+1.3*sin(z*.133+.9);
}
float waterLevel(float z,int mode){
 return mode==0?0.0:.44+z*.010+(mode==2?3.65*smoothstep(-2.5,1.7,z):0.0);
}
