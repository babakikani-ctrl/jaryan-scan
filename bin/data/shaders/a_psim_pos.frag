#version 150
// integrate position by velocity
uniform sampler2D uPos;
uniform sampler2D uVel;
uniform float uDt;
in  vec2 vUv;
out vec4 o;
void main(){
    vec2 pos = texture(uPos, vUv).xy + texture(uVel, vUv).xy * uDt;
    pos = clamp(pos, -0.15, 1.15);
    o = vec4(pos, 0.0, 1.0);
}
