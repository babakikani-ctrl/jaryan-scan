#version 150

// FLOOR DATA.STREAM — a clean, professional field of NUMBERS flowing downward in aligned
// columns (data-terminal aesthetic). Full but organised. The column beneath each detected
// person lights up in its colour, and the whole field pulses with overall motion.
uniform sampler2D uAtlas;
uniform vec2  uGridR;
uniform float uTime;
uniform float uGlyphGrid;
uniform float uHotX[8];
uniform int   uHotN;
uniform float uEnergy;

in  vec2 vUv;
out vec4 outColor;

float hash(vec2 p){ return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5453); }

void main(){
    vec2 uv = vUv;
    vec2 cell = floor(uv * uGridR);

    // a changing DIGIT (0-9) — digits sit at atlas cells (d%8, d/8)
    float rate = 2.0 + hash(vec2(cell.x, 9.0)) * 4.0;
    int d = int(hash(cell + floor(uTime * rate)) * 10.0);
    vec2 gc = vec2(float(d - (d / 8) * 8), float(d / 8));
    vec2 fr = fract(uv * uGridR); fr.y = 1.0 - fr.y;
    float m = texture(uAtlas, (gc + fr) / uGlyphGrid).r;

    // downward flow per column
    float colh = hash(vec2(cell.x, 1.0));
    float speed = 5.0 + 9.0 * colh;
    float band = 0.5 + 0.5 * sin(cell.y * 0.5 - uTime * speed + colh * 6.2831);
    float base = 0.26 + 0.42 * band;                     // filled but flowing

    float hot = 0.0;
    for (int i = 0; i < 8; i++) { if (i >= uHotN) break; hot = max(hot, smoothstep(0.045, 0.0, abs(uv.x - uHotX[i]))); }

    vec3 col = vec3(0.62, 0.78, 0.86) * base;             // clean cool-white data
    col = mix(col, vec3(0.45, 0.95, 1.0) * (base + 0.5), hot);   // subject column: bright cyan
    col *= (0.85 + 0.35 * uEnergy);                      // reacts to the visitors' movement

    outColor = vec4(col * m, 1.0);
}
