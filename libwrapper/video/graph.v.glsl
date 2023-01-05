#version 330

attribute float coord1d;
varying vec4 f_color;
uniform sampler2D mytexture;

void main(void) {
	float x = coord1d;
    float x1 = x / 2.0 + 0.5;
    float t = texture(mytexture, vec2(x1, 0)).r;
	float y = (t - 0.5) * 2.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
    float a1 = x / 2.0 + 0.5;
    float a2 = 0;
    if (y < 0)
        a2 = 1 + y;
    else
        a2 = 1 - y;
    float a = a1 * a2;
	f_color = vec4(x / 2.0 + 0.5, y / 2.0 + 0.5, 1.0, a);
	gl_PointSize = 3.0;
}
