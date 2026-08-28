#version 150

// FLOOR FLUX.FIELD — a beautiful flowing energy/liquid field (domain-warped fbm) with
// luminous contour lines. Each detected person is a glowing source that lights and bends
// the flow around them. Futuristic, full, mesmerising.
uniform vec2  uRes;
uniform float uTime;
uniform float uHotX[8];
uniform float uHotY[8];
uniform int   uHotN;

in  vec2 vUv;
out vec4 outColor;

float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p){
    vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
    float a = hash(i), b = hash(i + vec2(1,0)), c = hash(i + vec2(0,1)), d = hash(i + vec2(1,1));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm(vec2 p){
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { s += a * noise(p); p *= 2.02; a *= 0.5; }
    return s;
}

void main(){
    vec2 uv = vUv;
    float asp = uRes.x / uRes.y;
    vec2 p = vec2(uv.x * asp, uv.y) * 3.2;
    float t = uTime * 0.09;

    vec2 q = vec2(fbm(p + t), fbm(p + vec2(5.2, 1.3) - t));
    vec2 r = vec2(fbm(p + 2.4 * q + vec2(1.7, 9.2) + 0.15 * t),
                  fbm(p + 2.4 * q + vec2(8.3, 2.8) - 0.12 * t));
    float f = fbm(p + 3.0 * r);

    // person sources: glow + local swirl
    float glow = 0.0;
    for (int i = 0; i < 8; i++) {
        if (i >= uHotN) break;
        float d = distance(uv * vec2(asp, 1.0), vec2(uHotX[i] * asp, uHotY[i]));
        glow += smoothstep(0.30, 0.0, d);
    }

    vec3 deep = vec3(0.02, 0.04, 0.12);
    vec3 mid  = vec3(0.05, 0.45, 0.85);
    vec3 hi   = vec3(0.25, 0.85, 1.0);
    vec3 col = mix(deep, mid, smoothstep(0.25, 0.75, f));
    col = mix(col, vec3(0.55, 0.20, 0.85), clamp(r.x * 0.8, 0.0, 1.0) * 0.5);   // magenta veins
    col = mix(col, hi, smoothstep(0.6, 1.0, f) * 0.7);

    float iso = abs(fract(f * 9.0 + t * 3.0) - 0.5);                             // luminous contour lines
    col += smoothstep(0.06, 0.0, iso) * 0.35 * vec3(0.5, 0.95, 1.0);

    col += glow * vec3(0.35, 0.85, 1.0) * 1.2;                                   // person light
    col += glow * smoothstep(0.48, 0.0, abs(fract(f * 9.0) - 0.5)) * vec3(1.0, 0.6, 0.9) * 0.6;

    outColor = vec4(col, 1.0);
}
