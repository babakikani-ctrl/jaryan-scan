#version 150

// CODE RAIN (machine style): columns of code characters streaming top->bottom; the
// detected person is revealed brighter inside the rain. Monochrome, high-contrast.
uniform sampler2D uCam;
uniform sampler2D uSil;
uniform sampler2D uAtlas;
uniform vec2  uGridR;
uniform float uTime;
uniform float uGlyphGrid;

in  vec2 vUv;
out vec4 outColor;

float hash(vec2 p){ return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5453); }

void main(){
    vec2 uv = vUv;
    vec2 cell = floor(uv * uGridR);
    vec2 cuv  = (cell + 0.5) / uGridR;
    vec2 suv  = vec2(1.0 - cuv.x, cuv.y);                    // selfie mirror

    float colh = hash(vec2(cell.x, 1.0));
    float speed = 8.0 + 18.0 * colh;
    float len   = 10.0 + 18.0 * hash(vec2(cell.x, 5.0));
    float head  = mod(uTime * speed + colh * 137.0, uGridR.y + len);
    float dist  = head - cell.y;
    float streak = (dist >= 0.0 && dist < len) ? (1.0 - dist / len) : 0.0;
    float headGlow = smoothstep(1.6, 0.0, abs(dist));

    float sil = texture(uSil, suv).r;
    vec3 cc = texture(uCam, suv).rgb;
    float lum = dot(cc, vec3(0.299, 0.587, 0.114));
    float person = clamp(lum * (0.30 + 0.95 * sil), 0.0, 1.0);

    int g = int(hash(cell + floor(uTime * 11.0)) * (uGlyphGrid * uGlyphGrid - 0.001));
    vec2 gc = vec2(float(g - (g / int(uGlyphGrid)) * int(uGlyphGrid)), float(g / int(uGlyphGrid)));
    vec2 fr = fract(uv * uGridR); fr.y = 1.0 - fr.y;
    float m = texture(uAtlas, (gc + fr) / uGlyphGrid).r;

    float colh2 = hash(vec2(cell.x, 3.0));
    vec3 codeCol = mix(vec3(0.15, 0.95, 0.55), vec3(0.25, 0.70, 1.0), colh2);     // green -> cyan columns
    float lit = streak * 0.32 + headGlow * 1.15;
    vec3 col = codeCol * lit;
    col += person * mix(vec3(1.0, 0.75, 0.35), vec3(1.0), person) * 1.5;          // person: warm -> white, bright & clear
    col = mix(col, vec3(1.0), headGlow * 0.5);

    outColor = vec4(col * m, 1.0);
}
