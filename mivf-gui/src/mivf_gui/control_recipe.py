from pathlib import Path
from PIL import Image,ImageOps,ImageEnhance,ImageChops,ImageFilter,ImageDraw
import hashlib,struct
SPECS={'rewind':(64,60,27),'play_pause':(74,78,37),'fast_forward':(64,60,27),'movie_menu_back':(222,20,None)}
DEFAULT={'source':None,'fit':'contain','scale':1.0,'x':0.0,'y':0.0,'mask':'combined','threshold':128,'feather':0.0,'brightness':0.0,'contrast':0.0,'saturation':1.0,'tint':[255,255,255],'tint_strength':0.0,
 'corner_radius':8.0,'mask_inset':0.0,
 'chroma_key':[0,255,0],'chroma_tolerance':60.0,'chroma_edge_softness':0.0,'chroma_invert':False,'chroma_mode':'chroma_only'}
def clamp(v,d,a,b):
 try:v=float(v)
 except:v=d
 return max(a,min(b,v))
def norm(raw,control):
 d=dict(DEFAULT);d.update(raw or {});d['scale']=clamp(d.get('scale'),1,.25,4);d['x']=clamp(d.get('x'),0,-1,1);d['y']=clamp(d.get('y'),0,-1,1);d['threshold']=int(clamp(d.get('threshold'),128,1,255));d['feather']=clamp(d.get('feather'),0,0,4);d['brightness']=clamp(d.get('brightness'),0,-1,1);d['contrast']=clamp(d.get('contrast'),0,-1,1);d['saturation']=clamp(d.get('saturation'),1,0,2);d['tint_strength']=clamp(d.get('tint_strength'),0,0,1)
 if d.get('fit') not in ('contain','cover','center_crop','stretch'):d['fit']='contain'
 w,h,_=SPECS[control];half=min(w,h)/2
 d['corner_radius']=clamp(d.get('corner_radius'),8,0,half);d['mask_inset']=clamp(d.get('mask_inset'),0,0,half/2)
 kc=d.get('chroma_key') or [0,255,0];d['chroma_key']=[int(clamp(v,0,0,255)) for v in (list(kc)+[0,255,0])[:3]]
 d['chroma_tolerance']=clamp(d.get('chroma_tolerance'),60,0,441);d['chroma_edge_softness']=clamp(d.get('chroma_edge_softness'),0,0,220);d['chroma_invert']=bool(d.get('chroma_invert'))
 if d.get('chroma_mode') not in ('chroma_only','alpha_and_chroma'):d['chroma_mode']='chroma_only'
 ok=('alpha','full','rounded_rect','chroma_key') if SPECS[control][2] is None else ('alpha','disc','combined','full','rounded_rect','chroma_key')
 if d.get('mask') not in ok:d['mask']='alpha' if SPECS[control][2] is None else 'combined'
 return d
# Phase C.4A: grouped Focused inheritance. Each group can independently
# inherit Idle's value or use its own Focused-specific value, instead of
# one all-or-nothing switch. Backward compatibility: an older recipe only
# ever wrote a single `inherit_idle` boolean -- if the newer per-group keys
# are absent, all three groups derive from that one legacy flag, so an old
# project's Focused state renders exactly as it did before this change.
SOURCE_GEOMETRY_FIELDS = ('source', 'fit', 'scale', 'x', 'y')
MASK_FIELDS = ('mask', 'threshold', 'feather', 'corner_radius', 'mask_inset',
               'chroma_key', 'chroma_tolerance', 'chroma_edge_softness', 'chroma_invert', 'chroma_mode')
ADJUSTMENT_FIELDS = ('brightness', 'contrast', 'saturation', 'tint', 'tint_strength')

def state(edits,control,name,source,fit):
 c=(edits or {}).get(control,{}) or {}; idle=norm(c.get('idle'),control);idle['source']=idle.get('source') or source;idle['fit']=idle.get('fit') or fit
 if name=='idle':return idle
 f=c.get('focused',{}) or {}
 legacy=f.get('inherit_idle',True)
 inherit_geom=f.get('inherit_source_geometry',legacy)
 inherit_mask=f.get('inherit_mask',legacy)
 inherit_adjust=f.get('inherit_adjustments',legacy)
 o=dict(idle)
 if not inherit_geom:
  for k in SOURCE_GEOMETRY_FIELDS:
   if k in f:o[k]=f[k]
 if not inherit_mask:
  for k in MASK_FIELDS:
   if k in f:o[k]=f[k]
 if not inherit_adjust:
  for k in ADJUSTMENT_FIELDS:
   if k in f:o[k]=f[k]
 return norm(o,control)
