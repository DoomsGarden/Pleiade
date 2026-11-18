"""Continuity and normalization of the nine-tap spatial kernel (not image QA)."""
import math, random
r=random.Random(17);grid={(x,y):r.randrange(2) for x in range(-4,5) for y in range(-4,5)}
def filtered(x,y,value=None):
 bx,by=math.floor(x),math.floor(y);px,py=x-bx-.5,y-by-.5
 total=result=0
 for j in [-1,0,1]:
  for i in [-1,0,1]:
   w=max(0,1.5-abs(i-px))*max(0,1.5-abs(j-py));total+=w;result+=w*(value if value is not None else grid[(bx+i,by+j)])
 return result/total
max_jump=0
for k in range(1000):
 x,y=r.uniform(-2,2),r.uniform(-2,2)
 assert abs(filtered(x,y,1)-1)<1e-12 and filtered(x,y,0)==0
 assert 0<=filtered(x,y)<=1
 boundary=r.randrange(-1,2)
 jump=max(abs(filtered(boundary-1e-7,y)-filtered(boundary+1e-7,y)),abs(filtered(x,boundary-1e-7)-filtered(x,boundary+1e-7)))
 max_jump=max(max_jump,jump)
assert max_jump<1e-6,max_jump
print('PASS: 1000 positions; constant light/shadow preserved; maximum texel-boundary jump',max_jump)
