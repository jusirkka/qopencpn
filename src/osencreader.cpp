#include "osencreader.h"
#include <QFile>
#include <QDataStream>
#include <functional>
#include <QDate>
#include "s52presentation.h"



const QString& OsencReader::name() const {
  return m_name;
}

OsencReader::OsencReader()
  : m_name("osenc")
{}

using Buffer = QVector<char>;
using HandlerFunc = std::function<bool (const Buffer&)>;

struct Handler {
  Handler(HandlerFunc f): func(std::move(f)) {}
  HandlerFunc func;
  ~Handler() = default;
};

S57ChartOutline OsencReader::readOutline(const QString& path) {

  QFile file(path);
  if (!file.open(QFile::ReadOnly)) {
    throw ChartFileError(QString("Cannot open %1 for reading").arg(path));
  }
  QDataStream stream(&file);

  Buffer buffer;

  buffer.resize(sizeof(OSENC_Record_Base));
  if (stream.readRawData(buffer.data(), buffer.size()) == -1) {
    throw ChartFileError(QString("Error reading %1 bytes from %2").arg(buffer.size()).arg(path));
  }

  //  For identification purposes, the very first record must be the OSENC Version Number Record
  auto record = reinterpret_cast<OSENC_Record_Base*>(buffer.data());

  // Check Record
  if (record->record_type != SencRecordType::HEADER_SENC_VERSION){
    throw ChartFileError(QString("%1 is not a supported senc file").arg(path));
  }

  //  This is the correct record type (OSENC Version Number Record), so read it
  buffer.resize(record->record_length - sizeof(OSENC_Record_Base));
  if (stream.readRawData(buffer.data(), buffer.size()) == -1) {
    throw ChartFileError(QString("Error reading %1 bytes from %2").arg(buffer.size()).arg(path));
  }
  // auto p16 = reinterpret_cast<quint16*>(buffer.data());
  // qDebug() << "senc version =" << *p16;

  QDate pub;
  QDate mod;
  quint32 scale = 0;
  WGS84PointVector points;

  const QMap<SencRecordType, Handler*> handlers {
    {SencRecordType::HEADER_CELL_NAME, new Handler([] (const Buffer& b) {
        // qDebug() << "cell name" << QString::fromUtf8(b.constData());
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_PUBLISHDATE, new Handler([&pub] (const Buffer& b) {
        auto s = QString::fromUtf8(b.constData());
        // qDebug() << "cell publishdate" << s;
        pub = QDate::fromString(s, "yyyyMMdd");
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_UPDATEDATE, new Handler([&mod] (const Buffer& b) {
        auto s = QString::fromUtf8(b.constData());
        // qDebug() << "cell modified date" << s;
        mod = QDate::fromString(s, "yyyyMMdd");
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_EDITION, new Handler([] (const Buffer& b) {
        // const quint16* p16 = reinterpret_cast<const quint16*>(b.constData());
        // qDebug() << "cell edition" << *p16;
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_UPDATE, new Handler([] (const Buffer& b) {
        // const quint16* p16 = reinterpret_cast<const quint16*>(b.constData());
        // qDebug() << "cell update" << *p16;
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_NATIVESCALE, new Handler([&scale] (const Buffer& b) {
        const quint32* p32 = reinterpret_cast<const quint32*>(b.constData());
        // qDebug() << "cell nativescale" << *p32;
        scale = *p32;
        return true;
      })
    },
    {SencRecordType::HEADER_CELL_SENCCREATEDATE, new Handler([] (const Buffer& b) {
        // qDebug() << "cell senccreatedate" << QString::fromUtf8(b.constData());
        return true;
      })
    },
    {SencRecordType::CELL_EXTENT_RECORD, new Handler([&points] (const Buffer& b) {
        // qDebug() << "cell extent record";
        const OSENC_EXTENT_Record_Payload* p = reinterpret_cast<const OSENC_EXTENT_Record_Payload*>(b.constData());
        points << WGS84Point::fromLL(p->extent_sw_lon, p->extent_sw_lat);
        points << WGS84Point::fromLL(p->extent_se_lon, p->extent_se_lat);
        points << WGS84Point::fromLL(p->extent_ne_lon, p->extent_ne_lat);
        points << WGS84Point::fromLL(p->extent_nw_lon, p->extent_nw_lat);

        return true;
      })
    }
  };

  bool done = false;

  while (!done) {

    buffer.resize(sizeof(OSENC_Record_Base));
    if (stream.readRawData(buffer.data(), buffer.size()) == -1) {
      done = true;
      continue;
    }
    record = reinterpret_cast<OSENC_Record_Base*>(buffer.data());
    // copy, record_type will be overwritten in the next stream.readRawData
    SencRecordType rec_type = record->record_type;
    if (!handlers.contains(rec_type)) {
      done = true;
      continue;
    }
    buffer.resize(record->record_length - sizeof(OSENC_Record_Base));
    if (stream.readRawData(buffer.data(), buffer.size()) == -1) {
      done = true;
      continue;
    }

    done = !(handlers[rec_type])->func(buffer);

  }
  qDeleteAll(handlers);
  if (!pub.isValid() || !mod.isValid() || scale == 0 || points.isEmpty()) {
    throw ChartFileError(QString("Invalid osenc header in %1").arg(path));
  }

  return S57ChartOutline(points[0], points[1], points[2], points[3], scale, pub, mod);
}

namespace S57 {

// Helper class to set Object's private data
class ObjectBuilder {
public:
  void addAttribute(S57::Object* obj, quint16 acode, const Attribute& a) const {
    obj->m_attributes[acode] = a;
  }
  void addString(S57::Object* obj, quint16 acode, const QString& s) const {
    if (S52::GetAttributeType(acode) == S57::Attribute::Type::IntegerList) {
      const QStringList tokens = s.split(",");
      QVariantList vs;
      for (const QString& t: tokens) {
        vs << QVariant::fromValue(t.toInt());
      }
      // qDebug() << S52::GetAttributeName(acode) << vs;
      obj->m_attributes[acode] = S57::Attribute(vs);
    } else {
      obj->m_attributes[acode] = S57::Attribute(s);
    }
  }
  bool setGeometry(S57::Object* obj, S57::Geometry::Base* g, const QRectF& bb) const {
    if (obj->m_geometry != nullptr) {
      return false;
    }
    obj->m_geometry = g;
    obj->m_bbox = bb;
    return true;
  }
};

}

static GLsizei addIndices(GLuint first, GLuint mid1, GLuint mid2, bool reversed, GL::IndexVector& indices);
static GLuint addAdjacent(GLuint base, GLuint next, GL::VertexVector& vertices);
static QRectF computeBBox(const S57::ElementDataVector &elems,
                          const GL::VertexVector& vertices,
                          const GL::IndexVector& indices);
static QPointF computeLineCenter(const S57::ElementDataVector &elems,
                                 const GL::VertexVector& vertices,
                                 const GL::IndexVector& indices);
static QPointF computeAreaCenter(const S57::ElementDataVector &elems,
                                 const GL::VertexVector& vertices, GLsizei offset);


void OsencReader::readChart(GL::VertexVector& vertices,
                            GL::IndexVector& indices,
                            S57::ObjectVector& objects,
                            const QString& path,
                            const GeoProjection* proj) {

  QFile file(path);
  file.open(QFile::ReadOnly);
  QDataStream stream(&file);

  S57::Object* current = nullptr;
  S57::ObjectBuilder helper;

  struct TrianglePatch {
    GLenum mode;
    GL::VertexVector vertices;
  };

  using LineHandle = QVector<int>;
  using TriangleHandle = QVector<TrianglePatch>;

  struct OData {
    explicit OData(S57::Object* obj):
      object(obj),
      lines(),
      triangles(),
      type(S57::Geometry::Type::Meta) {}

    S57::Object* object;
    LineHandle lines;
    TriangleHandle triangles;
    S57::Geometry::Type type;
  };


  struct EdgeExtent {
    int first;
    int last;
  };

  QMap<int, int> connMap;
  QMap<int, EdgeExtent> edgeMap;

  QVector<OData> objectHandle;

  const QMap<SencRecordType, Handler*> handlers {

    {SencRecordType::FEATURE_ID_RECORD, new Handler([&current, &objectHandle] (const Buffer& b) {
        // qDebug() << "feature id record";
        auto p = reinterpret_cast<const OSENC_Feature_Identification_Record_Payload*>(b.constData());
        current = new S57::Object(p->feature_ID, p->feature_type_code);
        objectHandle.append(OData(current));
        return true;
      })
    },

    {SencRecordType::FEATURE_ATTRIBUTE_RECORD, new Handler([&current, helper] (const Buffer& b) {
        if (!current) return false;
        auto p = reinterpret_cast<const OSENC_Attribute_Record_Payload*>(b.constData());
        auto t = static_cast<S57::Attribute::Type>(p->attribute_value_type);
        // qDebug() << "feature attribute record" << p->attribute_type_code << S52::GetAttributeName(p->attribute_type_code) << p->attribute_value_type;
        switch (t) {
        case S57::Attribute::Type::Integer: {
          int v;
          memcpy(&v, &p->attribute_data, sizeof(int));
          helper.addAttribute(current,
                              p->attribute_type_code,
                              S57::Attribute(v));
          return true;
        }
        case S57::Attribute::Type::Real: {
          double v;
          memcpy(&v, &p->attribute_data, sizeof(double));
          helper.addAttribute(current,
                              p->attribute_type_code,
                              S57::Attribute(v));
          return true;
        }
        case S57::Attribute::Type::String: {
          const char* s = &p->attribute_data; // null terminated string
          // handles strings and integer lists
          helper.addString(current,
                           p->attribute_type_code,
                           QString::fromUtf8(s));
          return true;
        }
        case S57::Attribute::Type::None: {
          helper.addAttribute(current,
                              p->attribute_type_code,
                              S57::Attribute(S57::Attribute::Type::None));
          return true;
        }
        default: return false;
        }
        return false;
      })
    },

    {SencRecordType::FEATURE_GEOMETRY_RECORD_POINT, new Handler([&current, &objectHandle, helper, proj] (const Buffer& b) {
        // qDebug() << "feature geometry/point record";
        if (!current) return false;
        auto p = reinterpret_cast<const OSENC_PointGeometry_Record_Payload*>(b.constData());
        auto p0 = proj->fromWGS84(WGS84Point::fromLL(p->lon, p->lat));
        objectHandle.last().type = S57::Geometry::Type::Point;
        QRectF bb(p0 - QPointF(10, 10), QSizeF(20, 20));
        return helper.setGeometry(current, new S57::Geometry::Point(p0, proj), bb);
      })
    },

    {SencRecordType::FEATURE_GEOMETRY_RECORD_LINE, new Handler([&objectHandle] (const Buffer& b) {
        // qDebug() << "feature geometry/line record";
        auto p = reinterpret_cast<const OSENC_LineGeometry_Record_Payload*>(b.constData());
        LineHandle es(p->edgeVector_count * 3);
        memcpy(es.data(), &p->edge_data, p->edgeVector_count * 3 * sizeof(int));
        objectHandle.last().lines.append(es);
        objectHandle.last().type = S57::Geometry::Type::Line;
        return true;
      })
    },

    {SencRecordType::FEATURE_GEOMETRY_RECORD_AREA, new Handler([&objectHandle] (const Buffer& b) {
        // qDebug() << "feature geometry/area record";
        auto p = reinterpret_cast<const OSENC_AreaGeometry_Record_Payload*>(b.constData());
        LineHandle es(p->edgeVector_count * 3);
        const uint8_t* ptr = &p->edge_data;
        // skip contour counts
        ptr += p->contour_count * sizeof(int);
        TriangleHandle ts(p->triprim_count);
        for (uint i = 0; i < p->triprim_count; i++) {
          ts[i].mode = static_cast<GLenum>(*ptr); // Note: relies on GL_TRIANGLE* to be less than 128
          ptr++;
          auto nvert = *reinterpret_cast<const uint32_t*>(ptr);
          ptr += sizeof(uint32_t);
          ptr += 4 * sizeof(double); // skip bbox
          ts[i].vertices.resize(nvert * 2);
          memcpy(ts[i].vertices.data(), ptr, nvert * 2 * sizeof(float));
          ptr += nvert * 2 * sizeof(float);
        }
        memcpy(es.data(), ptr, p->edgeVector_count * 3 * sizeof(int));
        objectHandle.last().lines.append(es);
        objectHandle.last().triangles.append(ts);
        objectHandle.last().type = S57::Geometry::Type::Area;
        return true;
      })
    },

    {SencRecordType::FEATURE_GEOMETRY_RECORD_MULTIPOINT, new Handler([&current, &objectHandle, helper, proj] (const Buffer& b) {
        // qDebug() << "feature geometry/multipoint record";
        if (!current) return false;
        auto p = reinterpret_cast<const OSENC_MultipointGeometry_Record_Payload*>(b.constData());
        S57::Geometry::PointVector ps(p->point_count * 3);
        memcpy(ps.data(), &p->point_data, p->point_count * 3 * sizeof(double));
        objectHandle.last().type = S57::Geometry::Type::Point;
        // compute bounding box
        QPointF ur(-1.e15, -1.e15);
        QPointF ll(1.e15, 1.e15);
        for (uint i = 0; i < p->point_count; i++) {
          const QPointF q(ps[3 * i], ps[3 * i + 1]);
          ur.setX(qMax(ur.x(), q.x()));
          ur.setY(qMax(ur.y(), q.y()));
          ll.setX(qMin(ll.x(), q.x()));
          ll.setY(qMin(ll.y(), q.y()));
        }
        return helper.setGeometry(current, new S57::Geometry::Point(ps, proj), QRectF(ll, ur));
      })
    },

    {SencRecordType::VECTOR_EDGE_NODE_TABLE_RECORD, new Handler([&vertices, &edgeMap] (const Buffer& b) {
        // qDebug() << "vector edge node table record";
        const char* ptr = b.constData();

        // The Feature(Object) count
        auto cnt = *reinterpret_cast<const quint32*>(ptr);
        ptr += sizeof(quint32);

        for (uint i = 0; i < cnt; i++) {
          auto index = *reinterpret_cast<const quint32*>(ptr);
          ptr += sizeof(quint32);

          auto pcnt = *reinterpret_cast<const quint32*>(ptr);
          ptr += sizeof(quint32);

          QVector<float> edges(2 * pcnt);
          memcpy(edges.data(), ptr, 2 * pcnt * sizeof(float));
          ptr += 2 * pcnt * sizeof(float);

          EdgeExtent edge;
          edge.first = vertices.size() / 2;
          edge.last = edge.first + pcnt - 1;
          edgeMap[index] = edge;
          vertices.append(edges);
        }
        return true;
      })
    },

    {SencRecordType::VECTOR_CONNECTED_NODE_TABLE_RECORD, new Handler([&vertices, &connMap] (const Buffer& b) {
        // qDebug() << "vector connected node table record";
        const char* ptr = b.constData();

        // The Feature(Object) count
        auto cnt = *reinterpret_cast<const quint32*>(ptr);

        ptr += sizeof(quint32);

        for (uint i = 0; i < cnt; i++) {
          auto index = *reinterpret_cast<const quint32*>(ptr);
          ptr += sizeof(quint32);

          auto x = *reinterpret_cast<const float*>(ptr);
          ptr += sizeof(float);
          auto y = *reinterpret_cast<const float*>(ptr);
          ptr += sizeof(float);

          connMap[index] = vertices.size() / 2;
          vertices.append(x);
          vertices.append(y);
        }
        return true;
      })
    },

    {SencRecordType::CELL_COVR_RECORD, new Handler([] (const Buffer& b) {
        // qDebug() << "cell coverage record";
        return true;
      })
    },

    {SencRecordType::CELL_NOCOVR_RECORD, new Handler([] (const Buffer& b) {
        qDebug() << "cell nocoverage record";
        return true;
      })
    },
  };

  bool done = false;

  while (!done && !stream.atEnd()) {

    Buffer buffer;

    buffer.resize(sizeof(OSENC_Record_Base));
    if (stream.readRawData(buffer.data(), buffer.size()) == -1) {
      done = true;
      qWarning() << "Cannot read base record";
      continue;
    }

    auto record = reinterpret_cast<OSENC_Record_Base*>(buffer.data());

    // copy, record_type will be overwritten in the next stream.readRawData
    SencRecordType rec_type = record->record_type;

    buffer.resize(record->record_length - sizeof(OSENC_Record_Base));
    if (stream.readRawData(buffer.data(), buffer.size()) == -1) {
      done = true;
      qWarning() << "Cannot read base record data";
      continue;
    }
    if (!handlers.contains(rec_type)) {
      continue;
    }

    done = !(handlers[rec_type])->func(buffer);
    if (done) {
      qWarning() << "Handler failed for type" << as_numeric(rec_type);
    }

  }
  qDeleteAll(handlers);

  auto lineGeometry = [&indices, &vertices, edgeMap, connMap] (const LineHandle& geom, S57::ElementDataVector& elems) {
    const int cnt = geom.size() / 3;
    // qDebug() << geom;
    for (int i = 0; i < cnt;) {
      S57::ElementData e;
      e.mode = GL_LINE_STRIP_ADJACENCY;
      e.offset = indices.size() * sizeof(GLuint);
      indices.append(0); // dummy index to account adjacency
      e.count = 1;
      int np = 3 * i;
      while (i < cnt && geom[3 * i] == geom[np]) {
        const int c1 = connMap[geom[3 * i]];
        const int m = geom[3 * i + 1];
        if (edgeMap.contains(std::abs(m))) {
          const int first = edgeMap[std::abs(m)].first;
          const int last = edgeMap[std::abs(m)].last;
          e.count += addIndices(c1, first, last, m < 0, indices);
        } else {
          indices.append(c1);
          e.count += 1;
        }
        np = 3 * i + 2;
        i++;
      }
      if (i < cnt) {
        const GLuint last = connMap[geom[3 * i - 1]];
        // account adjacency
        const int adj = e.offset / sizeof(GLuint);
        const GLuint first = indices[adj + 1];
        if (last == first) {
          indices[adj] = indices.last(); // prev
          indices.append(last); // last real index
          indices.append(indices[adj + 2]); // adjacent = next of first
        } else {
          indices[adj] = addAdjacent(first, indices[adj + 2], vertices);
          indices.append(last);
          indices.append(addAdjacent(last, indices.last(), vertices));
        }
        e.count += 2;
      }
      elems.append(e);
    }
    const GLuint last = connMap[geom[geom.size() - 1]];
    // account adjacency
    const int adj = elems.last().offset / sizeof(GLuint);
    const GLuint first = indices[adj + 1];
    if (last == first) {
      indices[adj] = indices.last(); // prev
      indices.append(last); // last real index
      indices.append(indices[adj + 2]); // adjacent = next of first
    } else {
      indices.append(last); // adjacent = same as last
      indices[adj] = addAdjacent(first, indices[adj + 2], vertices);
      indices.append(last);
      indices.append(addAdjacent(last, indices.last(), vertices));
    }
    elems.last().count += 2;
  };

  auto triangleGeometry = [&vertices] (const TriangleHandle& geom, S57::ElementDataVector& elems) {
    GLint offset = 0;
    for (const TrianglePatch& p: geom) {
      S57::ElementData d;
      d.mode = p.mode;
      d.offset = offset;
      d.count = p.vertices.size() / 2;
      vertices.append(p.vertices);
      elems.append(d);
      offset += d.count;
    }
  };

  for (OData& d: objectHandle) {

    // setup meta, line and area geometries
    if (d.type == S57::Geometry::Type::Meta) {
      helper.setGeometry(d.object, new S57::Geometry::Meta(), QRectF());
    } else if (d.type == S57::Geometry::Type::Line) {

      S57::ElementDataVector elems;
      lineGeometry(d.lines, elems);

      const QPointF c = computeLineCenter(elems, vertices, indices);
      const QRectF bbox = computeBBox(elems, vertices, indices);
      helper.setGeometry(d.object,
                         new S57::Geometry::Line(elems, c, 0, proj),
                         bbox);

    } else if (d.type == S57::Geometry::Type::Area) {

      S57::ElementDataVector lelems;
      lineGeometry(d.lines, lelems);

      GLsizei offset = vertices.size() * sizeof(GLfloat);
      S57::ElementDataVector telems;
      triangleGeometry(d.triangles, telems);

      const QPointF c = computeAreaCenter(telems, vertices, offset);
      const QRectF bbox = computeBBox(lelems, vertices, indices);
      helper.setGeometry(d.object,
                         new S57::Geometry::Area(lelems, c, telems, offset, false, proj),
                         bbox);
    }

    objects.append(d.object);
  }
}


static QRectF computeBBox(const S57::ElementDataVector &elems,
                          const GL::VertexVector& vertices,
                          const GL::IndexVector& indices) {

  QPointF ur(-1.e15, -1.e15);
  QPointF ll(1.e15, 1.e15);

  for (const S57::ElementData& elem: elems) {
    const int first = elem.offset / sizeof(GLuint);
    for (uint i = 0; i < elem.count; i++) {
      const int index = indices[first + i];
      QPointF q(vertices[2 * index], vertices[2 * index + 1]);
      ur.setX(qMax(ur.x(), q.x()));
      ur.setY(qMax(ur.y(), q.y()));
      ll.setX(qMin(ll.x(), q.x()));
      ll.setY(qMin(ll.y(), q.y()));
    }
  }
  return QRectF(ll, ur); // inverted y-axis
}

static QPointF computeLineCenter(const S57::ElementDataVector &elems,
                                 const GL::VertexVector& vertices,
                                 const GL::IndexVector& indices) {
  int first = elems[0].offset / sizeof(GLuint) + 1; // account adjacency
  int last = first + elems[0].count - 3; // account adjacency
  if (elems.size() > 1 || indices[first] == indices[last]) {
    // several lines or closed loops: compute center of gravity
    QPointF s(0, 0);
    int n = 0;
    for (auto& elem: elems) {
      first = elem.offset / sizeof(GLuint) + 1; // account adjacency
      last = first + elem.count - 3; // account adjacency
      for (int i = first; i <= last; i++) {
        const int index = indices[i];
        s.rx() += vertices[2 * index + 0];
        s.ry() += vertices[2 * index + 1];
      }
      n += elem.count - 2;
    }
    return s / n;
  }
  // single open line: compute mid point of running length
  QVector<float> lengths;
  float len = 0;
  for (int i = first; i < last; i++) {
    const int i1 = indices[i + 1];
    const int i0 = indices[i];
    const QPointF d(vertices[2 * i1] - vertices[2 * i0], vertices[2 * i1 + 1] - vertices[2 * i0 + 1]);
    lengths.append(sqrt(QPointF::dotProduct(d, d)));
    len += lengths.last();
  }
  const float halfLen = len / 2;
  len = 0;
  int i = 0;
  while (i < lengths.size() && len < halfLen) {
    len += lengths[i];
    i++;
  }
  const int i1 = indices[first + i];
  const int i0 = indices[first + i - 1];
  const QPointF p1(vertices[2 * i1], vertices[2 * i1 + 1]);
  const QPointF p0(vertices[2 * i0], vertices[2 * i0 + 1]);
  return p0 + (len - halfLen) * (p1 - p0);

}

static QPointF computeAreaCenter(const S57::ElementDataVector &elems,
                                 const GL::VertexVector& vertices,
                                 GLsizei offset) {
  float area = 0;
  QPoint s(0, 0);
  for (const S57::ElementData& elem: elems) {
    if (elem.mode == GL_TRIANGLES) {
      int first = offset / sizeof(GLfloat) + 2 * elem.offset;
      for (uint i = 0; i < elem.count / 3; i++) {
        const QPointF p0(vertices[first + 6 * i + 0], vertices[first + 6 * i + 1]);
        const QPointF p1(vertices[first + 6 * i + 2], vertices[first + 6 * i + 3]);
        const QPointF p2(vertices[first + 6 * i + 4], vertices[first + 6 * i + 5]);
        const float da = std::abs((p1.x() - p0.x()) * (p2.y() - p0.y()) - (p2.x() - p0.x()) * (p1.y() - p0.y()));
        area += da;
        s.rx() += da / 3. * (p0.x() + p1.x() + p2.x());
        s.ry() += da / 3. * (p0.y() + p1.y() + p2.y());
      }
    } else if (elem.mode == GL_TRIANGLE_STRIP) {
      int first = offset / sizeof(GLfloat) + 2 * elem.offset;
      for (uint i = 0; i < elem.count - 2; i++) {
        const QPointF p0(vertices[first + 2 * i + 0], vertices[first + 2 * i + 1]);
        const QPointF p1(vertices[first + 2 * i + 2], vertices[first + 2 * i + 3]);
        const QPointF p2(vertices[first + 2 * i + 4], vertices[first + 2 * i + 5]);
        const float da = std::abs((p1.x() - p0.x()) * (p2.y() - p0.y()) - (p2.x() - p0.x()) * (p1.y() - p0.y()));
        area += da;
        s.rx() += da / 3. * (p0.x() + p1.x() + p2.x());
        s.ry() += da / 3. * (p0.y() + p1.y() + p2.y());
      }
    } else if (elem.mode == GL_TRIANGLE_FAN) {
      int first = offset / sizeof(GLfloat) + 2 * elem.offset;
      const QPointF p0(vertices[first + 0], vertices[first + 1]);
      for (uint i = 0; i < elem.count - 2; i++) {
        const QPointF p1(vertices[first + 2 * i + 2], vertices[first + 2 * i + 3]);
        const QPointF p2(vertices[first + 2 * i + 4], vertices[first + 2 * i + 5]);
        const float da = std::abs((p1.x() - p0.x()) * (p2.y() - p0.y()) - (p2.x() - p0.x()) * (p1.y() - p0.y()));
        area += da;
        s.rx() += da / 3. * (p0.x() + p1.x() + p2.x());
        s.ry() += da / 3. * (p0.y() + p1.y() + p2.y());
      }
    } else {
      Q_ASSERT_X(false, "computeAreaCenter", "Unknown primitive");
    }
  }

  return s / area;
}

static GLsizei addIndices(GLuint first, GLuint mid1, GLuint mid2, bool reversed, GL::IndexVector& indices) {
  indices.append(first);
  if (!reversed) {
    for (uint i = mid1; i <= mid2; i++) {
      indices.append(i);
    }
  } else {
    for (int i = mid2; i >= static_cast<int>(mid1); i--) {
      indices.append(i);
    }
  }
  return 1 + mid2 - mid1 + 1;
}

static GLuint addAdjacent(GLuint base, GLuint next, GL::VertexVector& vertices) {
  const float x1 = vertices[2 * base];
  const float y1 = vertices[2 * base + 1];
  const float x2 = vertices[2 * next];
  const float y2 = vertices[2 * next + 1];
  vertices.append(2 * x1 - x2);
  vertices.append(2 * y1 - y2);
  return (vertices.size() - 1) / 2;
}

