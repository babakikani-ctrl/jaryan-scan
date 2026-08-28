#version 150

// Renders a detected person's cropped camera region as either a grayscale surveillance
// crop-card (uGray=1) or a saturated R/G/B segmentation VOLUME (uGray=0), pixelated.
uniform sampler2D uCam;
uniform sampler2D uSil;
uniform vec2  uUvMin;     // cam-space uv of the person's box (already mirrored)
uniform vec2  uUvMax;
uniform vec2  uCells;     // pixelation grid
uniform vec3  uTint;
uniform float uGray;      // 1 = grayscale crop, 0 = tinted volume
uniform float uScan;      // scanline amount

in  vec2 vUv;
out vec4 outColor;

void main(){
    vec2 luv = vUv;
    vec2 pc  = (floor(luv * uCells) + 0.5) / uCells;     // pixelate
    vec2 cuv = mix(uUvMin, uUvMax, pc);
    vec3 cc  = texture(uCam, cuv).rgb;
    float sil = texture(uSil, cuv).r;
    float lum = dot(cc, vec3(0.299, 0.587, 0.114));

    vec3 col;
    if (uGray > 0.5) {
        col = vec3(0.08 + 0.9 * lum);                    // grayscale CCTV crop
    } else {
        col = uTint * (0.14 + 0.30 * lum) + uTint * sil * (0.7 + 0.9 * lum);   // tinted volume
        col += sil * vec3(0.05);                         // faint edge lift
    }
    col *= 1.0 - uScan * 0.5 * step(0.5, fract(luv.y * uCells.y));   // scanline texture

    outColor = vec4(col, 1.0);
}
