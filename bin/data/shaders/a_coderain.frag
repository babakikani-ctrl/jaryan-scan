#version 150

// CODE RAIN: columns of code characters streaming top->bottom (Matrix-style), with
// the visitor revealed INSIDE the rain - present cells glow in bold acid colour and
// hand motion lights the code locally. A strange data-center / engineering feeling.
uniform sampler2D uCam;
uniform sampler2D uSil;
uniform sampler2D uMotion;
uniform sampler2D uAtlas;
uniform vec2  uGridR;       // columns, rows
uniform float uTime;
uniform float uGlyphGrid;   // atlas grid dim (8)
uniform float uBright;

in  vec2 vUv;
out vec4 outColor;

vec3 pal(int i){
    vec3 P[10] = vec3[](vec3(0.05,0.05,0.06), vec3(0.35,0.08,0.70), vec3(0.12,0.27,0.92), vec3(0.88,0.12,0.18),
                        vec3(0.90,0.12,0.78), vec3(0.16,0.69,0.24), vec3(1.00,0.47,0.08), vec3(1.00,0.80,0.00),
                        vec3(0.00,0.84,0.88), vec3(0.94,0.94,0.96));
    return P[i];
}
float hash(vec2 p){ return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5453); }

void main(){
    vec2 uv = vUv;
    vec2 cell = floor(uv * uGridR);
    vec2 cuv  = (cell + 0.5) / uGridR;
    vec2 suv  = vec2(cuv.x, cuv.y);            // mirror for selfie

    // --- falling streak for this column ---
    float colh = hash(vec2(cell.x, 1.0));
    float speed = 7.0 + 16.0 * colh;                 // rows / second
    float len = 10.0 + 20.0 * hash(vec2(cell.x, 5.0));
    float head = mod(uTime * speed + colh * 137.0, uGridR.y + len);
    float dist = head - cell.y;                       // 0 at head, grows up the tail
    float streak = (dist >= 0.0 && dist < len) ? (1.0 - dist / len) : 0.0;
    float headGlow = smoothstep(1.6, 0.0, abs(dist));

    // --- visitor revealed inside the rain ---
    ivec2 cts = textureSize(uCam, 0);
    vec3 cc = texelFetch(uCam, ivec2(suv * vec2(cts)), 0).rgb;
    float lum = dot(cc, vec3(0.299, 0.587, 0.114));
    float pres = texelFetch(uSil, ivec2(suv * vec2(textureSize(uSil, 0))), 0).r;
    float person = clamp(lum * (0.35 + 0.95 * pres), 0.0, 1.0);
    float mot = texture(uMotion, suv).r; mot = smoothstep(0.05, 0.5, mot);

    // --- glyph (flickers over time) ---
    int g = int(hash(cell + floor(uTime * 11.0)) * (uGlyphGrid * uGlyphGrid - 0.001));
    vec2 gcell = vec2(float(g - (g / int(uGlyphGrid)) * int(uGlyphGrid)), float(g / int(uGlyphGrid)));
    vec2 fr = fract(uv * uGridR); fr.y = 1.0 - fr.y;
    float m = texture(uAtlas, (gcell + fr) / uGlyphGrid).r;

    // --- compose ---
    vec3 colBase = (person > 0.12) ? pal(int(clamp(person * 9.0, 1.0, 9.0)))
                                   : vec3(0.10, 0.80, 0.60);          // dim teal ambient code
    float lit = streak * 0.30
              + person * (0.35 + 0.75 * streak)
              + headGlow * 1.25
              + mot * 1.0;
    vec3 col = mix(colBase, vec3(1.0), headGlow * 0.7 + mot * 0.4);
    col *= lit * m * uBright;

    outColor = vec4(col, 1.0);
}
