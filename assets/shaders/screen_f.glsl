#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D screenTexture;
uniform float opacity;

void main() {
    vec3 color = texture(screenTexture, TexCoords).rgb;
    // Мы выводим цвет кадра и задаем ему прозрачность.
    // Благодаря glBlendFunc, он будет "накладываться" на старую картинку.
    FragColor = vec4(color, opacity);
}