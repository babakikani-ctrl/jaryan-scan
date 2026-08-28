#version 150

// 3D POINT-CLOUD WORLD: the visitor as a rotating cloud of points floating in a data
// space above a grid horizon (LIDAR-scan feel). Depth = brightness; hands push points.
uniform mat4  modelViewProjectionMatrix;
uniform sampler2D uCam;
uniform sampler2D uSil;
uniform sampler2D uMotion;
uniform float uDepth;
uniform float uPoint;
uniform float uAspect;

in vec4 position;   // unused
in vec2 texcoord;
out vec3 vCol;
out float vLum;

vec3 pal(int i){
    vec3 P[10] = vec3[](vec3(0.05,0.05,0.06), vec3(0.35,0.08,0.70), vec3(0.12,0.27,0.92), vec3(0.88,0.12,0.18),
                        vec3(0.90,0.12,0.78), vec3(0.16,0.69,0.24), vec3(1.00,0.47,0.08), vec3(1.00,0.80,0.00),
                        vec3(0.00,0.84,0.88), vec3(0.94,0.94,0.96));
    return P[i];
}

void main(){
    vec2 mtc = vec2(1.0 - texcoord.x, texcoord.y);             // mirror content (match all other modes)
    vec3 cc = texelFetch(uCam, ivec2(mtc * vec2(textureSize(uCam, 0))), 0).rgb;
    float lum = dot(cc, vec3(0.299, 0.587, 0.114));
    float pres = texelFetch(uSil, ivec2(mtc * vec2(textureSize(uSil, 0))), 0).r;
    lum = clamp(lum * (0.40 + 0.95 * pres), 0.0, 1.0);
    float mot = texelFetch(uMotion, ivec2(mtc * vec2(textureSize(uMotion, 0))), 0).r;
    mot = smoothstep(0.05, 0.5, mot);
    vLum = lum;

    int idx = int(clamp(lum * 9.0, 0.0, 9.0));
    vCol = mix(pal(idx), vec3(1.0), mot * 0.55);

    float u = 1.0 - texcoord.x, v = texcoord.y;                 // mirrored position
    vec3 p = vec3((u - 0.5) * uAspect * 2.0,
                  (v - 0.5) * 2.0,                              // upright (was upside-down)
                  lum * uDepth + mot * 0.7);                    // pop toward camera + hand push
    gl_Position = modelViewProjectionMatrix * vec4(p, 1.0);
    gl_PointSize = uPoint * (0.45 + lum);
}
