/* -*- coding: utf-8-unix -*-
 *
 * File: desktop/src/glwindow.h
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
#pragma once

#include <QOpenGLWindow>
#include <QOpenGLVertexArrayObject>

#include "detailmode.h"

class QOpenGLDebugLogger;
class ChartManager;
class TextManager;
class RasterSymbolManager;

class GLWindow: public QOpenGLWindow
{

  Q_OBJECT

public:

  GLWindow();
  ~GLWindow();
  void saveState();
  void northUp();
  void zoomIn();
  void zoomOut();
  void compassPan(Angle bearing, float pixels = 1.);
  void rotateEye(Angle amount);
  void setChartSet(const QString& s, bool force = false);

protected:

  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;

  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

signals:

  void updateViewport(const Camera* cam, quint32 flags = 0);
  void pointerChanged(const WGS84Point& loc);

private slots:

  void pan();

  void initializeChartMode();
  void finalizeChartMode();

  void updateCharts(const QRectF& viewArea);

private:


  QOpenGLVertexArrayObject m_vao;
  DetailMode* m_mode;
  QOpenGLDebugLogger* m_logger;

  QPoint m_diff;
  QPoint m_lastPos;
  bool m_gravity;
  QTimer* m_timer;
  quint64 m_moveCounter;

};

