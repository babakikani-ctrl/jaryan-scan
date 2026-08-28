#version 150
// draw the tile's code glyph, sampled from the atlas, tinted by its bold palette colour
uniform sampler2D uAtlas;
uniform float uGrid;
uniform float uBright;
in  vec3 vCol;
in  float vLum;
flat in vec2 vCell;
out vec4 o;
void main(){
    if (vLum < 0.10) discard;                        // empty background tiles vanish
    vec2 pc = gl_PointCoord;
    pc.y = 1.0 - pc.y;                                // atlas is stored top-down
    vec2 guv = (vCell + pc) / uGrid;
    float m = texture(uAtlas, guv).r;                // glyph mask (white char on black)
    if (m < 0.35) discard;
    o = vec4(vCol * (0.55 + 0.6 * vLum) * uBright, m);
}
