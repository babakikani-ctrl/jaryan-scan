#version 150
in  vec3 vCol;
in  float vLum;
out vec4 o;
void main(){
    if (vLum < 0.14) discard;
    vec2 d = gl_PointCoord - 0.5;
    if (dot(d, d) > 0.25) discard;
    o = vec4(vCol * (0.6 + 0.7 * vLum), 1.0);
}
