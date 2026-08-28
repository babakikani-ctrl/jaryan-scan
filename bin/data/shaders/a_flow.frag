#version 150

// FLOW-FIELD FLUID: the visitor injects coloured "data ink" that flows along an
// animated curl field; hand motion stirs the fluid, leaving liquid streaming trails.
// A world of moving data currents.
uniform sampler2D uInk;
uniform sampler2D uCam;
uniform sampler2D uSil;
uniform sampler2D uMotion;
uniform float uTime;
uniform float uDt;
uniform float uDecay;

in  vec2 vUv;
out vec4 outColor;

vec3 pal(int i){
    vec3 P[10] = vec3[](vec3(0.05,0.05,0.06), vec3(0.35,0.08,0.70), vec3(0.12,0.27,0.92), vec3(0.88,0.12,0.18),
                        vec3(0.90,0.12,0.78), vec3(0.16,0.69,0.24), vec3(1.00,0.47,0.08), vec3(1.00,0.80,0.00),
                        vec3(0.00,0.84,0.88), vec3(0.94,0.94,0.96));
    return P[i];
}
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p){
    vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
    float a = hash(i), b = hash(i + vec2(1,0)), c = hash(i + vec2(0,1)), d = hash(i + vec2(1,1));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
vec2 curl(vec2 p){
    float e = 0.1;
    float n1 = noise(p + vec2(0.0, e)), n2 = noise(p - vec2(0.0, e));
    float n3 = noise(p + vec2(e, 0.0)), n4 = noise(p - vec2(e, 0.0));
    return vec2(n1 - n2, n4 - n3) / (2.0 * e);
}

void main(){
    vec2 uv = vUv;
    vec2 muv = vec2(uv.x, 1.0 - uv.y);   // upright
    float mot = smoothstep(0.05, 0.5, texture(uMotion, muv).r);

    vec2 fl = curl(uv * 3.0 + uTime * 0.12) * 0.5 + vec2(0.0, 0.18);   // base drift + swirl
    fl += curl(uv * 9.0 - uTime * 0.25) * mot * 2.4;                    // hands stir
    vec2 src = uv - fl * uDt;

    vec3 ink = texture(uInk, src).rgb * uDecay;

    float pres = texture(uSil, muv).r;
    vec3 cc = texture(uCam, muv).rgb;
    float lum = dot(cc, vec3(0.299, 0.587, 0.114));
    float person = clamp(lum * (0.4 + 0.9 * pres), 0.0, 1.0);
    vec3 inj = pal(int(clamp(person * 9.0, 1.0, 9.0))) * person * (0.55 + mot * 2.4);

    ink = max(ink * 1.02, inj);            // brighter, longer-lived streams
    outColor = vec4(ink, 1.0);
}
