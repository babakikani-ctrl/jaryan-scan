#version 150
// particle init: position = home (its own grid cell), velocity = 0
in vec2 vUv;
out vec4 o;
void main(){
    o = vec4(vUv, 0.0, 1.0);
}
