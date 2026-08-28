#version 150

// Velocity integration for the interactive particle cloud.
// Each particle springs toward its home cell; the visitor's HAND MOTION injects
// swirling curl-noise turbulence, so moving your hands scatters and swirls the
// cloud, then it snaps back and reforms the image.
uniform sampler2D uPos;
uniform sampler2D uVel;
uniform sampler2D uMotion;
uniform float uTime;
uniform float uDt;
uniform float uPush;
uniform float uSpring;
uniform float uDamp;

in  vec2 vUv;
out vec4 o;

float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p){
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
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
    vec2 home = vUv;
    vec2 pos  = texture(uPos, vUv).xy;
    vec2 vel  = texture(uVel, vUv).xy;

    float mot = texture(uMotion, vec2(pos.x, pos.y)).r;    // hand motion at this particle
    mot = smoothstep(0.05, 0.5, mot);

    vec2 swirl  = curl(pos * 8.0 + uTime * 0.4) * mot * uPush;   // hands -> turbulence
    vec2 spring = (home - pos) * uSpring;                        // pull back home -> reforms image

    vec2 vN = (vel + (swirl + spring) * uDt) * uDamp;
    o = vec4(vN, 0.0, 1.0);
}
