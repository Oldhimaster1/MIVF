"""Phase C.5b.2 nondestructive dashboard-background recipe and readability pipeline."""
from __future__ import annotations
import hashlib, math, struct
from pathlib import Path
from PIL import Image, ImageEnhance, ImageFilter, ImageOps
W,H=320,240
MODES=('builtin','custom','generated')
DEFAULT={
 'fit':'contain','scale':1.0,'x':0.0,'y':0.0,
 'blur':0.0,'darken':0.0,'brightness':0.0,'contrast':0.0,'saturation':1.0,
 'tint':[255,255,255],'tint_strength':0.0,'vignette':0.0,
 'style':'foundation_gradient','secondary_rgb':[4,8,18],
}
def _clamp(v,d,a,b):
 try:v=float(v)
 except (TypeError,ValueError):v=d
 return max(a,min(b,v))
def _rgb(v,default):
 if not isinstance(v,(list,tuple)) or len(v)!=3:return list(default)
 return [int(_clamp(x,default[i],0,255)) for i,x in enumerate(v)]
def normalize(raw=None):
 d=dict(DEFAULT);d.update(raw or {})
 d['scale']=_clamp(d.get('scale'),1,.25,4);d['x']=_clamp(d.get('x'),0,-1,1);d['y']=_clamp(d.get('y'),0,-1,1)
 d['blur']=_clamp(d.get('blur'),0,0,12);d['darken']=_clamp(d.get('darken'),0,0,1)
 d['brightness']=_clamp(d.get('brightness'),0,-1,1);d['contrast']=_clamp(d.get('contrast'),0,-1,1);d['saturation']=_clamp(d.get('saturation'),1,0,2)
 d['tint']=_rgb(d.get('tint'),[255,255,255]);d['tint_strength']=_clamp(d.get('tint_strength'),0,0,1);d['vignette']=_clamp(d.get('vignette'),0,0,1)
 if d.get('fit') not in ('contain','cover','center_crop','stretch'):d['fit']='contain'
 d['secondary_rgb']=_rgb(d.get('secondary_rgb'),[4,8,18]);d['style']='foundation_gradient'
 return d
def effective_mode(artwork):
 mode=artwork.get('dashboard_bg_mode') if isinstance(artwork,dict) else getattr(artwork,'dashboard_bg_mode',None)
 source=artwork.get('dashboard_bg') if isinstance(artwork,dict) else getattr(artwork,'dashboard_bg',None)
 return mode if mode in MODES else ('custom' if source else 'builtin')
def _fit(img,d):
 img=ImageOps.exif_transpose(img).convert('RGBA');mode=d['fit']
 if mode=='stretch':fitted=img.resize((W,H),Image.Resampling.LANCZOS)
 elif mode in ('cover','center_crop'):fitted=ImageOps.fit(img,(W,H),Image.Resampling.LANCZOS,centering=(.5,.5))
 else:fitted=ImageOps.contain(img,(W,H),Image.Resampling.LANCZOS)
 nw=max(1,round(fitted.width*d['scale']));nh=max(1,round(fitted.height*d['scale']));fitted=fitted.resize((nw,nh),Image.Resampling.LANCZOS)
 canvas=Image.new('RGBA',(W,H),(0,0,0,255));x=(W-nw)//2+round(d['x']*W/2);y=(H-nh)//2+round(d['y']*H/2);canvas.alpha_composite(fitted,(x,y));return canvas.convert('RGB')
def _generated(d,accent):
 top=tuple(int(c) for c in accent);bottom=tuple(d['secondary_rgb']);im=Image.new('RGB',(W,H));px=im.load()
 for y in range(H):
  t=y/(H-1);row=tuple(round(top[i]*(1-t)+bottom[i]*t) for i in range(3))
  for x in range(W):px[x,y]=row
 return im
def _vignette(img,strength):
 if not strength:return img
 px=img.load();cx=(W-1)/2;cy=(H-1)/2;maxd=math.sqrt(cx*cx+cy*cy)
 for y in range(H):
  for x in range(W):
   edge=(math.sqrt((x-cx)**2+(y-cy)**2)/maxd)**1.7;factor=1-strength*edge*.82;r,g,b=px[x,y];px[x,y]=(round(r*factor),round(g*factor),round(b*factor))
 return img
def _adjust(img,d):
 if d['blur']:img=img.filter(ImageFilter.GaussianBlur(d['blur']))
 img=ImageEnhance.Brightness(img).enhance(1+d['brightness']);img=ImageEnhance.Contrast(img).enhance(1+d['contrast']);img=ImageEnhance.Color(img).enhance(d['saturation'])
 if d['tint_strength']:img=Image.blend(img,Image.new('RGB',(W,H),tuple(d['tint'])),d['tint_strength'])
 if d['darken']:img=Image.blend(img,Image.new('RGB',(W,H),(0,0,0)),d['darken'])
 return _vignette(img,d['vignette'])
def _rgb565(img):
 out=bytearray()
 for r,g,b in img.getdata():v=((r>>3)<<11)|((g>>2)<<5)|(b>>3);out+=bytes((v&255,v>>8))
 return bytes(out)
def _runtime(img):
 out=Image.new('RGBA',img.size);src=img.load();dst=out.load()
 for y in range(img.height):
  for x in range(img.width):
   r,g,b=src[x,y];dst[x,y]=((r>>3)<<3,(g>>2)<<2,(b>>3)<<3,255)
 return out
def render(mode,source=None,recipe=None,accent=(70,120,210),base=None):
 if mode=='builtin':return None
 d=normalize(recipe)
 if mode=='custom':
  if not source:raise ValueError('Custom dashboard background has no source image')
  p=Path(source);p=(base/p).resolve() if base and not p.is_absolute() else p.resolve()
  if not p.is_file():raise ValueError(f'Source artwork does not exist: {p}')
  with Image.open(p) as im:rgb=_fit(im,d)
 else:rgb=_generated(d,accent)
 prepared=rgb.copy();rgb=_adjust(rgb,d);pixels=_rgb565(rgb);asset=b'MVCA'+struct.pack('<IHH',1,W,H)+pixels
 return {'asset':asset,'sha':hashlib.sha256(asset).hexdigest(),'prepared':prepared,'adjusted':rgb,'runtime':_runtime(rgb),'recipe':d,'width':W,'height':H,'source':str(source) if source else None}
