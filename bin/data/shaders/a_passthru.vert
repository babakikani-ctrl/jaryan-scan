#version 150
// full-screen NDC quad for GPGPU sim passes (bypasses matrices)
in vec4 position;
in vec2 texcoord;
out vec2 vUv;
void main(){
    vUv = texcoord;
    gl_Position = position;
}
