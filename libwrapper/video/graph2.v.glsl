#version 330

attribute vec2 coord2d;
varying vec4 f_color;

void main(void) {
  gl_Position = vec4(coord2d.x, coord2d.y, 0, 1);
  if (coord2d.y > 0.1) {
	  f_color = vec4(0, 0, 0, 1.0);
  } else {
    f_color = vec4(0, 0, 0, 0);
  }
  gl_PointSize = 5.0;
}
