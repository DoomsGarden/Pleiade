from pathlib import Path
import subprocess,json
from PIL import Image
import numpy as np
import os
root=Path(os.environ['PLEIADE_ENGINE_ROOT']);out=root/'validation-water03';out.mkdir(exist_ok=True)
cases={'old':['--water-time','0'],'new':['--water-time','0'],'calm0':['--wave-strength','0','--water-time','0'],'calm3':['--wave-strength','0','--water-time','3'],'gentle0':['--wave-strength','35','--water-time','0'],'gentle3':['--wave-strength','35','--water-time','3'],'no-ssr':['--wave-strength','35','--no-water-ssr'],'strategy':['--wave-strength','35','--camera','strategy']}
for name,args in cases.items():
 d=out/name;d.mkdir(exist_ok=True)
 exe=os.environ['PLEIADE_BASELINE_BINARY'] if name=='old' else str(root/'build-cmake/pleiade')
 cmd=[exe,'--asset-dir',str(root/'assets/runtime'),'--water-demo','--headless','--frames','3','--freeze','--no-hud','--width','960','--height','540','--output',str(d),'--capture',str(d/'frame.bmp'),*args]
 p=subprocess.run(cmd,capture_output=True,text=True);(d/'run.log').write_text(p.stdout+p.stderr);assert p.returncode==0,(name,p.stderr)
 Image.open(d/'frame.bmp').save(d/'frame.png');print(name,'PASS',flush=True)
def pix(name):return np.asarray(Image.open(out/name/'frame.bmp')).astype(int)
def changed(a,b):return int(np.count_nonzero(np.max(abs(pix(a)-pix(b)),axis=2)>2))
assert np.array_equal(pix('calm0'),pix('calm3'))
assert changed('gentle0','gentle3')>100
assert changed('gentle0','no-ssr')>100
metrics={'calm_identical_over_time':True,'wave_changed_pixels':changed('gentle0','gentle3'),'ssr_changed_pixels':changed('gentle0','no-ssr'),'before_after_changed_pixels':changed('old','new'),'renderer':'Mesa llvmpipe software; not Android WebGL'}
(out/'checks.json').write_text(json.dumps(metrics,indent=2));print(metrics)
