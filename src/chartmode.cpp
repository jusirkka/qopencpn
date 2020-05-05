#include "chartmode.h"
#include "orthocam.h"
#include "outlinemode.h"

ChartMode::ChartMode(float wmm, float hmm, GeoProjection* p) {
  m_camera = new OrthoCam(wmm, hmm, p);
}


DetailMode* ChartMode::smallerScaleMode() const {
  return nullptr;
}

DetailMode* ChartMode::largerScaleMode() const {
  const float hmm = m_camera->heightMM();
  const float wmm = hmm * m_camera->aspect();
  auto p = GeoProjection::CreateProjection(m_camera->geoprojection()->className());
  DetailMode* outlines = new OutlineMode(wmm, hmm, p);
  const quint32 scale = qMax(m_camera->scale(), outlines->camera()->minScale());
  outlines->camera()->setScale(scale);
  outlines->camera()->reset(m_camera->eye(), m_camera->northAngle());
  return outlines;
}
