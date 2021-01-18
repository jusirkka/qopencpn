#include "chartmode.h"
#include "orthocam.h"
#include "outlinemode.h"
#include "chartpainter.h"

ChartMode::ChartMode(float wmm, float hmm, GeoProjection* p)
  : DetailMode()
{
  m_camera = new OrthoCam(wmm, hmm, p);
  m_drawables << new ChartPainter(this);
}

ChartMode::~ChartMode() {
  delete m_camera;
  qDeleteAll(m_drawables);
}

DetailMode* ChartMode::smallerScaleMode() const {
  return nullptr;
}

DetailMode* ChartMode::largerScaleMode() const {
  const float hmm = m_camera->heightMM();
  const float wmm = hmm * m_camera->aspect();
  auto p = GeoProjection::CreateProjection(m_camera->geoprojection()->className());
  DetailMode* outlines = new OutlineMode(wmm, hmm, p);
  const quint32 scale = qMin(outlines->camera()->maxScale(), qMax(m_camera->scale(), outlines->camera()->minScale()));
  outlines->camera()->setScale(scale);
  outlines->camera()->reset(m_camera->eye(), m_camera->northAngle());
  return outlines;
}

Camera* ChartMode::cloneCamera() const {
  const float wmm = m_camera->heightMM();
  const float hmm = wmm * m_camera->aspect();
  GeoProjection* p = GeoProjection::CreateProjection(m_camera->geoprojection()->className());
  Camera* cam = new OrthoCam(wmm, hmm, p);
  cam->setScale(m_camera->scale());
  cam->reset(m_camera->eye(), m_camera->northAngle());
  return cam;
}

