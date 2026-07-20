import json,sys,subprocess
from pathlib import Path
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'src'))
from mivf_gui import background_recipe,theme_plan
from mivf_gui.project import MivfProject

def test_legacy_mode_inference():
 assert background_recipe.effective_mode({'dashboard_bg':'x.png'})=='custom'
 assert background_recipe.effective_mode({'dashboard_bg':None})=='builtin'
def test_builtin_returns_no_asset():assert background_recipe.render('builtin') is None
def test_custom_neutral_matches_legacy_tool(tmp_path):
 src=tmp_path/'bg.png';Image.new('RGB',(400,300),(20,40,80)).save(src)
 r=background_recipe.render('custom',str(src),{'fit':'contain'})
 repo=Path(__file__).resolve().parents[2];out=tmp_path/'tool.mivfasset'
 subprocess.run([sys.executable,str(repo/'tools/mivf_make_dashboard_bg.py'),str(src),str(out),'--force'],check=True,capture_output=True)
 assert r['asset']==out.read_bytes() and len(r['asset'])==153612
def test_generated_is_deterministic_exact_size():
 a=background_recipe.render('generated',accent=(70,120,210));b=background_recipe.render('generated',accent=(70,120,210))
 assert a['asset']==b['asset'] and len(a['asset'])==153612
def test_project_round_trip_background_fields(tmp_path):
 p=MivfProject();p.artwork.dashboard_bg_mode='generated';p.artwork.dashboard_bg_recipe={'style':'foundation_gradient','secondary_rgb':[1,2,3]};f=tmp_path/'p.mivfproj';p.save(f);q=MivfProject.load(f);assert q.artwork.dashboard_bg_mode=='generated' and q.artwork.dashboard_bg_recipe['secondary_rgb']==[1,2,3]
def _proj(tmp,mode,source=None):
 art={'dashboard_bg_mode':mode,'dashboard_bg':source,'rewind_underlay':source,'play_pause_underlay':source,'fast_forward_underlay':source,'movie_menu_back':source}
 p=tmp/'p.mivfproj';p.write_text(json.dumps({'schema':'mivf-toolkit-project-v1','artwork':art,'theme':{'accent_rgb':[70,120,210]}}));return p
def test_builtin_omits_asset_and_manifest_key(tmp_path):
 src=tmp_path/'x.png';Image.new('RGBA',(100,100),(100,120,140,255)).save(src);plan=theme_plan.build_plan(_proj(tmp_path,'builtin',str(src)),tmp_path/'out','x')
 assert plan.ok_to_export and not any(f.role=='dashboard_bg' for f in plan.files) and 'DASHBOARD_BG=' not in plan.manifest_text
def test_generated_plan_has_exact_asset(tmp_path):
 src=tmp_path/'x.png';Image.new('RGBA',(100,100),(100,120,140,255)).save(src);plan=theme_plan.build_plan(_proj(tmp_path,'generated',str(src)),tmp_path/'out','x')
 bg=next(f for f in plan.files if f.role=='dashboard_bg');assert bg.size==153612 and bg.recipe['mode']=='generated'


def test_neutral_readability_preserves_c5b1_bytes(tmp_path):
 src=tmp_path/'bg.png';Image.new('RGB',(400,300),(20,40,80)).save(src)
 a=background_recipe.render('custom',str(src),{'fit':'contain'})
 b=background_recipe.render('custom',str(src),{'fit':'contain','blur':0,'darken':0,'brightness':0,'contrast':0,'saturation':1,'tint_strength':0,'vignette':0,'x':0,'y':0})
 assert a['asset']==b['asset']

def test_readability_adjustments_are_deterministic_and_change_output(tmp_path):
 src=tmp_path/'bg.png';Image.new('RGB',(320,240),(120,160,200)).save(src)
 neutral=background_recipe.render('custom',str(src))
 recipe={'blur':2,'darken':.25,'brightness':.1,'contrast':.2,'saturation':.7,'tint':[255,180,120],'tint_strength':.15,'vignette':.4}
 a=background_recipe.render('custom',str(src),recipe);b=background_recipe.render('custom',str(src),recipe)
 assert a['asset']==b['asset'] and a['asset']!=neutral['asset'] and len(a['asset'])==153612

def test_readability_values_clamp():
 n=background_recipe.normalize({'blur':99,'darken':9,'brightness':-9,'contrast':9,'saturation':9,'tint_strength':9,'vignette':9,'tint':[999,-4,20]})
 assert n['blur']==12 and n['darken']==1 and n['brightness']==-1 and n['contrast']==1 and n['saturation']==2 and n['tint_strength']==1 and n['vignette']==1 and n['tint']==[255,0,20]
