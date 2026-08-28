#version 150
// FLOOR CASCADE — draw each falling particle as a code glyph. Colour from the acid
// palette (echoes the wall swarm); bright while falling, softly dimming into the sediment.
uniform sampler2D uState;
uniform float uPoint;
uniform float uGrid;        // glyph atlas grid dim (e.g. 8)
in  vec4 position;          // unused
in  vec2 texcoord;          // particle id
out vec3  vCol;
out float vBright;
flat out vec2 vCell;

vec3 pal(int i){
    vec3 P[10] = vec3[](vec3(0.05,0.05,0.06), vec3(0.35,0.08,0.70), vec3(0.12,0.27,0.92), vec3(0.88,0.12,0.18),
                        vec3(0.90,0.12,0.78), vec3(0.16,0.69,0.24), vec3(1.00,0.47,0.08), vec3(1.00,0.80,0.00),
                        vec3(0.00,0.84,0.88), vec3(0.94,0.94,0.96));
    return P[i];
}
float hash(vec2 p){ return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5453); }

void main(){
    vec4 s = texture(uState, texcoord);
    float x = s.r, y = s.g, life = s.b, seed = s.a;

    float band    = smoothstep(0.5, 0.98, y);              // SMOOTH dim toward the floor (no hard line)
    float fadeIn  = smoothstep(0.0, 0.5, life);
    float fadeOut = 1.0 - smoothstep(5.0, 9.5, life);
    float dim     = mix(1.0, 0.6, band);
    vBright = fadeIn * fadeOut * dim;

    int idx = 1 + int(seed * 8.99);                        // skip near-black P[0]
    vCol = pal(idx);

    int gg = int(uGrid);
    int g  = int(hash(vec2(seed * 17.0, floor(life * 2.2))) * (uGrid * uGrid - 0.001));
    vCell  = vec2(float(g - (g / gg) * gg), float(g / gg));

    gl_Position  = vec4(x * 2.0 - 1.0, 1.0 - 2.0 * y, 0.0, 1.0);   // y=0 top .. 1 bottom (falls down)
    gl_PointSize = uPoint * (0.7 + 0.6 * seed);
}
