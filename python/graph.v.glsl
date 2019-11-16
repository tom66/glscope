#version 120
#extension GL_OES_EGL_image_external : enable
attribute float coord1d;
//varying vec4 f_color;
uniform float offset_x;
uniform float offset_x2;
uniform float scale_x;
uniform float wavenum;
uniform float nwaves;
uniform float nxtiles;
uniform float xtile;
uniform sampler2D mytexture;

void main(void) {
	float x = fract(coord1d); // / scale_x) - offset_x;
        float wn = wavenum + (floor(coord1d) / (nwaves));
	float y = (texture2D(mytexture, vec2(x, wn)).r - 0.5) * 2.0;
//        gl_Position = gl_Vertex;
	gl_Position = vec4((x*2.0-1.0)/nxtiles + xtile, y, 0.0, 1.0)-vec4(offset_x2,0,0,0);
//	f_color = color;
}
