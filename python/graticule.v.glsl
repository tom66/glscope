varying vec4 f_color;
uniform vec4 color;
attribute vec2 coord2d;
void main() {
  gl_Position = vec4(coord2d,0.0,1.0);
  f_color = color;
}