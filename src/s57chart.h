#pragma once

#include "types.h"
#include <QObject>
#include "s57object.h"
#include "s52presentation.h"
#include <QOpenGLBuffer>



class GeoProjection;
class Settings;
class Camera;
class QOpenGLContext;

class S57Chart: public QObject {

  Q_OBJECT

public:

  S57Chart(quint32 id, const QString& path);

  void drawAreas(const Camera* cam, int prio);
  void drawSolidLines(const Camera* cam, int prio);
  void drawText(const Camera* cam, int prio);
  void drawDashedLines(const Camera* cam, int prio);
  void drawRasterSymbols(const Camera* cam, int prio);
  void drawVectorSymbols(const Camera* cam, int prio);
  void drawVectorPatterns(const Camera* cam);
  void drawRasterPatterns(const Camera* cam);

  const GeoProjection* geoProjection() const {return m_nativeProj;}

  quint32 id() const {return m_id;}

  void updatePaintData(const QRectF& viewArea, quint32 scale);
  void finalizePaintData();

  ~S57Chart();

signals:

public slots:

private:

  struct ObjectLookup {
    ObjectLookup(const S57::Object* obj, S52::Lookup* lup)
      : object(obj)
      , lookup(lup) {}

    ObjectLookup() = default;

    const S57::Object* object;
    S52::Lookup* lookup;
  };

  using ObjectLookupVector = QVector<ObjectLookup>;

  using PaintPriorityVector = QVector<S57::PaintDataMap>;

  using LocationHash = QMultiHash<WGS84Point, const S57::Object*>;
  using LocationIterator = LocationHash::const_iterator;

  using SymbolMap = QHash<SymbolKey, S57::PaintData*>;
  using SymbolIterator = QHash<SymbolKey, S57::PaintData*>::const_iterator;
  using SymbolMutIterator = QHash<SymbolKey, S57::PaintData*>::iterator;
  using SymbolPriorityVector = QVector<SymbolMap>;

  qreal scaleFactor(const QRectF& vp, quint32 scale);

  GeoProjection* m_nativeProj;
  ObjectLookupVector m_lookups;
  LocationHash m_locations;
  PaintPriorityVector m_paintData;
  PaintPriorityVector m_updatedPaintData;
  GL::VertexVector m_updatedVertices;
  GL::VertexVector m_updatedPivots;
  GL::VertexVector m_updatedTransforms;
  quint32 m_id;
  Settings* m_settings;
  QOpenGLBuffer m_coordBuffer;
  QOpenGLBuffer m_indexBuffer;
  QOpenGLBuffer m_pivotBuffer;
  QOpenGLBuffer m_transformBuffer;
  GLsizei m_staticVertexOffset;
  GLsizei m_staticElemOffset;

};

