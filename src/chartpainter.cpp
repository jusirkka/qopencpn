#include "chartpainter.h"
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include "s57chart.h"
#include "chartmanager.h"
#include <QGuiApplication>
#include <QScreen>
#include "shader.h"
#include <QOpenGLFramebufferObject>
#include "glcontext.h"
#include <QVector>
#include "orthocam.h"
#include "platform.h"

ChartPainter::ChartPainter(QObject* parent)
  : Drawable(parent)
  , m_manager(ChartManager::instance())
  , m_initialized(false)
  , m_bufSize()
  , m_fbo(nullptr)
  , m_coordBuffer(QOpenGLBuffer::VertexBuffer)
  , m_indexBuffer(QOpenGLBuffer::IndexBuffer)
{}

ChartPainter::~ChartPainter() {
  GL::Context::instance()->makeCurrent();
  delete m_fbo;
}

void ChartPainter::initializeGL() {

  if (!m_initialized) {

    m_initialized = true;

    m_areaShader = GL::AreaShader::instance();
    m_solidShader = GL::SolidLineShader::instance();
    m_dashedShader = GL::DashedLineShader::instance();
    m_textShader = GL::TextShader::instance();
    m_rasterShader = GL::RasterSymbolShader::instance();
    m_vectorShader = GL::VectorSymbolShader::instance();
    m_textureShader = GL::TextureShader::instance();

    const QVector<GLuint> indices {0, 1, 2, 0, 2, 3};

    // upper left
    const QPointF p0(-1., 1.);
    const QPointF t0(0., 1.);
    // lower right
    const QPointF p1(1., -1.);
    const QPointF t1(1., 0.);

    QVector<GLfloat> vertices;
    vertices << p0.x() << p0.y() << t0.x() << t0.y();
    vertices << p1.x() << p0.y() << t1.x() << t0.y();
    vertices << p1.x() << p1.y() << t1.x() << t1.y();
    vertices << p0.x() << p1.y() << t0.x() << t1.y();

    // fill in vertex buffer
    m_coordBuffer.create();
    m_coordBuffer.bind();
    m_coordBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_coordBuffer.allocate(vertices.constData(), sizeof(GLfloat) * vertices.size());

    // fill in index buffer
    m_indexBuffer.create();
    m_indexBuffer.bind();
    m_indexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_indexBuffer.allocate(indices.constData(), sizeof(GLuint) * indices.size());

  }
}

void ChartPainter::paintGL(const Camera *cam) {

  if (!m_viewArea.isValid()) return;

  m_fbo->bindDefault();

  auto funcs = QOpenGLContext::currentContext()->functions();
  funcs->glDisable(GL_DEPTH_TEST);
  funcs->glDisable(GL_STENCIL_TEST);
  funcs->glDisable(GL_CULL_FACE);
  funcs->glDisable(GL_BLEND);

  m_textureShader->initializePaint();

  m_coordBuffer.bind();
  m_indexBuffer.bind();
  funcs->glBindTexture(GL_TEXTURE_2D, m_fbo->texture());

  m_textureShader->setUniforms(cam, m_ref, m_viewArea);

  funcs->glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void ChartPainter::updateCharts(const Camera* cam, const QRectF& viewArea) {

  m_ref = cam->eye();
  m_viewArea = viewArea;

  auto bufCam = createBufferCamera(cam, viewArea.size());
  qreal scale = .5 * bufCam->heightMM() * dots_per_mm_y * bufCam->projection()(1, 1);

  const QSizeF bufSize(viewArea.width() * scale, viewArea.height() * scale);
  if (m_bufSize != bufSize) {
    m_bufSize = bufSize;
    delete m_fbo;
    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    m_fbo = new QOpenGLFramebufferObject(bufSize.toSize(), fmt);
  }

  // qDebug() << "updateCharts:" << bufSize << viewArea.size();

  m_fbo->bind();

  auto f = QOpenGLContext::currentContext()->extraFunctions();

  f->glEnable(GL_DEPTH_TEST);
  f->glDisable(GL_STENCIL_TEST);
  f->glDisable(GL_CULL_FACE);
  f->glDisable(GL_BLEND);
  f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  f->glClearColor(.7, .7, .7, 1.);
  f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  f->glViewport(0, 0, bufSize.width(), bufSize.height());

  for (S57Chart* chart: m_manager->charts()) {
    chart->updateModelTransform(bufCam);
  }

  // draw opaque objects nearest (highest priority) first
  for (int i = S52::Lookup::PriorityCount - 1; i >= 0; i--) {
    m_vectorShader->initializePaint();
    for (S57Chart* chart: m_manager->charts()) {
      chart->drawVectorSymbols(bufCam, i);
    }
    m_solidShader->initializePaint();
    for (S57Chart* chart: m_manager->charts()) {
      chart->drawSolidLines(bufCam, i);
    }
    m_areaShader->initializePaint();
    for (S57Chart* chart: m_manager->charts()) {
      chart->drawAreas(bufCam, i);
    }
  }

  f->glEnable(GL_BLEND);

  // draw translucent objects farthest first
  for (int i = 0; i < S52::Lookup::PriorityCount; i++) {
    m_dashedShader->initializePaint();
    for (S57Chart* chart: m_manager->charts()) {
      chart->drawDashedLines(bufCam, i);
    }
    m_rasterShader->initializePaint();
    for (S57Chart* chart: m_manager->charts()) {
      chart->drawRasterSymbols(bufCam, i);
    }
    m_textShader->initializePaint();
    for (S57Chart* chart: m_manager->charts()) {
      chart->drawText(bufCam, i);
    }
  }

  // draw stencilled objects
  for (S57Chart* chart: m_manager->charts()) {
    chart->drawRasterPatterns(bufCam);
    chart->drawVectorPatterns(bufCam);
  }

  f->glDisable(GL_BLEND);

  m_fbo->bindDefault();

  f->glViewport(0, 0,
                cam->heightMM() * cam->aspect() * dots_per_mm_x,
                cam->heightMM() * dots_per_mm_y);

  delete bufCam;
}

Camera* ChartPainter::createBufferCamera(const Camera *cam, const QSizeF &vp) const {
  auto bufCam = new OrthoCam(vp,
                             cam->eye(),
                             cam->scale(),
                             GeoProjection::CreateProjection(cam->geoprojection()->className()));

  bufCam->rotateEye(cam->northAngle());

  return bufCam;
}