def render(control,recipe,base=None):
 d=norm(recipe,control);p=Path(d['source']);p=(base/p).resolve() if base and not p.is_absolute() else p.resolve()
 if not p.is_file():raise ValueError(f'Missing artwork: {p}')
 w,h,r=SPECS[control]
 im=ImageOps.exif_transpose(Image.open(p)).convert('RGBA')
 if d['fit']=='stretch':im=im.resize((w,h),Image.Resampling.LANCZOS)
 elif d['fit'] in ('cover','center_crop'):im=ImageOps.fit(im,(w,h),Image.Resampling.LANCZOS)
 else:im=ImageOps.contain(im,(w,h),Image.Resampling.LANCZOS)
 im=im.resize((max(1,round(im.width*d['scale'])),max(1,round(im.height*d['scale']))),Image.Resampling.LANCZOS);can=Image.new('RGBA',(w,h));can.alpha_composite(im,((w-im.width)//2+round(d['x']*w/2),(h-im.height)//2+round(d['y']*h/2)))
 rgb=ImageEnhance.Color(ImageEnhance.Contrast(ImageEnhance.Brightness(can.convert('RGB')).enhance(1+d['brightness'])).enhance(1+d['contrast'])).enhance(d['saturation'])
 if d['tint_strength']:rgb=Image.blend(rgb,Image.new('RGB',(w,h),tuple(d['tint'])),d['tint_strength'])
 alpha=can.getchannel('A');full=Image.new('L',(w,h),255)
 if r:
  disc=Image.new('L',(w,h));px=disc.load();cx=(w-1)/2;cy=(h-1)/2
  for yy in range(h):
   for xx in range(w):px[xx,yy]=255 if (xx-cx)**2+(yy-cy)**2<=r*r else 0
 else:disc=full
 if d['mask']=='rounded_rect':
  inset=d['mask_inset'];rr=Image.new('L',(w,h),0)
  ImageDraw.Draw(rr).rounded_rectangle([inset,inset,w-1-inset,h-1-inset],radius=d['corner_radius'],fill=255)
  mask=rr
 elif d['mask']=='chroma_key':
  kr,kg,kb=d['chroma_key'];tol=d['chroma_tolerance'];soft=d['chroma_edge_softness']
  src_rgb=can.convert('RGB');px=src_rgb.load();chroma=Image.new('L',(w,h));cpx=chroma.load()
  for yy in range(h):
   for xx in range(w):
    sr,sg,sb=px[xx,yy];dist=((sr-kr)**2+(sg-kg)**2+(sb-kb)**2)**0.5
    if soft>0:
     lo=tol-soft;hi=tol+soft
     v=0 if dist<=lo else(255 if dist>=hi else round(255*(dist-lo)/(hi-lo)))
    else:v=255 if dist>=tol else 0
    cpx[xx,yy]=v
  if d['chroma_invert']:chroma=ImageChops.invert(chroma)
  mask=ImageChops.multiply(alpha,chroma) if d['chroma_mode']=='alpha_and_chroma' else chroma
 else:
  mask={'alpha':alpha,'disc':disc,'combined':ImageChops.multiply(alpha,disc),'full':full}[d['mask']]
 if d['feather']:mask=mask.filter(ImageFilter.GaussianBlur(d['feather']))
 raw=bytearray()
 for rr,g,b in rgb.getdata():v=((rr>>3)<<11)|((g>>2)<<5)|(b>>3);raw+=bytes((v&255,v>>8))
 mb=bytearray((w*h+7)//8);i=0
 for a in mask.getdata():
  if a>=d['threshold']:mb[i>>3]|=0x80>>(i&7)
  i+=1
 asset=b'MVCA'+struct.pack('<IHH',1,w,h)+bytes(raw)+bytes(mb)
 binary=mask.point(lambda a:255 if a>=d['threshold'] else 0);run=rgb.convert('RGBA');run.putalpha(binary)
 return {'asset':asset,'sha':hashlib.sha256(asset).hexdigest(),'prepared':can,'mask':mask,'binary':binary,'runtime':run,'recipe':d}
