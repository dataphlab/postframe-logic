#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform float opacity;
uniform vec2 u_resolution;
uniform int u_useDenoise;

// Настройки денозера
const float SIGMA = 5.0;      // Радиус размытия
const float BSIGMA = 0.2;    // Порог сохранения границ (чем меньше, тем резче края)
const float M_SIZE = 5.0;     // Размер ядра

float normpdf(float x, float s) {
    return 0.39894 * exp(-0.5 * x * x / (s * s)) / s;
}

void main() {
    vec3 centerColor = texture(screenTexture, TexCoords).rgb;
    
    if (u_useDenoise == 0) {
        FragColor = vec4(centerColor, opacity);
        return;
    }
    
    vec3 final_colour = vec3(0.0);
    float Z = 0.0;
    
    float kernel[11];
    for (int j = 0; j <= int(M_SIZE); ++j) {
        kernel[int(M_SIZE) + j] = kernel[int(M_SIZE) - j] = normpdf(float(j), SIGMA);
    }
    
    float bsigma2 = BSIGMA * BSIGMA;
    vec2 invRes = 1.0 / u_resolution;

    // Проход по соседним пикселям
    for (int i = -int(M_SIZE); i <= int(M_SIZE); ++i) {
        for (int j = -int(M_SIZE); j <= int(M_SIZE); ++j) {
            vec2 offset = vec2(float(i), float(j)) * invRes;
            vec3 sampleColor = texture(screenTexture, TexCoords + offset).rgb;
            
            // Математика биллятерального фильтра
            float factor = kernel[int(M_SIZE) + j] * kernel[int(M_SIZE) + i];
            vec3 diff = sampleColor - centerColor;
            float dist2 = dot(diff, diff);
            float bWeight = exp(-(dist2) / (2.0 * bsigma2));
            
            float weight = factor * bWeight;
            Z += weight;
            final_colour += weight * sampleColor;
        }
    }

    vec3 denoised = final_colour / Z;
    
    FragColor = vec4(denoised, opacity);
}