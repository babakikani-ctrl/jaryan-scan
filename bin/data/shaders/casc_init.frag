#version 150
// FLOOR CASCADE — particle init. Spread particles through the whole height so the
// pool fills immediately; each gets a stable random seed (glyph + colour + lane).
in vec2 vUv;
out vec4 o;
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
void main(){
    float x    = hash(vUv * 13.31 + 1.70);
    float y    = hash(vUv *  7.77 + 4.20);
    float seed = hash(vUv * 91.13 + 9.10);
    float life = hash(vUv * 23.70 + 0.30) * 6.0;
    o = vec4(x, y, life, seed);
}
