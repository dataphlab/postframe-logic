#include "themes.h"
#include <iostream>

namespace theme {
    void defaultTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // === Основные параметры стиля ===
        style.WindowRounding    = 4.0f;   // Лёгкое скругление окон
        style.FrameRounding     = 3.0f;   // Скругление элементов ввода/кнопок
        style.PopupRounding     = 4.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding      = 3.0f;

        style.WindowPadding     = ImVec2(10, 10);
        style.FramePadding      = ImVec2(6, 4);
        style.ItemSpacing       = ImVec2(8, 6);
        style.ItemInnerSpacing  = ImVec2(6, 6);

        // === Цветовая палитра (строго чёрно-белая с оттенками серого) ===
        // Фон
        colors[ImGuiCol_WindowBg]          = ImVec4(0.08f, 0.08f, 0.08f, 1.00f); // Почти чёрный (#141414)
        colors[ImGuiCol_PopupBg]           = ImVec4(0.10f, 0.10f, 0.10f, 0.98f); // Чуть светлее фона
        colors[ImGuiCol_ChildBg]           = ImVec4(0.07f, 0.07f, 0.07f, 1.00f); // Для дочерних окон

        // Заголовки
        colors[ImGuiCol_TitleBg]           = ImVec4(0.12f, 0.12f, 0.12f, 1.00f); // Тёмно-серый (#1F1F1F)
        colors[ImGuiCol_TitleBgActive]     = ImVec4(0.18f, 0.18f, 0.18f, 1.00f); // Активный заголовок
        colors[ImGuiCol_TitleBgCollapsed]  = ImVec4(0.08f, 0.08f, 0.08f, 1.00f); // Свёрнутый

        // Кнопки
        colors[ImGuiCol_Button]            = ImVec4(0.20f, 0.20f, 0.20f, 1.00f); // Нейтральный серый (#333333)
        colors[ImGuiCol_ButtonHovered]     = ImVec4(0.28f, 0.28f, 0.28f, 1.00f); // Светлее при наведении
        colors[ImGuiCol_ButtonActive]      = ImVec4(0.35f, 0.35f, 0.35f, 1.00f); // Ещё светлее при нажатии

        // Рамки и поля ввода
        colors[ImGuiCol_Border]            = ImVec4(0.30f, 0.30f, 0.30f, 1.00f); // Серебристый (#4D4D4D)
        colors[ImGuiCol_FrameBg]           = ImVec4(0.15f, 0.15f, 0.15f, 1.00f); // Фон полей ввода
        colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_FrameBgActive]     = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

        // Текст
        colors[ImGuiCol_Text]              = ImVec4(0.95f, 0.95f, 0.95f, 1.00f); // Почти белый (#F2F2F2)
        colors[ImGuiCol_TextDisabled]      = ImVec4(0.50f, 0.50f, 0.50f, 1.00f); // Серый текст

        // Меню и выделение
        colors[ImGuiCol_MenuBarBg]         = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_Header]            = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_HeaderHovered]     = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_HeaderActive]      = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

        // Скроллбар
        colors[ImGuiCol_ScrollbarBg]       = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab]     = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

        // Чекбоксы и радио
        colors[ImGuiCol_CheckMark]         = ImVec4(0.95f, 0.95f, 0.95f, 1.00f); // Белая галочка

        // Сепараторы
        colors[ImGuiCol_Separator]         = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_SeparatorHovered]  = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
        colors[ImGuiCol_SeparatorActive]   = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);

        // Ресайз-хендлы
        colors[ImGuiCol_ResizeGrip]        = ImVec4(0.30f, 0.30f, 0.30f, 0.30f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.45f, 0.45f, 0.45f, 0.60f);
        colors[ImGuiCol_ResizeGripActive]  = ImVec4(0.60f, 0.60f, 0.60f, 0.90f);
    }
}