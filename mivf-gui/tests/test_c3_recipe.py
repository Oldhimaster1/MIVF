import sys
from pathlib import Path
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'src'))
from mivf_gui.control_recipe import *
def test_recipe_deterministic(tmp_path):
 p=tmp_path/'x.png';Image.new('RGBA',(20,20),(200,40,80,255)).save(p);a=render('rewind',{'source':str(p)});b=render('rewind',{'source':str(p)});assert a['asset']==b['asset'];assert len(a['asset'])==8172
def test_focused_inherit_and_clamp():
 # Phase C.4A changed inheritance from one all-or-nothing `inherit_idle`
 # flag to three independent groups (geometry/mask/adjustments). Under the
 # OLD code, `inherit_idle=True` still let any field literally present in
 # the focused dict override Idle -- so a lone hand-set `brightness` won
 # even while "inheriting". Under the NEW grouped design this is
 # intentionally no longer true: checking a group's inherit box means that
 # group ALWAYS tracks Idle live, so a stray `brightness` key with no
 # matching `inherit_adjustments: False` is ignored, not applied.
 r=state({'rewind':{'focused':{'inherit_idle':True,'brightness':.2}}},'rewind','focused','x.png','contain')
 assert r['source']=='x.png' and r['brightness']==0.0
 assert norm({'scale':99},'rewind')['scale']==4
def test_back_rejects_disc():assert norm({'mask':'disc'},'movie_menu_back')['mask']=='alpha'
def test_legacy_never_touched_focused_matches_idle_exactly():
 # Realistic old-project case: Focused was never opened/edited, so its
 # dict is empty and the legacy `inherit_idle` default (True) applies.
 # Both before and after C.4A, this must render pixel-identical to Idle.
 idle=state({'rewind':{'idle':{'source':'x.png','brightness':.3}}},'rewind','idle','x.png','contain')
 foc=state({'rewind':{'idle':{'source':'x.png','brightness':.3},'focused':{}}},'rewind','focused','x.png','contain')
 assert foc==idle
def test_legacy_fully_independent_focused_all_fields_present():
 # Realistic old-GUI-usage case: the real editor's change() handler always
 # writes every widget value at once, so a "no inherit" Focused dict is
 # never partial -- it has every field set. With inherit_idle=False this
 # must be fully independent of Idle both before and after C.4A.
 full={'inherit_idle':False,'source':'y.png','fit':'cover','scale':2.0,'x':0.1,'y':-0.1,
       'mask':'alpha','threshold':100,'feather':1.0,'brightness':0.3,'contrast':0.1,
       'saturation':1.2,'tint':[200,200,255],'tint_strength':0.4}
 r=state({'rewind':{'idle':{'source':'x.png'},'focused':full}},'rewind','focused','x.png','contain')
 assert r['source']=='y.png' and r['scale']==2.0 and r['brightness']==0.3 and r['saturation']==1.2
def test_grouped_inheritance_partial_independence():
 # New Phase C.4A behavior: geometry can inherit Idle live while
 # adjustments are independently overridden, matching the three-checkbox
 # Studio UI (Source/geometry, Mask, Image adjustments).
 edits={'rewind':{'idle':{'source':'idle.png','scale':1.5,'brightness':0.0},
                  'focused':{'inherit_source_geometry':True,'inherit_mask':True,
                             'inherit_adjustments':False,'brightness':0.5,'source':'ignored.png'}}}
 r=state(edits,'rewind','focused','x.png','contain')
 assert r['source']=='idle.png' and r['scale']==1.5  # geometry inherited live, ignoring focused's own source/scale
 assert r['brightness']==0.5  # adjustments independent

