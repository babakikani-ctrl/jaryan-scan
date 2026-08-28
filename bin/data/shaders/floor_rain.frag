#version 150

// FLOOR DATA.FIELD — a DENSE, full-screen field of bold coloured code/number tiles
// (Symphony-in-Acid density). Brightness waves + flickering cells give a living "data"
// feeling; the column beneath each detected subject burns bright/warm — their data.
uniform sampler2D uAtlas;
uniform vec2  uGridR;
uniform float uTime;
uniform float uGlyphGrid;
uniform float uHotX[8];
uniform int   uHotN;

in  vec2 vUv;
out vec4 outColor;

vec3 pal(int i){
    vec3 P[10] = vec3[](vec3(0.12,0.85,0.55), vec3(0.20,0.65,1.0), vec3(0.95,0.25,0.35), vec3(1.0,0.55,0.15),
                        vec3(1.0,0.82,0.15), vec3(0.75,0.30,0.95), vec3(0.15,0.9,0.9), vec3(0.95,0.35,0.7),
                        vec3(0.55,0.95,0.30), vec3(0.9,0.95,1.0));
    return P[i];
}
float hash(vec2 p){ return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5453); }

void main(){
    vec2 uv = vUv;
    vec2 cell = floor(uv * uGridR);

    int g = int(hash(cell + floor(uTime * 7.0) * 0.37) * (uGlyphGrid * uGlyphGrid - 0.001));
    vec2 gc = vec2(float(g - (g / int(uGlyphGrid)) * int(uGlyphGrid)), float(g / int(uGlyphGrid)));
    vec2 fr = fract(uv * uGridR); fr.y = 1.0 - fr.y;
    float m = texture(uAtlas, (gc + fr) / uGlyphGrid).r;

    int ci = int(hash(cell + floor(uTime * 1.5)) * 10.0);
    vec3 col = pal(ci);

    float wave  = 0.45 + 0.4 * sin(uv.y * 22.0 - uTime * 3.0 + hash(vec2(cell.x, 1.0)) * 6.2831);
    float flick = step(0.90, hash(cell + floor(uTime * 11.0)));         // random bright data cells
    float b = 0.30 + 0.55 * wave + flick * 0.6;

    float hot = 0.0;
    for (int i = 0; i < 8; i++) { if (i >= uHotN) break; hot = max(hot, smoothstep(0.05, 0.0, abs(uv.x - uHotX[i]))); }
    b += hot * 1.0;
    col = mix(col, vec3(1.0), hot * 0.45);

    // thin tile gaps -> crisp mosaic
    float gap = step(0.06, fr.x) * step(0.06, fr.y);
    outColor = vec4(col * b * max(m, 0.10) * mix(0.5, 1.0, gap), 1.0);
}
