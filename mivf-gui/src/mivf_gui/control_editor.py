import copy
from pathlib import Path
from PIL import Image as PILImage
from PySide6.QtCore import Qt,QObject,QEvent,QTimer
from PySide6.QtGui import QPixmap,QColor,QShortcut,QKeySequence
from PySide6.QtWidgets import *
from .control_recipe import state,render,SPECS,norm
from .asset_pipeline import pil_to_qpixmap
LABEL={'rewind':'Rewind','play_pause':'Play/Pause','fast_forward':'Fast Forward','movie_menu_back':'Movie-menu Back'};LEG={'rewind':'rewind_underlay','play_pause':'play_pause_underlay','fast_forward':'fast_forward_underlay','movie_menu_back':'movie_menu_back'}
MASK_ITEMS_R={'alpha':'Source alpha','full':'Full rectangle','rounded_rect':'Rounded rectangle','chroma_key':'Chroma key'}
MASK_ITEMS_D={'alpha':'Source alpha','disc':'Runtime disc','combined':'Combined','full':'Full rectangle','rounded_rect':'Rounded rectangle','chroma_key':'Chroma key'}
DROP_EXTS=('.png','.jpg','.jpeg','.webp','.bmp','.gif')
COALESCE_MS=700
class _DropFilter(QObject):
 def __init__(self,cb,parent=None):super().__init__(parent);self.cb=cb
 def eventFilter(self,obj,event):
  if event.type()==QEvent.DragEnter or event.type()==QEvent.DragMove:
   if event.mimeData().hasUrls():event.acceptProposedAction();return True
   return False
  if event.type()==QEvent.Drop:
   self.cb(event.mimeData().urls());event.acceptProposedAction();return True
  return False
