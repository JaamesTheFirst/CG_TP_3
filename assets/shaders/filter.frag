#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform int uMode;   // 0 -> original , 1 -> mean filter applied
uniform int uRadius; // kernel radius for mean filter

// 3x3 mean filter
void main() {
    vec3 color;
    if (uMode == 0) {
        color = texture(uTexture, vUV).rgb; // original
    } else {
        // Clamp radius to avoid very large loops; supports up to 5.
        int r = clamp(uRadius, 1, 50);
        int kernelSize = 2 * r + 1;
        float invCount = 1.0 / float(kernelSize * kernelSize);
        vec2 texel = 1.0 / vec2(textureSize(uTexture, 0));
        vec3 sum = vec3(0.0);
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                sum += texture(uTexture, vUV + vec2(dx, dy) * texel).rgb;
            }
        }
        color = sum * invCount;
    }
    FragColor = vec4(color, 1.0);
}

