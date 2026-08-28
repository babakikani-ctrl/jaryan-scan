#version 150
// draw the particle's code glyph from the atlas, tinted by its palette colour
uniform sampler2D uAtlas;
uniform float uGrid;
uniform float uBright;
in  vec3  vCol;
in  float vBright;
flat in vec2 vCell;
out vec4 o;
void main(){
    if (vBright < 0.02) discard;
    vec2 pc = gl_PointCoord; pc.y = 1.0 - pc.y;     // atlas stored top-down
    vec2 guv = (vCell + pc) / uGrid;
    float m = texture(uAtlas, guv).r;               // glyph mask
    if (m < 0.35) discard;
    o = vec4(vCol * (0.5 + 0.7 * vBright) * uBright, m * vBright);
}
