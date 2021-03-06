#version 320 es
layout (location = 0) in vec2 vertex;
layout (location = 1) in vec2 texin;
layout (location = 2) in vec2 pivot;

uniform mat4 m_p;
uniform float depth;
uniform float windowScale; // transform between display (mm) and chart (m) units
uniform mat4 m_model;
uniform vec2 offset;


out vec2 tex;

void main() {
  tex = texin;
  float a = 1. / windowScale;
  vec2 v =  a * (vertex + offset) + pivot;
  gl_Position = m_p * m_model * vec4(v, depth, 1.);
}