class ControlArtworkDialog(QDialog):
 def __init__(self,project,parent=None):
  super().__init__(parent);self.p=project;self.work=copy.deepcopy(project.artwork.control_edits or {});self.loading=False;self.tint=QColor(255,255,255);self.chroma=QColor(0,255,0)
  self.undo_stack=[];self.redo_stack=[];self._coalescing=False;self._coalesce_timer=QTimer(self);self._coalesce_timer.setSingleShot(True);self._coalesce_timer.timeout.connect(self._end_coalesce)
  self._eyedrop_active=False;self._last_rendered=None
  self.setWindowTitle('Control Artwork Studio');self.resize(950,760);self.setAcceptDrops(True);root=QHBoxLayout(self)
  self.ctrl=QComboBox();[self.ctrl.addItem(v,k) for k,v in LABEL.items()];self.st=QComboBox();self.st.addItems(['Idle','Focused']);left=QVBoxLayout();left.addWidget(self.ctrl);left.addWidget(self.st);self.view=QLabel();self.view.setMinimumSize(420,360);self.view.setAlignment(Qt.AlignCenter);self.view.setStyleSheet('background:#111;color:#aaa');left.addWidget(self.view);self.info=QLabel();self.info.setWordWrap(True);left.addWidget(self.info)
  maskrow=QHBoxLayout();self.mask_authoring_view=QLabel();self.mask_authoring_view.setFixedSize(120,90);self.mask_authoring_view.setAlignment(Qt.AlignCenter);self.mask_authoring_view.setStyleSheet('background:#222;color:#888');self.mask_final_view=QLabel();self.mask_final_view.setFixedSize(120,90);self.mask_final_view.setAlignment(Qt.AlignCenter);self.mask_final_view.setStyleSheet('background:#222;color:#888')
  ac=QVBoxLayout();ac.addWidget(QLabel('Authoring mask'));ac.addWidget(self.mask_authoring_view);fc=QVBoxLayout();fc.addWidget(QLabel('Final 1-bit mask'));fc.addWidget(self.mask_final_view);maskrow.addLayout(ac);maskrow.addLayout(fc);left.addLayout(maskrow)
  undorow=QHBoxLayout();self.undo_btn=QPushButton('Undo');self.undo_btn.clicked.connect(self.undo);self.redo_btn=QPushButton('Redo');self.redo_btn.clicked.connect(self.redo);undorow.addWidget(self.undo_btn);undorow.addWidget(self.redo_btn);left.addLayout(undorow)
  copyrow=QHBoxLayout();b1=QPushButton('Copy Idle → Focused');b1.clicked.connect(lambda:self._copy_within('idle_to_focused'));b2=QPushButton('Copy Focused → Idle');b2.clicked.connect(lambda:self._copy_within('focused_to_idle'));b3=QPushButton('Mirror to…');b3.clicked.connect(self._mirror_to);copyrow.addWidget(b1);copyrow.addWidget(b2);copyrow.addWidget(b3);left.addLayout(copyrow)
  root.addLayout(left,3)
  f=QFormLayout();root.addLayout(f,2)
  self.inh_geom=QCheckBox('Source and geometry');self.inh_mask=QCheckBox('Mask settings');self.inh_adjust=QCheckBox('Image adjustments');inhbox=QVBoxLayout();inhbox.addWidget(QLabel('Focused inherits:'));inhbox.addWidget(self.inh_geom);inhbox.addWidget(self.inh_mask);inhbox.addWidget(self.inh_adjust);self.inh_widget=QWidget();self.inh_widget.setLayout(inhbox);f.addRow(self.inh_widget)
  self.src=QLineEdit();self.src.setPlaceholderText('Path, or drop an image here');b=QPushButton('Browse');b.clicked.connect(self.pick);q=QHBoxLayout();q.addWidget(self.src);q.addWidget(b);w=QWidget();w.setLayout(q);f.addRow('Source',w);self.fit=QComboBox();self.fit.addItems(['contain','cover','center_crop','stretch']);f.addRow('Fit',self.fit)
  self.scale=QDoubleSpinBox();self.scale.setRange(.25,4);self.scale.setSingleStep(.05);f.addRow('Scale',self.scale);self.x=QDoubleSpinBox();self.x.setRange(-1,1);self.x.setSingleStep(.05);f.addRow('X position',self.x);self.y=QDoubleSpinBox();self.y.setRange(-1,1);self.y.setSingleStep(.05);f.addRow('Y position',self.y)
  self.mask=QComboBox();f.addRow('Mask',self.mask);self.threshold=QSpinBox();self.threshold.setRange(1,255);f.addRow('Threshold',self.threshold);self.feather=QDoubleSpinBox();self.feather.setRange(0,4);f.addRow('Feather source',self.feather)
  self.corner_radius=QDoubleSpinBox();self.corner_radius.setRange(0,200);self.corner_radius_row=self._row(f,'Corner radius',self.corner_radius);self.mask_inset=QDoubleSpinBox();self.mask_inset.setRange(0,100);self.mask_inset_row=self._row(f,'Inset',self.mask_inset)
  self.chroma_btn=QPushButton();self.chroma_btn.setFixedWidth(60);self.chroma_btn.clicked.connect(self.pick_chroma);self.eyedrop_btn=QPushButton('Eyedropper');self.eyedrop_btn.clicked.connect(self.start_eyedrop);chromacolrow=QHBoxLayout();chromacolrow.addWidget(self.chroma_btn);chromacolrow.addWidget(self.eyedrop_btn);chromacolw=QWidget();chromacolw.setLayout(chromacolrow);self.chroma_color_row=self._row(f,'Key color',chromacolw)
  self.chroma_tolerance=QDoubleSpinBox();self.chroma_tolerance.setRange(0,441);self.chroma_tolerance_row=self._row(f,'Tolerance',self.chroma_tolerance)
  self.chroma_softness=QDoubleSpinBox();self.chroma_softness.setRange(0,220);self.chroma_softness_row=self._row(f,'Edge softness',self.chroma_softness)
  self.chroma_invert=QCheckBox('Invert');self.chroma_invert_row=self._row(f,'',self.chroma_invert)
  self.chroma_mode=QComboBox();self.chroma_mode.addItem('Chroma Key Only','chroma_only');self.chroma_mode.addItem('Source Alpha × Chroma Key','alpha_and_chroma');self.chroma_mode_row=self._row(f,'Combine',self.chroma_mode)
  self.bright=QDoubleSpinBox();self.bright.setRange(-1,1);f.addRow('Brightness',self.bright);self.contrast=QDoubleSpinBox();self.contrast.setRange(-1,1);f.addRow('Contrast',self.contrast);self.sat=QDoubleSpinBox();self.sat.setRange(0,2);f.addRow('Saturation',self.sat)
  self.tint_btn=QPushButton();self.tint_btn.setFixedWidth(60);self.tint_btn.clicked.connect(self.pick_tint);self.tint_strength=QDoubleSpinBox();self.tint_strength.setRange(0,1);self.tint_strength.setSingleStep(.05);tint_reset=QPushButton('Reset');tint_reset.clicked.connect(self.reset_tint);tintrow=QHBoxLayout();tintrow.addWidget(self.tint_btn);tintrow.addWidget(self.tint_strength);tintrow.addWidget(tint_reset);tintw=QWidget();tintw.setLayout(tintrow);f.addRow('Tint',tintw)
  buttons=QDialogButtonBox(QDialogButtonBox.Apply|QDialogButtonBox.Cancel);buttons.button(QDialogButtonBox.Apply).clicked.connect(self.apply);buttons.rejected.connect(self.reject);f.addRow(buttons)
  self.ctrl.currentIndexChanged.connect(self.load);self.st.currentIndexChanged.connect(self.load)
  self.mask.currentIndexChanged.connect(self._mask_mode_changed)
  for cb in (self.inh_geom,self.inh_mask,self.inh_adjust):cb.toggled.connect(self._inherit_toggled)
  self.chroma_invert.toggled.connect(self._forced_change)
  for z in (self.src,self.fit,self.scale,self.x,self.y,self.threshold,self.feather,self.corner_radius,self.mask_inset,self.chroma_tolerance,self.chroma_softness,self.chroma_mode,self.bright,self.contrast,self.sat,self.tint_strength):(z.currentIndexChanged if isinstance(z,QComboBox) else z.valueChanged if hasattr(z,'valueChanged') else z.editingFinished).connect(self.change)
  self._drop_filter=_DropFilter(self._handle_drop,self);self.view.setAcceptDrops(True);self.view.installEventFilter(self._drop_filter);self.src.setAcceptDrops(True);self.src.installEventFilter(self._drop_filter)
  self.view.mousePressEvent=self._on_view_click
  QShortcut(QKeySequence('Ctrl+Z'),self).activated.connect(self.undo);QShortcut(QKeySequence('Ctrl+Y'),self).activated.connect(self.redo)
  self.load()
 def _row(self,form,label,widget):
  form.addRow(label,widget);return widget
 def c(self):return self.ctrl.currentData()
 def sn(self):return 'focused' if self.st.currentIndex() else 'idle'
 def legacy(self):k=LEG[self.c()];return getattr(self.p.artwork,k),self.p.artwork.fit_modes.get(k,'contain')
 def recipe(self):a,b=self.legacy();return state(self.work,self.c(),self.sn(),a,b)
 def _set_tint_swatch(self):self.tint_btn.setStyleSheet(f'background:{self.tint.name()}')
 def _set_chroma_swatch(self):self.chroma_btn.setStyleSheet(f'background:{self.chroma.name()}')
 # ---- undo/redo ----
 def _push_undo(self,force=False):
  if self._coalescing and not force:self._coalesce_timer.start(COALESCE_MS);return
  self.undo_stack.append(copy.deepcopy(self.work));self.redo_stack.clear();self._coalescing=True;self._coalesce_timer.start(COALESCE_MS)
 def _end_coalesce(self):self._coalescing=False
 def undo(self):
  if not self.undo_stack:return
  self.redo_stack.append(copy.deepcopy(self.work));self.work=self.undo_stack.pop();self._coalesce_timer.stop();self._end_coalesce();self.load()
 def redo(self):
  if not self.redo_stack:return
  self.undo_stack.append(copy.deepcopy(self.work));self.work=self.redo_stack.pop();self._coalesce_timer.stop();self._end_coalesce();self.load()
 def _inherit_toggled(self,*_):
  if self.loading:return
  self._push_undo(force=True);self.change()
 def _forced_change(self,*_):
  if self.loading:return
  self._push_undo(force=True);self.change()
 # ---- load/change/preview ----
 def load(self,*_):
  self.loading=True;r=self.recipe();raw=self.work.get(self.c(),{}).get(self.sn(),{});is_focused=self.sn()=='focused';self.inh_widget.setVisible(is_focused)
  legacy=raw.get('inherit_idle',True);self.inh_geom.setChecked(raw.get('inherit_source_geometry',legacy));self.inh_mask.setChecked(raw.get('inherit_mask',legacy));self.inh_adjust.setChecked(raw.get('inherit_adjustments',legacy))
  self.src.setText(r.get('source') or '');self.fit.setCurrentText(r['fit']);self.scale.setValue(r['scale']);self.x.setValue(r['x']);self.y.setValue(r['y'])
  self.mask.blockSignals(True);self.mask.clear();items=MASK_ITEMS_D if SPECS[self.c()][2] is not None else MASK_ITEMS_R;[self.mask.addItem(v,k) for k,v in items.items()];self.mask.setCurrentIndex(max(0,self.mask.findData(r['mask'])));self.mask.blockSignals(False)
  self.threshold.setValue(r['threshold']);self.feather.setValue(r['feather']);self.corner_radius.setValue(r['corner_radius']);self.mask_inset.setValue(r['mask_inset'])
  self.chroma=QColor(*r['chroma_key']);self._set_chroma_swatch();self.chroma_tolerance.setValue(r['chroma_tolerance']);self.chroma_softness.setValue(r['chroma_edge_softness']);self.chroma_invert.setChecked(r['chroma_invert']);self.chroma_mode.setCurrentIndex(max(0,self.chroma_mode.findData(r['chroma_mode'])))
  self.bright.setValue(r['brightness']);self.contrast.setValue(r['contrast']);self.sat.setValue(r['saturation'])
  self.tint=QColor(*r['tint']);self._set_tint_swatch();self.tint_strength.setValue(r['tint_strength'])
  self._update_mask_field_visibility(r['mask']);self.undo_btn.setEnabled(bool(self.undo_stack));self.redo_btn.setEnabled(bool(self.redo_stack))
  self.loading=False;self.preview()
 def _mask_mode_changed(self,*_):
  if self.loading:return
  self._push_undo(force=True);self._update_mask_field_visibility(self.mask.currentData());self.change()
 def _update_mask_field_visibility(self,mode):
  self.corner_radius_row.setVisible(mode=='rounded_rect');self.mask_inset_row.setVisible(mode=='rounded_rect')
  for w in (self.chroma_color_row,self.chroma_tolerance_row,self.chroma_softness_row,self.chroma_invert_row,self.chroma_mode_row):w.setVisible(mode=='chroma_key')
 def change(self,*_):
  if self.loading:return
  self._push_undo()
  d=self.work.setdefault(self.c(),{}).setdefault(self.sn(),{});d.update(source=self.src.text() or None,fit=self.fit.currentText(),scale=self.scale.value(),x=self.x.value(),y=self.y.value(),mask=self.mask.currentData(),threshold=self.threshold.value(),feather=self.feather.value(),corner_radius=self.corner_radius.value(),mask_inset=self.mask_inset.value(),chroma_key=[self.chroma.red(),self.chroma.green(),self.chroma.blue()],chroma_tolerance=self.chroma_tolerance.value(),chroma_edge_softness=self.chroma_softness.value(),chroma_invert=self.chroma_invert.isChecked(),chroma_mode=self.chroma_mode.currentData(),brightness=self.bright.value(),contrast=self.contrast.value(),saturation=self.sat.value(),tint=[self.tint.red(),self.tint.green(),self.tint.blue()],tint_strength=self.tint_strength.value())
  if self.sn()=='focused':d['inherit_source_geometry']=self.inh_geom.isChecked();d['inherit_mask']=self.inh_mask.isChecked();d['inherit_adjustments']=self.inh_adjust.isChecked();d.pop('inherit_idle',None)
  self.preview()
 def preview(self):
  self.undo_btn.setEnabled(bool(self.undo_stack));self.redo_btn.setEnabled(bool(self.redo_stack))
  try:
   r=render(self.c(),self.recipe(),self.p.project_path.parent if self.p.project_path else None)
   self._last_rendered=r['prepared']
   self.view.setPixmap(pil_to_qpixmap(r['runtime']).scaled(420,360,Qt.KeepAspectRatio,Qt.FastTransformation))
   self.mask_authoring_view.setPixmap(pil_to_qpixmap(r['mask'].convert('RGB')).scaled(120,90,Qt.KeepAspectRatio,Qt.FastTransformation))
   self.mask_final_view.setPixmap(pil_to_qpixmap(r['binary'].convert('RGB')).scaled(120,90,Qt.KeepAspectRatio,Qt.FastTransformation))
   self.info.setText(f"Runtime: {len(r['asset'])} bytes; SHA-256 {r['sha'][:20]}…; mask is final 1-bit")
  except Exception as e:
   self.view.setPixmap(QPixmap());self.view.setText('No preview');self.info.setText(str(e))
   self.mask_authoring_view.setPixmap(QPixmap());self.mask_final_view.setPixmap(QPixmap());self._last_rendered=None
 # ---- source assignment ----
 def pick(self):
  p,_=QFileDialog.getOpenFileName(self,'Artwork','','Images (*.png *.jpg *.jpeg *.webp)')
  if p:self._push_undo(force=True);self.src.setText(p);self.change()
 def _handle_drop(self,urls):
  if len(urls)!=1:
   QMessageBox.warning(self,'Drop rejected','Drop exactly one image file at a time.');return
  path=urls[0].toLocalFile()
  if not path:return
  if Path(path).suffix.lower() not in DROP_EXTS:
   QMessageBox.warning(self,'Unsupported file',f'{path} is not a supported image type.');return
  try:
   with PILImage.open(path) as im:im.verify()
  except Exception as e:
   QMessageBox.warning(self,'Cannot read image',f'{path}\n\n{e}');return
  self._push_undo(force=True);self.src.setText(path);self.change()
 def dragEnterEvent(self,event):
  if event.mimeData().hasUrls():event.acceptProposedAction()
 def dropEvent(self,event):self._handle_drop(event.mimeData().urls());event.acceptProposedAction()
 # ---- tint / chroma color pickers ----
 def pick_tint(self):
  col=QColorDialog.getColor(self.tint,self,'Tint color')
  if col.isValid():self._push_undo(force=True);self.tint=col;self._set_tint_swatch();self.change()
 def reset_tint(self):self._push_undo(force=True);self.tint=QColor(255,255,255);self._set_tint_swatch();self.tint_strength.setValue(0);self.change()
 def pick_chroma(self):
  col=QColorDialog.getColor(self.chroma,self,'Chroma key color')
  if col.isValid():self._push_undo(force=True);self.chroma=col;self._set_chroma_swatch();self.change()
 def start_eyedrop(self):self._eyedrop_active=True;self.info.setText('Click the preview to sample the key color…')
 def _on_view_click(self,event):
  if not self._eyedrop_active or self._last_rendered is None:return
  pm=self.view.pixmap()
  if pm is None or pm.isNull():return
  pos=event.position() if hasattr(event,'position') else event.pos()
  ex,ey=(pos.x(),pos.y()) if hasattr(pos,'x') else pos
  lbl_w,lbl_h=self.view.width(),self.view.height();pm_w,pm_h=pm.width(),pm.height()
  off_x=(lbl_w-pm_w)//2;off_y=(lbl_h-pm_h)//2;px=ex-off_x;py=ey-off_y
  if px<0 or py<0 or px>=pm_w or py>=pm_h:return
  src_w,src_h=self._last_rendered.size;ix=min(max(int(px*src_w/pm_w),0),src_w-1);iy=min(max(int(py*src_h/pm_h),0),src_h-1)
  r,g,b=self._last_rendered.convert('RGB').getpixel((ix,iy))
  self._push_undo(force=True);self.chroma=QColor(r,g,b);self._set_chroma_swatch();self._eyedrop_active=False;self.change()
 # ---- copy / mirror ----
 def _copy_within(self,direction):
  self._push_undo(force=True)
  src_name,dst_name=('idle','focused') if direction=='idle_to_focused' else ('focused','idle')
  src_recipe=state(self.work,self.c(),src_name,*self.legacy())
  dst=self.work.setdefault(self.c(),{}).setdefault(dst_name,{});dst.clear();dst.update(src_recipe)
  if dst_name=='focused':dst['inherit_source_geometry']=False;dst['inherit_mask']=False;dst['inherit_adjustments']=False
  if self.sn()!=dst_name:self.st.setCurrentIndex(1 if dst_name=='focused' else 0)
  else:self.load()
 def _mirror_to(self):
  others=[k for k in LABEL if k!=self.c()]
  choice,ok=QInputDialog.getItem(self,'Mirror recipe to','Copy this state to control:',[LABEL[k] for k in others],0,False)
  if not ok:return
  target=next(k for k in others if LABEL[k]==choice)
  if SPECS[target][:2]!=SPECS[self.c()][:2]:
   reply=QMessageBox.warning(self,'Different geometry',f'{LABEL[target]} has different dimensions than {LABEL[self.c()]} ({SPECS[target][0]}x{SPECS[target][1]} vs {SPECS[self.c()][0]}x{SPECS[self.c()][1]}). Source position, scale, and mask radius may not translate correctly. Continue?',QMessageBox.Yes|QMessageBox.No)
   if reply!=QMessageBox.Yes:return
  self._push_undo(force=True)
  src_recipe=self.recipe();dst_name=self.sn()
  dst=self.work.setdefault(target,{}).setdefault(dst_name,{});dst.clear();dst.update(src_recipe)
  dst['mask']=norm({'mask':src_recipe['mask']},target)['mask']  # re-clamp: source control's mask mode may be invalid for target (e.g. disc on Back)
  if dst_name=='focused':dst['inherit_source_geometry']=False;dst['inherit_mask']=False;dst['inherit_adjustments']=False
 def apply(self):self.p.artwork.control_edits=copy.deepcopy(self.work);self.accept()
