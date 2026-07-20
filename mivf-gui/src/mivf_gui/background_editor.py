from __future__ import annotations
import copy
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPixmap
from PySide6.QtWidgets import QDialog,QDialogButtonBox,QDoubleSpinBox,QFormLayout,QHBoxLayout,QLabel,QPushButton,QColorDialog,QVBoxLayout,QWidget
from . import background_recipe
from .asset_pipeline import pil_to_qpixmap
class BackgroundRecipeDialog(QDialog):
 def __init__(self,project,accent,parent=None):
  super().__init__(parent);self.project=project;self.accent=accent;self.work=background_recipe.normalize(copy.deepcopy(project.artwork.dashboard_bg_recipe));self.loading=True
  self.setWindowTitle('Background Readability');self.resize(720,620);root=QHBoxLayout(self);left=QVBoxLayout();self.preview=QLabel();self.preview.setMinimumSize(360,270);self.preview.setAlignment(Qt.AlignCenter);self.preview.setStyleSheet('background:#111;color:#aaa');left.addWidget(self.preview);self.info=QLabel();self.info.setWordWrap(True);left.addWidget(self.info);root.addLayout(left,3);form=QFormLayout();root.addLayout(form,2)
  self.blur=self._spin(0,12,.25);self.darken=self._spin(0,1,.05);self.brightness=self._spin(-1,1,.05);self.contrast=self._spin(-1,1,.05);self.saturation=self._spin(0,2,.05);self.vignette=self._spin(0,1,.05);self.x=self._spin(-1,1,.05);self.y=self._spin(-1,1,.05);self.tint_strength=self._spin(0,1,.05);self.tint=QColor(*self.work['tint']);self.tint_btn=QPushButton();self.tint_btn.clicked.connect(self._pick_tint);self._swatch()
  for label,w in [('Blur',self.blur),('Darken overlay',self.darken),('Brightness',self.brightness),('Contrast',self.contrast),('Saturation',self.saturation),('Vignette',self.vignette),('Focal X',self.x),('Focal Y',self.y)]:form.addRow(label,w)
  tw=QWidget();tl=QHBoxLayout(tw);tl.setContentsMargins(0,0,0,0);tl.addWidget(self.tint_btn);tl.addWidget(self.tint_strength);form.addRow('Tint',tw)
  reset=QPushButton('Reset readability');reset.clicked.connect(self._reset);form.addRow('',reset);buttons=QDialogButtonBox(QDialogButtonBox.Apply|QDialogButtonBox.Cancel);buttons.button(QDialogButtonBox.Apply).clicked.connect(self._apply);buttons.rejected.connect(self.reject);form.addRow(buttons)
  values={'blur':self.blur,'darken':self.darken,'brightness':self.brightness,'contrast':self.contrast,'saturation':self.saturation,'vignette':self.vignette,'x':self.x,'y':self.y,'tint_strength':self.tint_strength}
  for k,w in values.items():w.setValue(self.work[k]);w.valueChanged.connect(self._changed)
  self.loading=False;self._render()
 def _spin(self,a,b,step):w=QDoubleSpinBox();w.setRange(a,b);w.setSingleStep(step);w.setDecimals(2);return w
 def _swatch(self):self.tint_btn.setText(self.tint.name());self.tint_btn.setStyleSheet(f'background:{self.tint.name()}')
 def _pick_tint(self):
  c=QColorDialog.getColor(self.tint,self,'Background tint')
  if c.isValid():self.tint=c;self._swatch();self._changed()
 def _changed(self,*_):
  if self.loading:return
  self.work.update(blur=self.blur.value(),darken=self.darken.value(),brightness=self.brightness.value(),contrast=self.contrast.value(),saturation=self.saturation.value(),vignette=self.vignette.value(),x=self.x.value(),y=self.y.value(),tint=[self.tint.red(),self.tint.green(),self.tint.blue()],tint_strength=self.tint_strength.value());self._render()
 def _reset(self):
  self.loading=True
  for w,v in ((self.blur,0),(self.darken,0),(self.brightness,0),(self.contrast,0),(self.saturation,1),(self.vignette,0),(self.x,0),(self.y,0),(self.tint_strength,0)):w.setValue(v)
  self.tint=QColor(255,255,255);self._swatch();self.loading=False;self._changed()
 def _render(self):
  try:
   mode=background_recipe.effective_mode(self.project.artwork);source=self.project.artwork.dashboard_bg;base=self.project.project_path.parent if self.project.project_path else None;r=background_recipe.render(mode,source,self.work,self.accent,base);self.preview.setPixmap(pil_to_qpixmap(r['runtime']).scaled(360,270,Qt.KeepAspectRatio,Qt.FastTransformation));self.info.setText(f"Exact RGB565 preview - {len(r['asset'])} bytes - SHA-256 {r['sha'][:20]}...")
  except Exception as e:self.preview.setPixmap(QPixmap());self.preview.setText('No preview');self.info.setText(str(e))
 def _apply(self):self.project.artwork.dashboard_bg_recipe=copy.deepcopy(self.work);self.accept()
