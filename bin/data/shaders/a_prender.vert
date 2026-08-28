#version 150

// Render the code-glyph swarm. Each tile reads its live position from the sim FBO,
// takes its bold colour from the visitor's image at its home cell, and is drawn as a
// code character (index picked from a hash) sampled from the glyph atlas in the frag.
uniform sampler2D uPos;
uniform sampler2D uCam;
uniform float uPoint;
uniform float uGrid;      // glyph atlas grid dim (e.g. 8)

in vec4 position;   // unused
in vec2 texcoord;   // tile index (its home)
out vec3 vCol;
out float vLum;
flat out vec2 vCell;      // glyph cell (col,row) in the atlas

vec3 pal(int i){
    vec3 P[10] = vec3[](vec3(0.05,0.05,0.06), vec3(0.35,0.08,0.70), vec3(0.12,0.27,0.92), vec3(0.88,0.12,0.18),
                        vec3(0.90,0.12,0.78), vec3(0.16,0.69,0.24), vec3(1.00,0.47,0.08), vec3(1.00,0.80,0.00),
                        vec3(0.00,0.84,0.88), vec3(0.94,0.94,0.96));
    return P[i];
}
float hash(vec2 p){ return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5453); }

void main(){
    vec2 p = texture(uPos, texcoord).xy;                              // live position 0..1
    vec3 cc = texture(uCam, vec2(texcoord.x, texcoord.y)).rgb;   // colour from home (mirrored)
    float lum = dot(cc, vec3(0.299, 0.587, 0.114));
    vLum = lum;

    int idx = int(clamp(lum * 9.0, 0.0, 9.0));
    vCol = pal(idx);

    int g = int(hash(texcoord) * (uGrid * uGrid - 0.001));            // glyph index
    vCell = vec2(float(g - (g / int(uGrid)) * int(uGrid)), float(g / int(uGrid)));

    gl_Position = vec4(p.x * 2.0 - 1.0, 2.0 * p.y - 1.0, 0.0, 1.0);   // upright
    gl_PointSize = uPoint;
}
