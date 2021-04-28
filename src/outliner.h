/* -*- coding: utf-8-unix -*-
 *
 * File: src/outliner.h
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

#include "drawable.h"
#include <QOpenGLBuffer>
#include <glm/glm.hpp>
#include <QColor>

class ChartManager;

class Outliner: public Drawable {

  Q_OBJECT

public:

  Outliner(QObject* parent);

  void paintGL(const Camera* cam) override;
  void initializeGL() override;
  void updateCharts(const Camera* cam, const QRectF& viewArea) override;

  ~Outliner() = default;

private:

  void updateBuffers();

  QOpenGLBuffer m_cornerBuffer;
  int m_instances;
  QOpenGLShaderProgram* m_program;

  struct _locations {
    int base_color;
    int m_pv;
    int center;
    int angle;
    int nump;
  } m_locations;


  ChartManager* m_manager;

};