def test_rounded_rect_mask_is_hard_1bit_and_respects_radius_and_inset(tmp_path):
 # Phase C.4B: rounded-rectangle mask, most useful for the 222x20 Back
 # control. The authoring mask may be anti-aliased at the curve, but the
 # runtime asset's mask bytes must still be pure hard 1-bit (this is
 # already guaranteed generically by render()'s threshold step -- verify
 # it actually holds for this new mode specifically).
 p=tmp_path/'x.png';Image.new('RGBA',(222,20),(255,255,255,255)).save(p)
 r=render('movie_menu_back',{'source':str(p),'mask':'rounded_rect','corner_radius':6,'mask_inset':2})
 assert len(r['asset'])==9447
 for byte in r['binary'].tobytes():assert byte in (0,255)
 # corner pixels of a rounded rect with real radius/inset must be masked out (0)
 assert r['binary'].getpixel((0,0))==0
 # center must be opaque (255)
 assert r['binary'].getpixel((111,10))==255
def test_rounded_rect_zero_radius_is_a_plain_inset_rectangle(tmp_path):
 p=tmp_path/'x.png';Image.new('RGBA',(222,20),(255,255,255,255)).save(p)
 r=render('movie_menu_back',{'source':str(p),'mask':'rounded_rect','corner_radius':0,'mask_inset':0})
 assert r['binary'].getpixel((0,0))==255  # no radius, no inset -> full rectangle, corners included
def test_chroma_key_keys_out_matching_color_and_keeps_distinct_color(tmp_path):
 p=tmp_path/'x.png';Image.new('RGBA',(64,60),(0,255,0,255)).save(p)  # solid pure green
 keyed=render('rewind',{'source':str(p),'mask':'chroma_key','chroma_key':[0,255,0],'chroma_tolerance':40})
 assert keyed['binary'].getpixel((10,10))==0  # matches key color closely -> transparent
 far=render('rewind',{'source':str(p),'mask':'chroma_key','chroma_key':[255,0,0],'chroma_tolerance':40})
 assert far['binary'].getpixel((10,10))==255  # far from key color -> opaque
def test_chroma_key_invert_flips_result(tmp_path):
 p=tmp_path/'x.png';Image.new('RGBA',(64,60),(0,255,0,255)).save(p)
 normal=render('rewind',{'source':str(p),'mask':'chroma_key','chroma_key':[0,255,0],'chroma_tolerance':40,'chroma_invert':False})
 inverted=render('rewind',{'source':str(p),'mask':'chroma_key','chroma_key':[0,255,0],'chroma_tolerance':40,'chroma_invert':True})
 assert normal['binary'].getpixel((10,10))==0 and inverted['binary'].getpixel((10,10))==255
def test_chroma_key_alpha_and_chroma_mode_intersects_source_alpha(tmp_path):
 # Source has a transparent half; chroma_only would still mark the far-color
 # half opaque, but alpha_and_chroma must also respect the source's own
 # alpha channel (transparent stays transparent even if not the key color).
 im=Image.new('RGBA',(64,60),(255,0,0,255));px=im.load()
 for yy in range(60):
  for xx in range(32):px[xx,yy]=(255,0,0,0)  # left half fully transparent
 p=tmp_path/'x.png';im.save(p)
 r=render('rewind',{'source':str(p),'mask':'chroma_key','chroma_key':[0,255,0],'chroma_tolerance':40,'chroma_mode':'alpha_and_chroma'})
 assert r['binary'].getpixel((5,10))==0  # transparent source half stays masked out
 assert r['binary'].getpixel((50,10))==255  # opaque, far-from-key half stays opaque
def test_mask_clamp_out_of_range_values():
 n=norm({'corner_radius':9999,'mask_inset':9999,'chroma_tolerance':9999,'chroma_edge_softness':9999,'chroma_key':[999,-50,10],'chroma_mode':'bogus'},'rewind')
 assert 0<=n['corner_radius']<=30 and 0<=n['mask_inset']<=15
 assert n['chroma_tolerance']==441 and n['chroma_edge_softness']==220
 assert n['chroma_key']==[255,0,10]
 assert n['chroma_mode']=='chroma_only'
def test_existing_mask_modes_unaffected_by_new_mask_fields():
 # Back-compat: alpha/disc/combined/full must render identically to before
 # C.4B even though norm() now injects new default fields alongside them.
 for control,mask in (('rewind','alpha'),('rewind','disc'),('rewind','combined'),('movie_menu_back','full')):
  assert norm({'mask':mask},control)['mask']==mask
