from pathlib import Path
from .control_recipe import state,render,SPECS
LEG={'rewind':'rewind_underlay','play_pause':'play_pause_underlay','fast_forward':'fast_forward_underlay','movie_menu_back':'movie_menu_back'}
def build_state_assets(project,project_path,stage,basename):
 """Renders every control's Idle/Focused state (the one real render path
 shared by the exporter and PackagePlan -- see theme_plan.py). `details`
 carries per-(control,state) metadata for ALL eight combos, including ones
 that deduped to an earlier file's bytes (`is_new=False`), so a plan can be
 built without a second, separate render pass.

 A failure rendering ONE (control,state) -- e.g. a missing source image --
 is caught per-entry rather than aborting the whole batch: `errors` carries
 that key's failure message, `names`/`details` simply omit it. This lets a
 Preflight/PackagePlan report every problem across all controls in one
 pass instead of stopping at the first one (the real exporter still refuses
 to promote when any such error exists -- see theme_plan.py)."""
 art=project.get('artwork',{});edits=art.get('control_edits',{});fits=art.get('fit_modes',{});names={};files=[];seen={};details={};errors={}
 for c,k in LEG.items():
  w,h,_=SPECS[c]
  for st in ('idle','focused'):
   key=c+'_'+st
   try:
    recipe=state(edits,c,st,art.get(k),fits.get(k,'contain'))
    if not recipe.get('source'):
     errors[key]='Missing artwork: no source image assigned';continue
    r=render(c,recipe,project_path.parent)
   except Exception as e:
    errors[key]=str(e);continue
   is_new=r['sha'] not in seen
   if not is_new:name=seen[r['sha']]
   else:name=f'{basename}.{c}.{st}.mivfasset';(stage/name).write_bytes(r['asset']);seen[r['sha']]=name;files.append((key,stage/name))
   names[key]=name
   details[key]={'sha':r['sha'],'size':len(r['asset']),'w':w,'h':h,'recipe':recipe,'is_new':is_new,'filename':name,'control':c,'state':st}
 return names,files,details,errors
