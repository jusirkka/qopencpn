/* -*- coding: utf-8-unix -*-
 *
 * File: src/hpglparser.cpp
 *
 * Copyright (C) 2021 Jukka Sirkka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "hpglopenglparser.h"
#include <QDebug>
#include <glm/glm.hpp>
#include "triangulator.h"




HPGL::OpenGLParser::OpenGLParser(const QString &src, const QString& colors,
                                 const QPointF& pivot)
  : Parser(colors)
  , m_started(false)
  , m_penDown(false)
  , m_pivot(pivot)
{

  parse(src);

  if (!m_ok) {
    return;
  }

  while (m_sketches.size() > 0) {
    edgeSketch();
  }
  edgeSketch();

  DataIterator it = m_storage.begin();
  while (it != m_storage.end()) {
    m_data.append(it.value());
    it = m_storage.erase(it);
  }
}


void HPGL::OpenGLParser::setColor(char c) {
  if (!m_cmap.contains(c)) {
    qWarning() << c << "not in colormap";
    m_ok = false;
    return;
  }
  const quint32 index = m_cmap[c];
  if (!m_started) {
    m_started = true;
    m_currentSketch.color = S52::Color(index);
    m_currentSketch.lineWidth = 0.;
    LineString ls;
    ls.closed = false;
    m_currentSketch.parts.append(ls);
  } else if (m_currentSketch.color.index != index) {
    Sketch sketch = m_currentSketch.clone();
    sketch.color.index = index;
    m_sketches.push(m_currentSketch);
    m_currentSketch = sketch;
  }
}

void HPGL::OpenGLParser::setAlpha(int a) {
  auto alpha = as_enum<S52::Alpha>(a, S52::AllAlphas);
  if (m_currentSketch.color.alpha == S52::Alpha::Unset) {
    m_currentSketch.color.alpha = alpha;
  } else if (m_currentSketch.color.alpha != alpha) {
    Sketch sketch = m_currentSketch.clone();
    sketch.color.alpha = alpha;
    m_sketches.push(m_currentSketch);
    m_currentSketch = sketch;
  }
}

void HPGL::OpenGLParser::setWidth(int w) {
  const qreal lw = S52::LineWidthMM(w);
  if (m_currentSketch.lineWidth == 0.) {
    m_currentSketch.lineWidth = lw;
  } else if (m_currentSketch.lineWidth != lw) {
    Sketch sketch = m_currentSketch.clone();
    sketch.lineWidth = lw;
    m_sketches.push(m_currentSketch);
    m_currentSketch = sketch;
  }
}

void HPGL::OpenGLParser::movePen(const RawPoints &ps) {
  if (ps.size() % 2 != 0) {
    qWarning() << ps << "contains odd number of coordinates";
    m_ok = false;
    return;
  }

  m_pen = makePoint(ps[ps.size() - 2], ps[ps.size() - 1]);

  LineString ls;
  ls.closed = false;
  m_currentSketch.parts.append(ls);
  m_penDown = false;
}

void HPGL::OpenGLParser::drawLineString(const RawPoints &ps) {
  if (ps.isEmpty()) {
    // draw a single point
    drawPoint();
    m_penDown = true;
    return;
  }

  if (ps.size() % 2 != 0) {
    qWarning() << ps << "contains odd number of coordinates";
    m_ok = false;
    return;
  }

  if (!m_penDown) {
    m_currentSketch.parts.last().points.append(m_pen);
  }
  for (int i = 0; i < ps.size() / 2; i++) {
    m_currentSketch.parts.last().points.append(makePoint(ps[2 * i], ps[2 * i + 1]));
  }

  m_currentSketch.parts.last().closed =
      m_currentSketch.parts.last().points.first() ==
      m_currentSketch.parts.last().points.last();

  m_pen = m_currentSketch.parts.last().points.last();
  m_penDown = true;
}

void HPGL::OpenGLParser::drawPoint() {
  pushSketch();
  // opaque fill
  m_currentSketch.color.alpha = S52::Alpha::P0;
  const qreal lw = m_currentSketch.lineWidth == 0. ?
        S52::LineWidthMM(1) : m_currentSketch.lineWidth;
  const QPointF q = .5 * QPointF(lw, lw);
  const QPointF p = .5 * QPointF(lw, -lw);
  m_currentSketch.parts.last().closed = true;
  m_currentSketch.parts.last().points.append(m_pen - q);
  m_currentSketch.parts.last().points.append(m_pen + p);
  m_currentSketch.parts.last().points.append(m_pen + q);
  m_currentSketch.parts.last().points.append(m_pen - p);
  m_currentSketch.parts.last().points.append(m_pen - q);
  fillSketch();
}


void HPGL::OpenGLParser::drawCircle(int r0) {
  LineString ls;
  ls.closed = true;
  const qreal lw = m_currentSketch.lineWidth == 0 ?
        S52::LineWidthMM(1) : m_currentSketch.lineWidth;

  const qreal r = mmUnit * r0;


  auto n = qMin(90, 3 + 4 * static_cast<int>(r / lw));
  for (int i = 0; i <= n; i++) {
    const qreal a = 2 * i * M_PI / n;
    const QPointF p = m_pen + QPointF(r * cos(a), r * sin(a));
    ls.points.append(p);
  }
  m_currentSketch.parts.append(ls);
  LineString ls2;
  ls2.closed = false;
  if (m_penDown) {
    ls2.points.append(m_pen);
  }
  m_currentSketch.parts.append(ls2);
}

void HPGL::OpenGLParser::drawArc(int x0, int y0, int a0) {
  LineString ls;
  ls.closed = false;
  const qreal lw = m_currentSketch.lineWidth == 0 ?
        S52::LineWidthMM(1) : m_currentSketch.lineWidth;

  const QPointF c = makePoint(x0, y0);
  const QPointF d = m_pen - c;
  const qreal r = sqrt(QPointF::dotProduct(d, d));
  const int n = qMin(90., std::ceil((3. + 4 * r / lw) * a0 / 360.));
  for (int i = 0; i <= n; i++) {
    const qreal a = a0 / 180. * i  * M_PI / n;
    const qreal ca = cos(a);
    const qreal sa = sin(a);
    const QPointF p = c + QPointF(ca * d.x() - sa * d.y(), sa * d.x() + ca * d.y());
    ls.points.append(p);
  }
  m_currentSketch.parts.append(ls);
  LineString ls2;
  ls2.closed = false;
  if (m_penDown) {
    ls2.points.append(m_pen);
  }
  m_currentSketch.parts.append(ls2);
}

void HPGL::OpenGLParser::pushSketch() {
  Sketch sketch = m_currentSketch.clone();
  m_sketches.push(m_currentSketch);
  m_currentSketch = sketch;
}

void HPGL::OpenGLParser::endSketch() {
  // noop
}

void HPGL::OpenGLParser::fillSketch() {
  // triangulate
  Data d;
  d.color = m_currentSketch.color;
  if (d.color.alpha == S52::Alpha::Unset) {
    d.color.alpha = S52::Alpha::P0;
  }
  for (const LineString& part: m_currentSketch.parts) {
    if (!part.closed) continue;
    if (part.points.size() < 3) continue;
    triangulate(part.points, d);
  }
  // merge storage
  if (!d.vertices.isEmpty()) {
    if (m_storage.contains(d.color)) {
      mergeData(m_storage[d.color], d);
    } else {
      m_storage.insert(d.color, d);
    }
  }
  // pop sketch
  if (!m_sketches.isEmpty()) {
    m_currentSketch = m_sketches.pop();
  }
}


void HPGL::OpenGLParser::edgeSketch() {
  // thicker lines
  Data d;
  d.color.index = m_currentSketch.color.index;
  d.color.alpha = S52::Alpha::P0;
  const qreal lw = m_currentSketch.lineWidth == 0. ?
        S52::LineWidthMM(1) : m_currentSketch.lineWidth;

  for (const LineString& part: m_currentSketch.parts) {
    if (part.points.size() < 2) continue;
    thickerlines(part, lw, d);
  }
  // merge storage
  if (!d.vertices.isEmpty()) {
    if (m_storage.contains(d.color)) {
      mergeData(m_storage[d.color], d);
    } else {
      m_storage.insert(d.color, d);
    }
  }
  // pop sketch
  if (!m_sketches.isEmpty()) {
    m_currentSketch = m_sketches.pop();
  }
}

void HPGL::OpenGLParser::mergeData(Data &tgt, const Data &d) {
  const GLuint offset = tgt.vertices.size() / 2;
  tgt.vertices.append(d.vertices);
  for (auto index: d.indices) {
    tgt.indices << offset + index;
  }
}


QPointF HPGL::OpenGLParser::makePoint(int x, int y) const {
  return QPointF(x * mmUnit - m_pivot.x(),
                 m_pivot.y() - y * mmUnit);
}

void HPGL::OpenGLParser::triangulate(const PointList& points, Data& out) {

  GL::VertexVector vertices;
  for (const QPointF p0: points) {
    vertices << p0.x() << p0.y();
  }

  Triangulator tri(vertices);
  tri.addPolygon(0, vertices.size() / 2 - 1);
  auto indices = tri.triangulate();

  const GLuint offset = out.vertices.size() / 2;
  out.vertices.append(vertices);
  for (auto index: indices) {
    out.indices << offset + index;
  }

}

//
// Original source:
// https://github.com/paulhoux/Cinder-Samples/blob/master/GeometryShader/assets/shaders/lines1.geom
//

void HPGL::OpenGLParser::thickerlines(const LineString &ls, qreal lw, Data &out) {
  const GLfloat MITER_LIMIT = .75;

  const float HW = .5 * lw;

  GLuint offset = out.vertices.size() / 2;
  const int n = ls.closed ? ls.points.size() - 2 : ls.points.size() - 1;
  for (int i = 0; i < n; i++) {

    const glm::vec2 p0 = GL::Vec2(i == 0 ? (ls.closed ? ls.points[n] : 2 * ls.points[0] - ls.points[1]) :
      ls.points[i - 1]);
    const glm::vec2 p1 = GL::Vec2(ls.points[i]);
    const glm::vec2 p2 = GL::Vec2(ls.points[i + 1]);
    const glm::vec2 p3 = GL::Vec2(i == n - 1 ? (ls.closed ? ls.points[0] : ls.points[n - 1] -  2 * ls.points[n]) :
      ls.points[i + 2]);

    // determine the direction of each of the 3 segments (previous, current, next)
    const glm::vec2 v0 = glm::normalize(p1 - p0);
    const glm::vec2 v1 = glm::normalize(p2 - p1);
    const glm::vec2 v2 = glm::normalize(p3 - p2);

    // determine the normal of each of the 3 segments (previous, current, next)
    const glm::vec2 n0 = glm::vec2(-v0.y, v0.x);
    const glm::vec2 n1 = glm::vec2(-v1.y, v1.x);
    const glm::vec2 n2 = glm::vec2(-v2.y, v2.x);

    // determine miter lines by averaging the normals of the 2 segments
    glm::vec2 miter_a = glm::normalize(n0 + n1);	// miter at start of current segment
    glm::vec2 miter_b = glm::normalize(n1 + n2);	// miter at end of current segment

    // determine the length of the miter from a right angled triangle (miter, n, v)
    GLfloat len_a = HW / glm::dot(miter_a, n1);
    GLfloat len_b = HW / glm::dot(miter_b, n1);

    glm::vec2 r;
    // prevent excessively long miters at sharp corners
    if (glm::dot(v0, v1) < - MITER_LIMIT) {
      miter_a = n1;
      len_a = HW;

      // close the gap
      if(glm::dot(v0, n1) > 0) {
        r = p1 + HW * n0;
        out.vertices << r.x << r.y;
        r = p1 + HW * n1;
        out.vertices << r.x << r.y;
        r = p1;
        out.vertices << r.x << r.y;
      } else {
        r = p1 - HW * n1;
        out.vertices << r.x << r.y;
        r = p1 - HW * n0;
        out.vertices << r.x << r.y;
        r = p1;
        out.vertices << r.x << r.y;
      }
      out.indices << offset << offset + 1 << offset + 2;
      offset += 3;
    }

    if (glm::dot(v1, v2) < -MITER_LIMIT ) {
      miter_b = n1;
      len_b = HW;
    }

    // generate the triangle strip
    r = p1 + len_a * miter_a;
    out.vertices << r.x << r.y;
    r = p1 - len_a * miter_a;
    out.vertices << r.x << r.y;
    r = p2 + len_b * miter_b;
    out.vertices << r.x << r.y;
    r = p2 - len_b * miter_b;
    out.vertices << r.x << r.y;

    out.indices << offset << offset + 1 << offset + 3
                << offset << offset + 3 << offset + 2;
    offset += 4;
  }
}

