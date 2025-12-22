#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    vec3 color = texture(uTexture, vUV).rgb;
    // Simple grayscale filter; replace with other kernels as needed.
    float gray = dot(color, vec3(0.299, 0.587, 0.114));
    FragColor = vec4(vec3(gray), 1.0);
}

