#include "Menu.hpp"

#include "AppState.hpp"
#include "Config.hpp"

#include "imgui.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>

namespace Menu {
    namespace {
        struct Palette {
            ImVec4 Accent, AccentAlt, AccentSoft, Background, Sidebar, Surface, SurfaceRaised;
            ImVec4 Border, Text, Muted, Good, Warning, Danger;
        };

        enum class NavIcon { Overview, Character, Career, Stream, Movement, System };

        constexpr float SidebarWidth = 218.0f;
        constexpr float HeaderHeight = 88.0f;
        constexpr ImVec2 DefaultWindowSize{1020.0f, 660.0f};
        constexpr std::array<const char*, 6> PageTitles{
            "Kontrol Merkezi", "Karakter", "Kariyer", "Yayin", "Hareket", "Sistem"
        };
        constexpr std::array<const char*, 6> PageDescriptions{
            "Oyuncu verileri ve etkin moduller tek bakista",
            "Hayatta kalma ve ihtiyac degerlerini yonet",
            "Butce, kitle ve ilerlemeyi hizlandir",
            "Yayin gorunurlugunu gercek zamanli destekle",
            "Temel yurume hizini guvenli bicimde carp",
            "Gorunum, profil ve DLL kontrolleri"
        };

        ImFont* BodyFont = nullptr;
        ImFont* MediumFont = nullptr;
        ImFont* DisplayFont = nullptr;
        Palette Colors{};
        int Page = 0;
        int PreviousPage = 0;
        float PageTransition = 1.0f;
        bool MotionReduced = false;
        std::unordered_map<ImGuiID, float> Animations;
        std::string Notice;
        float MoneyDelta = 10000.0f;
        int FollowersDelta = 1000;
        int SubscribersDelta = 100;
        int ExperienceDelta = 1000;
        int SkillDelta = 10;

        ImVec4 Alpha(ImVec4 color, float alpha) {
            color.w *= alpha;
            return color;
        }

        ImVec4 Mix(const ImVec4& a, const ImVec4& b, float amount) {
            amount = std::clamp(amount, 0.0f, 1.0f);
            return ImVec4(a.x + (b.x - a.x) * amount, a.y + (b.y - a.y) * amount,
                a.z + (b.z - a.z) * amount, a.w + (b.w - a.w) * amount);
        }

        Palette MakePalette(int theme) {
            Palette palette{
                ImVec4(0.49f, 0.39f, 0.98f, 1.0f), ImVec4(0.16f, 0.76f, 0.94f, 1.0f),
                ImVec4(0.16f, 0.13f, 0.31f, 1.0f), ImVec4(0.018f, 0.022f, 0.037f, 0.985f),
                ImVec4(0.026f, 0.031f, 0.050f, 1.0f), ImVec4(0.044f, 0.052f, 0.078f, 0.96f),
                ImVec4(0.065f, 0.075f, 0.108f, 0.98f), ImVec4(0.145f, 0.164f, 0.218f, 0.90f),
                ImVec4(0.955f, 0.965f, 0.985f, 1.0f), ImVec4(0.60f, 0.64f, 0.72f, 1.0f),
                ImVec4(0.22f, 0.82f, 0.55f, 1.0f), ImVec4(0.96f, 0.70f, 0.26f, 1.0f),
                ImVec4(0.96f, 0.31f, 0.38f, 1.0f)
            };
            if (theme == 1) {
                palette.Accent = ImVec4(0.10f, 0.76f, 0.88f, 1.0f);
                palette.AccentAlt = ImVec4(0.26f, 0.88f, 0.58f, 1.0f);
                palette.AccentSoft = ImVec4(0.07f, 0.22f, 0.27f, 1.0f);
            } else if (theme == 2) {
                palette.Accent = ImVec4(0.91f, 0.66f, 0.27f, 1.0f);
                palette.AccentAlt = ImVec4(0.94f, 0.43f, 0.31f, 1.0f);
                palette.AccentSoft = ImVec4(0.28f, 0.19f, 0.08f, 1.0f);
            }
            return palette;
        }

        float Animate(ImGuiID id, float target, float speed = 12.0f) {
            auto [iterator, inserted] = Animations.emplace(id, target);
            if (inserted || MotionReduced) {
                iterator->second = target;
                return target;
            }
            const float step = 1.0f - std::exp(-speed * std::min(ImGui::GetIO().DeltaTime, 0.05f));
            iterator->second += (target - iterator->second) * step;
            if (std::abs(iterator->second - target) < 0.001f) iterator->second = target;
            return iterator->second;
        }

        void PushFont(ImFont* font) { if (font) ImGui::PushFont(font, font->LegacySize); }
        void PopFont(ImFont* font) { if (font) ImGui::PopFont(); }

        std::string Narrow(const std::wstring& wide) {
            if (wide.empty()) return {};
            const auto length = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
            std::string output(static_cast<size_t>(length), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), output.data(), length, nullptr, nullptr);
            return output;
        }

        void ApplyPalette() {
            auto& style = ImGui::GetStyle();
            style.Colors[ImGuiCol_Text] = Colors.Text;
            style.Colors[ImGuiCol_TextDisabled] = Colors.Muted;
            style.Colors[ImGuiCol_WindowBg] = Colors.Background;
            style.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
            style.Colors[ImGuiCol_PopupBg] = Colors.SurfaceRaised;
            style.Colors[ImGuiCol_Border] = Colors.Border;
            style.Colors[ImGuiCol_FrameBg] = Colors.Surface;
            style.Colors[ImGuiCol_FrameBgHovered] = Mix(Colors.SurfaceRaised, Colors.AccentSoft, 0.24f);
            style.Colors[ImGuiCol_FrameBgActive] = Mix(Colors.SurfaceRaised, Colors.AccentSoft, 0.42f);
            style.Colors[ImGuiCol_CheckMark] = Colors.Accent;
            style.Colors[ImGuiCol_SliderGrab] = Colors.Accent;
            style.Colors[ImGuiCol_SliderGrabActive] = Colors.AccentAlt;
            style.Colors[ImGuiCol_Button] = Colors.SurfaceRaised;
            style.Colors[ImGuiCol_ButtonHovered] = Mix(Colors.SurfaceRaised, Colors.Accent, 0.18f);
            style.Colors[ImGuiCol_ButtonActive] = Mix(Colors.SurfaceRaised, Colors.Accent, 0.30f);
            style.Colors[ImGuiCol_Header] = Colors.AccentSoft;
            style.Colors[ImGuiCol_HeaderHovered] = Mix(Colors.AccentSoft, Colors.Accent, 0.16f);
            style.Colors[ImGuiCol_HeaderActive] = Mix(Colors.AccentSoft, Colors.Accent, 0.25f);
            style.Colors[ImGuiCol_Separator] = Colors.Border;
            style.Colors[ImGuiCol_ScrollbarBg] = Alpha(Colors.Background, 0.0f);
            style.Colors[ImGuiCol_ScrollbarGrab] = Alpha(Colors.Border, 0.75f);
            style.Colors[ImGuiCol_ScrollbarGrabHovered] = Alpha(Colors.Accent, 0.55f);
            style.Colors[ImGuiCol_ScrollbarGrabActive] = Alpha(Colors.Accent, 0.75f);
            style.Colors[ImGuiCol_NavCursor] = Colors.Accent;
        }

        void DrawNavIcon(ImDrawList* draw, NavIcon icon, ImVec2 center, ImU32 color) {
            constexpr float stroke = 1.65f;
            switch (icon) {
                case NavIcon::Overview:
                    for (int y = -1; y <= 1; y += 2) for (int x = -1; x <= 1; x += 2)
                        draw->AddRect(ImVec2(center.x + x * 5.8f - 3.8f, center.y + y * 5.8f - 3.8f),
                            ImVec2(center.x + x * 5.8f + 3.8f, center.y + y * 5.8f + 3.8f), color, 2.0f, stroke, 0);
                    break;
                case NavIcon::Character:
                    draw->AddCircle(ImVec2(center.x, center.y - 5.0f), 4.2f, color, 18, stroke);
                    draw->AddBezierCubic(ImVec2(center.x - 8.0f, center.y + 8.0f), ImVec2(center.x - 6.0f, center.y + 1.0f),
                        ImVec2(center.x + 6.0f, center.y + 1.0f), ImVec2(center.x + 8.0f, center.y + 8.0f), color, stroke);
                    break;
                case NavIcon::Career:
                    draw->AddLine(ImVec2(center.x - 8.0f, center.y + 7.0f), ImVec2(center.x - 2.0f, center.y + 1.0f), color, stroke);
                    draw->AddLine(ImVec2(center.x - 2.0f, center.y + 1.0f), ImVec2(center.x + 2.0f, center.y + 4.0f), color, stroke);
                    draw->AddLine(ImVec2(center.x + 2.0f, center.y + 4.0f), ImVec2(center.x + 8.0f, center.y - 6.0f), color, stroke);
                    draw->AddLine(ImVec2(center.x + 3.0f, center.y - 6.0f), ImVec2(center.x + 8.0f, center.y - 6.0f), color, stroke);
                    draw->AddLine(ImVec2(center.x + 8.0f, center.y - 6.0f), ImVec2(center.x + 8.0f, center.y - 1.0f), color, stroke);
                    break;
                case NavIcon::Stream:
                    draw->AddRect(ImVec2(center.x - 9.0f, center.y - 7.0f), ImVec2(center.x + 9.0f, center.y + 7.0f), color, 4.0f, stroke, 0);
                    draw->AddTriangleFilled(ImVec2(center.x - 2.0f, center.y - 3.5f), ImVec2(center.x - 2.0f, center.y + 3.5f),
                        ImVec2(center.x + 4.5f, center.y), color);
                    break;
                case NavIcon::Movement:
                    draw->AddLine(ImVec2(center.x - 8.0f, center.y), ImVec2(center.x + 8.0f, center.y), color, stroke);
                    draw->AddLine(ImVec2(center.x - 8.0f, center.y), ImVec2(center.x - 3.0f, center.y - 5.0f), color, stroke);
                    draw->AddLine(ImVec2(center.x - 8.0f, center.y), ImVec2(center.x - 3.0f, center.y + 5.0f), color, stroke);
                    draw->AddLine(ImVec2(center.x + 8.0f, center.y), ImVec2(center.x + 3.0f, center.y - 5.0f), color, stroke);
                    draw->AddLine(ImVec2(center.x + 8.0f, center.y), ImVec2(center.x + 3.0f, center.y + 5.0f), color, stroke);
                    break;
                case NavIcon::System:
                    draw->AddCircle(center, 7.0f, color, 20, stroke);
                    draw->AddCircle(center, 2.5f, color, 16, stroke);
                    for (int index = 0; index < 4; ++index) {
                        const float angle = 0.7854f + index * 1.5708f;
                        draw->AddLine(ImVec2(center.x + std::cos(angle) * 8.0f, center.y + std::sin(angle) * 8.0f),
                            ImVec2(center.x + std::cos(angle) * 10.5f, center.y + std::sin(angle) * 10.5f), color, stroke);
                    }
                    break;
            }
        }

        bool NavigationItem(const char* label, NavIcon icon, int index) {
            ImGui::PushID(index);
            const bool selected = Page == index;
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const ImVec2 size(ImGui::GetContentRegionAvail().x, 46.0f);
            ImGui::InvisibleButton("##nav", size);
            const bool hovered = ImGui::IsItemHovered();
            const bool focused = ImGui::IsItemFocused();
            if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            const float state = Animate(ImGui::GetID("nav-state"), selected ? 1.0f : (hovered ? 0.38f : 0.0f), 14.0f);
            auto* draw = ImGui::GetWindowDrawList();
            if (state > 0.01f) draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                ImGui::GetColorU32(Alpha(Mix(Colors.Surface, Colors.AccentSoft, selected ? 0.72f : 0.32f), state)), 10.0f);
            if (selected) draw->AddRectFilled(ImVec2(origin.x, origin.y + 10.0f), ImVec2(origin.x + 3.0f, origin.y + size.y - 10.0f),
                ImGui::GetColorU32(Colors.Accent), 2.0f);
            if (focused) draw->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), ImGui::GetColorU32(Colors.AccentAlt), 10.0f, 1.0f, 0);
            const ImVec4 foreground = selected ? Colors.Text : Mix(Colors.Muted, Colors.Text, state * 0.62f);
            DrawNavIcon(draw, icon, ImVec2(origin.x + 23.0f, origin.y + 23.0f), ImGui::GetColorU32(selected ? Colors.Accent : foreground));
            draw->AddText(MediumFont ? MediumFont : ImGui::GetFont(), MediumFont ? MediumFont->LegacySize : ImGui::GetFontSize(),
                ImVec2(origin.x + 46.0f, origin.y + 13.0f), ImGui::GetColorU32(foreground), label);
            const bool clicked = ImGui::IsItemClicked();
            if (clicked) Page = index;
            ImGui::PopID();
            return clicked;
        }

        bool ActionButton(const char* id, const char* label, ImVec2 size, int tone = 0) {
            ImGui::PushID(id);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##action", size);
            const bool hovered = ImGui::IsItemHovered();
            const bool active = ImGui::IsItemActive();
            const bool focused = ImGui::IsItemFocused();
            if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            const float hover = Animate(ImGui::GetID("hover"), hovered ? 1.0f : 0.0f, 16.0f);
            ImVec4 base = tone == 1 ? Colors.Danger : (tone == 2 ? Colors.SurfaceRaised : Colors.Accent);
            ImVec4 highlight = tone == 1 ? Mix(Colors.Danger, Colors.Text, 0.13f) :
                (tone == 2 ? Mix(Colors.SurfaceRaised, Colors.Accent, 0.22f) : Colors.AccentAlt);
            ImVec4 background = Mix(base, highlight, hover * 0.58f);
            if (active) background = Mix(background, Colors.Background, 0.18f);
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), ImGui::GetColorU32(background), 9.0f);
            if (tone == 2 || focused) draw->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                ImGui::GetColorU32(focused ? Colors.AccentAlt : Colors.Border), 9.0f, 1.0f, 0);
            const ImVec2 textSize = MediumFont ? MediumFont->CalcTextSizeA(MediumFont->LegacySize, FLT_MAX, 0.0f, label) : ImGui::CalcTextSize(label);
            draw->AddText(MediumFont ? MediumFont : ImGui::GetFont(), MediumFont ? MediumFont->LegacySize : ImGui::GetFontSize(),
                ImVec2(origin.x + (size.x - textSize.x) * 0.5f, origin.y + (size.y - textSize.y) * 0.5f - 1.0f),
                ImGui::GetColorU32(Colors.Text), label);
            const bool clicked = ImGui::IsItemClicked();
            ImGui::PopID();
            return clicked;
        }

        bool ToggleLine(const char* id, const char* title, const char* detail, bool& value) {
            ImGui::PushID(id);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            ImGui::InvisibleButton("##toggle-line", ImVec2(width, 54.0f));
            const bool hovered = ImGui::IsItemHovered();
            const bool focused = ImGui::IsItemFocused();
            if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (ImGui::IsItemClicked()) value = !value;
            const float state = Animate(ImGui::GetID("state"), value ? 1.0f : 0.0f, 15.0f);
            const float hover = Animate(ImGui::GetID("hover"), hovered ? 1.0f : 0.0f, 18.0f);
            auto* draw = ImGui::GetWindowDrawList();
            if (hover > 0.01f) draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + 54.0f),
                ImGui::GetColorU32(Alpha(Colors.SurfaceRaised, hover * 0.55f)), 8.0f);
            draw->AddText(MediumFont ? MediumFont : ImGui::GetFont(), MediumFont ? MediumFont->LegacySize : ImGui::GetFontSize(),
                ImVec2(origin.x + 2.0f, origin.y + 4.0f), ImGui::GetColorU32(Colors.Text), title);
            draw->AddText(BodyFont ? BodyFont : ImGui::GetFont(), BodyFont ? BodyFont->LegacySize - 2.0f : ImGui::GetFontSize() - 2.0f,
                ImVec2(origin.x + 2.0f, origin.y + 29.0f), ImGui::GetColorU32(Colors.Muted), detail);
            const ImVec2 switchMin(origin.x + width - 48.0f, origin.y + 13.0f);
            const ImVec2 switchMax(switchMin.x + 46.0f, switchMin.y + 26.0f);
            draw->AddRectFilled(switchMin, switchMax, ImGui::GetColorU32(Mix(Colors.Border, Colors.Accent, state)), 13.0f);
            draw->AddCircleFilled(ImVec2(switchMin.x + 13.0f + state * 20.0f, switchMin.y + 13.0f), 9.0f,
                ImGui::GetColorU32(Colors.Text), 24);
            if (focused) draw->AddRect(ImVec2(switchMin.x - 3.0f, switchMin.y - 3.0f), ImVec2(switchMax.x + 3.0f, switchMax.y + 3.0f),
                ImGui::GetColorU32(Colors.AccentAlt), 16.0f, 1.0f, 0);
            ImGui::PopID();
            return value;
        }

        void BeginCard(const char* id, float height) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Colors.Surface);
            ImGui::PushStyleColor(ImGuiCol_Border, Colors.Border);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 13.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
            ImGui::BeginChild(id, ImVec2(0.0f, height), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(2);
        }

        void EndCard() { ImGui::EndChild(); }

        void SectionLabel(const char* title, const char* detail = nullptr) {
            PushFont(MediumFont);
            ImGui::TextColored(Colors.Text, "%s", title);
            PopFont(MediumFont);
            if (detail) { ImGui::SameLine(); ImGui::TextColored(Colors.Muted, "%s", detail); }
            ImGui::Dummy(ImVec2(0.0f, 3.0f));
        }

        void ValueSliderFloat(const char* id, const char* label, float& value, float minimum, float maximum, const char* format, bool percentage = false) {
            ImGui::PushID(id);
            const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
            ImGui::TextColored(Colors.Muted, "%s", label);
            char text[48]{};
            std::snprintf(text, sizeof(text), format, percentage ? value * 100.0f : value);
            const float width = ImGui::CalcTextSize(text).x;
            ImGui::SameLine(right - width);
            ImGui::TextColored(Colors.AccentAlt, "%s", text);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, Colors.Background);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Mix(Colors.Background, Colors.AccentSoft, 0.45f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Mix(Colors.Background, Colors.AccentSoft, 0.65f));
            ImGui::SliderFloat("##value", &value, minimum, maximum, "", ImGuiSliderFlags_AlwaysClamp);
            ImGui::PopStyleColor(3);
            ImGui::PopID();
        }

        void ValueSliderInt(const char* id, const char* label, int& value, int minimum, int maximum) {
            ImGui::PushID(id);
            const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
            ImGui::TextColored(Colors.Muted, "%s", label);
            char text[48]{};
            std::snprintf(text, sizeof(text), "%d", value);
            const float width = ImGui::CalcTextSize(text).x;
            ImGui::SameLine(right - width);
            ImGui::TextColored(Colors.AccentAlt, "%s", text);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, Colors.Background);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Mix(Colors.Background, Colors.AccentSoft, 0.45f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Mix(Colors.Background, Colors.AccentSoft, 0.65f));
            ImGui::SliderInt("##value", &value, minimum, maximum, "", ImGuiSliderFlags_AlwaysClamp);
            ImGui::PopStyleColor(3);
            ImGui::PopID();
        }

        void StatCard(const char* id, const char* label, const char* value, const ImVec4& accent) {
            BeginCard(id, 94.0f);
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(cursor.x + 4.0f, cursor.y + 7.0f), 3.5f,
                ImGui::GetColorU32(accent), 16);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15.0f);
            ImGui::TextColored(Colors.Muted, "%s", label);
            PushFont(DisplayFont);
            ImGui::TextColored(Colors.Text, "%s", value);
            PopFont(DisplayFont);
            EndCard();
        }

        int ActiveFeatureCount(const AppState::Settings& settings) {
            return static_cast<int>(settings.LockMoney) + static_cast<int>(settings.LockHealth) +
                static_cast<int>(settings.LockEnergy) + static_cast<int>(settings.LockNeed) +
                static_cast<int>(settings.LockHappiness) + static_cast<int>(settings.LockSkillPoints) +
                static_cast<int>(settings.StreamBoost) + static_cast<int>(settings.SpeedEnabled);
        }

        void DrawProgress(const char* label, float value, const ImVec4& color) {
            value = std::clamp(value, 0.0f, 1.0f);
            ImGui::TextColored(Colors.Muted, "%s", label);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + 7.0f), ImGui::GetColorU32(Colors.Background), 3.5f);
            draw->AddRectFilled(origin, ImVec2(origin.x + width * value, origin.y + 7.0f), ImGui::GetColorU32(color), 3.5f);
            ImGui::Dummy(ImVec2(0.0f, 11.0f));
        }

        void Commit(const AppState::Settings& settings) {
            auto normalized = settings;
            AppState::Normalize(normalized);
            std::scoped_lock lock(AppState::ModelMutex);
            AppState::Options = normalized;
        }

        void DrawOverview(const AppState::Settings& settings, const AppState::Snapshot& live) {
            SectionLabel("Canli istatistikler", "oyun verisinden anlik okunur");
            char money[64]{}, followers[64]{}, subscribers[64]{}, health[64]{}, energy[64]{}, level[64]{};
            std::snprintf(money, sizeof(money), "%.0f", live.Money);
            std::snprintf(followers, sizeof(followers), "%d", live.Followers);
            std::snprintf(subscribers, sizeof(subscribers), "%d", live.Subscribers);
            std::snprintf(health, sizeof(health), "%.0f%%", live.Health * 100.0f);
            std::snprintf(energy, sizeof(energy), "%.0f%%", live.Energy * 100.0f);
            std::snprintf(level, sizeof(level), "%d", live.Level);
            if (ImGui::BeginTable("overview-stats", 3, ImGuiTableFlags_SizingStretchSame)) {
                const std::array<const char*, 6> ids{"money", "followers", "subscribers", "health", "energy", "level"};
                const std::array<const char*, 6> labels{"PARA", "TAKIPCI", "ABONE", "SAGLIK", "ENERJI", "SEVIYE"};
                const std::array<const char*, 6> values{money, followers, subscribers, health, energy, level};
                const std::array<ImVec4, 6> accents{Colors.Accent, Colors.AccentAlt, Colors.Warning, Colors.Good, Colors.AccentAlt, Colors.Accent};
                for (int index = 0; index < 6; ++index) { ImGui::TableNextColumn(); StatCard(ids[index], labels[index], values[index], accents[index]); }
                ImGui::EndTable();
            }
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            if (ImGui::BeginTable("overview-detail", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextColumn();
                BeginCard("connection", 150.0f);
                SectionLabel("Baglanti durumu");
                const ImVec4 statusColor = live.PlayerReady ? Colors.Good : Colors.Warning;
                const ImVec2 dot = ImGui::GetCursorScreenPos();
                const float pulse = MotionReduced ? 1.0f : 0.78f + std::sin(static_cast<float>(ImGui::GetTime()) * 3.0f) * 0.16f;
                ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(dot.x + 5.0f, dot.y + 9.0f), 5.0f,
                    ImGui::GetColorU32(Alpha(statusColor, pulse)), 20);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 18.0f);
                PushFont(MediumFont); ImGui::TextColored(statusColor, "%s", live.Status.c_str()); PopFont(MediumFont);
                ImGui::TextWrapped("Internal baglanti aktifken degerler otomatik yenilenir. Menuyu Insert ile gizleyebilirsin.");
                EndCard();
                ImGui::TableNextColumn();
                BeginCard("module-health", 150.0f);
                SectionLabel("Etkin moduller");
                PushFont(DisplayFont); ImGui::TextColored(Colors.Accent, "%d / 8", ActiveFeatureCount(settings)); PopFont(DisplayFont);
                DrawProgress("Saglik", live.Health, Colors.Good);
                DrawProgress("Enerji", live.Energy, Colors.AccentAlt);
                EndCard();
                ImGui::EndTable();
            }
        }

        void DrawCharacter(AppState::Settings& settings, const AppState::Snapshot& live) {
            SectionLabel("Karakter modulleri", "her kart ayri ayarlanabilir");
            if (ImGui::BeginTable("character-grid", 2, ImGuiTableFlags_SizingStretchSame)) {
                struct Item { const char* id; const char* title; const char* detail; bool* enabled; float* target; float live; };
                const std::array<Item, 4> items{{
                    {"health", "Sinirsiz saglik", "Sagligi secilen hedefte tutar", &settings.LockHealth, &settings.HealthTarget, live.Health},
                    {"energy", "Sinirsiz enerji", "Enerji dususunu aninda tamamlar", &settings.LockEnergy, &settings.EnergyTarget, live.Energy},
                    {"need", "Ihtiyaclari sabitle", "Temel ihtiyac seviyesini korur", &settings.LockNeed, &settings.NeedTarget, live.Need},
                    {"happiness", "Mutlulugu sabitle", "Mutlulugu secilen hedefte tutar", &settings.LockHappiness, &settings.HappinessTarget, live.Happiness}
                }};
                for (const auto& item : items) {
                    ImGui::TableNextColumn();
                    ImGui::PushID(item.id);
                    BeginCard("card", 142.0f);
                    ToggleLine("toggle", item.title, item.detail, *item.enabled);
                    char liveLabel[48]{};
                    std::snprintf(liveLabel, sizeof(liveLabel), "Hedef  |  canli %.0f%%", item.live * 100.0f);
                    ValueSliderFloat("target", liveLabel, *item.target, 0.0f, 1.0f, "%.0f%%", true);
                    EndCard();
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        void DrawCareer(AppState::Settings& settings, const AppState::Snapshot& live) {
            SectionLabel("Kariyer araclari", "degisiklikler oyuna aninda uygulanir");
            if (ImGui::BeginTable("career-top", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextColumn();
                BeginCard("money-card", 205.0f);
                ToggleLine("money-lock", "Parayi sabitle", "Bakiyeyi hedef degerde korur", settings.LockMoney);
                ImGui::TextColored(Colors.Muted, "Sabitlenecek bakiye");
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputFloat("##money-target", &settings.MoneyTarget, 1000.0f, 10000.0f, "%.0f");
                ImGui::Dummy(ImVec2(0.0f, 3.0f));
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 125.0f);
                ImGui::InputFloat("##money-add", &MoneyDelta, 1000.0f, 10000.0f, "%.0f");
                ImGui::SameLine();
                if (ActionButton("money-add-button", "Para ekle", ImVec2(115.0f, 34.0f))) {
                    AppState::PendingActions action{}; action.AddMoney = std::max(0.0f, MoneyDelta); AppState::Queue(action);
                }
                EndCard();

                ImGui::TableNextColumn();
                BeginCard("audience-card", 205.0f);
                SectionLabel("Kitle buyutme");
                ImGui::TextColored(Colors.Muted, "Takipci");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 125.0f);
                ImGui::InputInt("##followers-add", &FollowersDelta, 100, 1000);
                ImGui::SameLine();
                if (ActionButton("followers-button", "Takipci ekle", ImVec2(115.0f, 34.0f))) {
                    AppState::PendingActions action{}; action.AddFollowers = std::max(0, FollowersDelta); AppState::Queue(action);
                }
                ImGui::TextColored(Colors.Muted, "Abone");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 125.0f);
                ImGui::InputInt("##subscribers-add", &SubscribersDelta, 10, 100);
                ImGui::SameLine();
                if (ActionButton("subscribers-button", "Abone ekle", ImVec2(115.0f, 34.0f))) {
                    AppState::PendingActions action{}; action.AddSubscribers = std::max(0, SubscribersDelta); AppState::Queue(action);
                }
                EndCard();
                ImGui::EndTable();
            }
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            BeginCard("progress-card", 218.0f);
            ToggleLine("skill-lock", "Yetenek puanini sabitle", "Harcanan puani belirlenen hedefe tamamlar", settings.LockSkillPoints);
            if (ImGui::BeginTable("progress-actions", 3, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextColumn();
                ImGui::TextColored(Colors.Muted, "Hedef puan");
                ImGui::SetNextItemWidth(-1.0f); ImGui::InputInt("##skill-target", &settings.SkillPointsTarget, 1, 10);
                ImGui::TableNextColumn();
                ImGui::TextColored(Colors.Muted, "Eklenecek puan");
                ImGui::SetNextItemWidth(-1.0f); ImGui::InputInt("##skill-add", &SkillDelta, 1, 10);
                if (ActionButton("skill-button", "Puan ekle", ImVec2(ImGui::GetContentRegionAvail().x, 34.0f))) {
                    AppState::PendingActions action{}; action.AddSkillPoints = std::max(0, SkillDelta); AppState::Queue(action);
                }
                ImGui::TableNextColumn();
                ImGui::TextColored(Colors.Muted, "Eklenecek tecrube");
                ImGui::SetNextItemWidth(-1.0f); ImGui::InputInt("##experience-add", &ExperienceDelta, 100, 1000);
                if (ActionButton("experience-button", "Tecrube ekle", ImVec2(ImGui::GetContentRegionAvail().x, 34.0f))) {
                    AppState::PendingActions action{}; action.AddExperience = std::max(0, ExperienceDelta); AppState::Queue(action);
                }
                ImGui::EndTable();
            }
            ImGui::TextColored(Colors.Muted, "Canli: %d takipci  /  %d abone  /  %d puan  /  %d tecrube",
                live.Followers, live.Subscribers, live.SkillPoints, live.Experience);
            EndCard();
        }

        void DrawStream(AppState::Settings& settings, const AppState::Snapshot& live) {
            SectionLabel("Yayin destegi", "yayindaki gorunurlugu kontrol et");
            BeginCard("stream-hero", 142.0f);
            ImGui::TextColored(Colors.Muted, "CANLI EK IZLEYICI");
            PushFont(DisplayFont); ImGui::TextColored(Colors.Accent, "%d", live.ExtraViewers); PopFont(DisplayFont);
            ImGui::TextColored(Colors.Muted, "KarakterS.EkstraIzleyen alanindan anlik okunuyor");
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(origin, ImVec2(origin.x + width, origin.y + 3.0f),
                ImGui::GetColorU32(Colors.Accent), ImGui::GetColorU32(Colors.AccentAlt),
                ImGui::GetColorU32(Colors.AccentAlt), ImGui::GetColorU32(Colors.Accent));
            EndCard();
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            BeginCard("stream-control", 160.0f);
            ToggleLine("stream-boost", "Minimum ek izleyici", "Degerin belirlenen sayinin altina dusmesini engeller", settings.StreamBoost);
            ValueSliderInt("viewer-minimum", "Minimum izleyici", settings.MinimumExtraViewers, 0, 100000);
            EndCard();
        }

        void DrawMovement(AppState::Settings& settings, const AppState::Snapshot& live) {
            SectionLabel("Hareket profili", "temel hiz kapanista otomatik geri yuklenir");
            if (ImGui::BeginTable("movement-grid", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextColumn();
                BeginCard("speed-live", 168.0f);
                ImGui::TextColored(Colors.Muted, "CANLI YURUME HIZI");
                PushFont(DisplayFont); ImGui::TextColored(Colors.AccentAlt, "%.3f", live.WalkSpeed); PopFont(DisplayFont);
                ImGui::TextWrapped("Oyun tarafinda okunan guncel hareket hizi. Mod kapandiginda yakalanan temel deger geri gelir.");
                EndCard();
                ImGui::TableNextColumn();
                BeginCard("speed-control", 168.0f);
                ToggleLine("speed", "Hiz carpanini etkinlestir", "Temel yurume hizini secilen oranda carp", settings.SpeedEnabled);
                ValueSliderFloat("speed-multiplier", "Hiz carpani", settings.SpeedMultiplier, 0.50f, 5.00f, "%.2fx");
                EndCard();
                ImGui::EndTable();
            }
        }

        bool ThemeChoice(const char* id, const char* name, const char* detail, const ImVec4& first, const ImVec4& second, bool selected) {
            ImGui::PushID(id);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const ImVec2 size(ImGui::GetContentRegionAvail().x, 92.0f);
            ImGui::InvisibleButton("##theme", size);
            const bool hovered = ImGui::IsItemHovered();
            if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            const float state = Animate(ImGui::GetID("theme-state"), selected ? 1.0f : (hovered ? 0.35f : 0.0f), 14.0f);
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                ImGui::GetColorU32(Mix(Colors.SurfaceRaised, Colors.AccentSoft, state * 0.55f)), 11.0f);
            draw->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), ImGui::GetColorU32(selected ? Colors.Accent : Colors.Border),
                11.0f, selected ? 1.5f : 1.0f, 0);
            draw->AddCircleFilled(ImVec2(origin.x + 19.0f, origin.y + 22.0f), 7.0f, ImGui::GetColorU32(first), 20);
            draw->AddCircleFilled(ImVec2(origin.x + 29.0f, origin.y + 22.0f), 7.0f, ImGui::GetColorU32(second), 20);
            draw->AddText(MediumFont ? MediumFont : ImGui::GetFont(), MediumFont ? MediumFont->LegacySize : ImGui::GetFontSize(),
                ImVec2(origin.x + 44.0f, origin.y + 13.0f), ImGui::GetColorU32(Colors.Text), name);
            draw->AddText(BodyFont ? BodyFont : ImGui::GetFont(), BodyFont ? BodyFont->LegacySize - 2.0f : ImGui::GetFontSize() - 2.0f,
                ImVec2(origin.x + 14.0f, origin.y + 54.0f), ImGui::GetColorU32(Colors.Muted), detail);
            const bool clicked = ImGui::IsItemClicked();
            ImGui::PopID();
            return clicked;
        }

        void DrawSystem(AppState::Settings& settings) {
            SectionLabel("Arayuz temasi", "secim profile kaydedilir");
            if (ImGui::BeginTable("theme-grid", 3, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextColumn();
                if (ThemeChoice("violet", "Neon Violet", "Mor + camgobegi", ImVec4(0.49f, 0.39f, 0.98f, 1.0f), ImVec4(0.16f, 0.76f, 0.94f, 1.0f), settings.AccentTheme == 0)) settings.AccentTheme = 0;
                ImGui::TableNextColumn();
                if (ThemeChoice("cyan", "Digital Cyan", "Cyan + yesil", ImVec4(0.10f, 0.76f, 0.88f, 1.0f), ImVec4(0.26f, 0.88f, 0.58f, 1.0f), settings.AccentTheme == 1)) settings.AccentTheme = 1;
                ImGui::TableNextColumn();
                if (ThemeChoice("gold", "Premium Gold", "Altin + mercan", ImVec4(0.91f, 0.66f, 0.27f, 1.0f), ImVec4(0.94f, 0.43f, 0.31f, 1.0f), settings.AccentTheme == 2)) settings.AccentTheme = 2;
                ImGui::EndTable();
            }
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            BeginCard("motion-card", 90.0f);
            ToggleLine("reduced-motion", "Azaltilmis hareket", "Sayfa ve kontrol animasyonlarini aninda tamamlar", settings.ReducedMotion);
            EndCard();
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            BeginCard("profile-card", 148.0f);
            SectionLabel("Profil ve konfigurasyon");
            if (ActionButton("save", "Ayarlari kaydet", ImVec2(170.0f, 36.0f))) {
                Commit(settings);
                std::string error;
                Notice = Config::Save(&error) ? "Ayarlar basariyla kaydedildi" : "Kaydetme hatasi: " + error;
            }
            ImGui::SameLine();
            if (ActionButton("reload", "Yeniden yukle", ImVec2(160.0f, 36.0f), 2)) {
                std::string error;
                if (Config::Load(&error)) {
                    std::scoped_lock lock(AppState::ModelMutex);
                    settings = AppState::Options;
                    Notice = "Ayarlar yeniden yuklendi";
                } else Notice = "Yukleme hatasi: " + error;
            }
            ImGui::TextColored(Colors.Muted, "%s", Narrow(Config::Path()).c_str());
            if (!Notice.empty()) ImGui::TextColored(Colors.AccentAlt, "%s", Notice.c_str());
            EndCard();
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            BeginCard("danger-card", 126.0f);
            SectionLabel("Guvenli kaldirma");
            ImGui::TextColored(Colors.Muted, "Grafik baglantisini temizler, hiz degerini geri yukler ve DLL'i surecten cikartir.");
            if (ActionButton("unload", "Hileyi tamamen kaldir", ImVec2(220.0f, 36.0f), 1)) {
                Commit(settings); Config::Save(); AppState::UnloadRequested.store(true);
            }
            EndCard();
        }

        void DrawHeader(const AppState::Snapshot& live) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(25.0f, 17.0f));
            ImGui::BeginChild("header", ImVec2(0.0f, HeaderHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
            ImGui::PopStyleVar();
            ImGui::SetCursorPos(ImVec2(25.0f, 15.0f));
            PushFont(DisplayFont); ImGui::TextColored(Colors.Text, "%s", PageTitles[Page]); PopFont(DisplayFont);
            ImGui::TextColored(Colors.Muted, "%s", PageDescriptions[Page]);

            auto* draw = ImGui::GetWindowDrawList();
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 windowSize = ImGui::GetWindowSize();
            const ImVec2 closeMin(windowPos.x + windowSize.x - 48.0f, windowPos.y + 23.0f);
            ImGui::SetCursorScreenPos(closeMin);
            ImGui::InvisibleButton("##close-menu", ImVec2(30.0f, 30.0f));
            const bool closeHovered = ImGui::IsItemHovered();
            if (closeHovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            draw->AddRectFilled(closeMin, ImVec2(closeMin.x + 30.0f, closeMin.y + 30.0f),
                ImGui::GetColorU32(closeHovered ? Mix(Colors.SurfaceRaised, Colors.Danger, 0.22f) : Colors.SurfaceRaised), 8.0f);
            draw->AddLine(ImVec2(closeMin.x + 10.0f, closeMin.y + 10.0f), ImVec2(closeMin.x + 20.0f, closeMin.y + 20.0f), ImGui::GetColorU32(Colors.Muted), 1.5f);
            draw->AddLine(ImVec2(closeMin.x + 20.0f, closeMin.y + 10.0f), ImVec2(closeMin.x + 10.0f, closeMin.y + 20.0f), ImGui::GetColorU32(Colors.Muted), 1.5f);
            if (ImGui::IsItemClicked()) AppState::MenuOpen.store(false);

            const ImVec2 keyMin(closeMin.x - 69.0f, closeMin.y);
            draw->AddRectFilled(keyMin, ImVec2(keyMin.x + 58.0f, keyMin.y + 30.0f), ImGui::GetColorU32(Colors.SurfaceRaised), 8.0f);
            draw->AddRect(keyMin, ImVec2(keyMin.x + 58.0f, keyMin.y + 30.0f), ImGui::GetColorU32(Colors.Border), 8.0f);
            draw->AddText(ImVec2(keyMin.x + 11.0f, keyMin.y + 7.0f), ImGui::GetColorU32(Colors.Muted), "INSERT");

            const ImVec4 status = live.PlayerReady ? Colors.Good : Colors.Warning;
            const char* statusText = live.PlayerReady ? "OYUNA BAGLI" : "BAGLANTI BEKLIYOR";
            const float statusWidth = live.PlayerReady ? 120.0f : 158.0f;
            const ImVec2 statusMin(keyMin.x - statusWidth - 11.0f, keyMin.y);
            draw->AddRectFilled(statusMin, ImVec2(statusMin.x + statusWidth, statusMin.y + 30.0f), ImGui::GetColorU32(Alpha(status, 0.10f)), 15.0f);
            draw->AddRect(statusMin, ImVec2(statusMin.x + statusWidth, statusMin.y + 30.0f), ImGui::GetColorU32(Alpha(status, 0.35f)), 15.0f);
            draw->AddCircleFilled(ImVec2(statusMin.x + 15.0f, statusMin.y + 15.0f), 4.0f, ImGui::GetColorU32(status), 16);
            draw->AddText(ImVec2(statusMin.x + 27.0f, statusMin.y + 7.0f), ImGui::GetColorU32(status), statusText);
            ImGui::EndChild();
        }

        void DrawSidebar(const AppState::Snapshot& live) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 16.0f));
            ImGui::BeginChild("sidebar", ImVec2(SidebarWidth, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
            ImGui::PopStyleVar();
            const ImVec2 logo = ImGui::GetCursorScreenPos();
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(logo, ImVec2(logo.x + 44.0f, logo.y + 44.0f), ImGui::GetColorU32(Colors.AccentSoft), 12.0f);
            draw->AddRect(logo, ImVec2(logo.x + 44.0f, logo.y + 44.0f), ImGui::GetColorU32(Alpha(Colors.Accent, 0.55f)), 12.0f);
            draw->AddCircleFilled(ImVec2(logo.x + 15.0f, logo.y + 16.0f), 5.0f, ImGui::GetColorU32(Colors.Accent), 18);
            draw->AddCircleFilled(ImVec2(logo.x + 29.0f, logo.y + 28.0f), 7.0f, ImGui::GetColorU32(Colors.AccentAlt), 18);
            ImGui::SetCursorPos(ImVec2(68.0f, 18.0f));
            PushFont(MediumFont); ImGui::TextColored(Colors.Text, "SCC"); PopFont(MediumFont);
            ImGui::SetCursorPos(ImVec2(68.0f, 38.0f));
            ImGui::TextColored(Colors.Muted, "STREAMER DAILY");
            ImGui::SetCursorPosY(82.0f);
            ImGui::TextColored(Colors.Muted, "KONTROL PANELI");
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            NavigationItem("Genel Bakis", NavIcon::Overview, 0);
            NavigationItem("Karakter", NavIcon::Character, 1);
            NavigationItem("Kariyer", NavIcon::Career, 2);
            NavigationItem("Yayin", NavIcon::Stream, 3);
            NavigationItem("Hareket", NavIcon::Movement, 4);
            NavigationItem("Sistem", NavIcon::System, 5);

            const float footerY = ImGui::GetWindowHeight() - 88.0f;
            ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY() + 12.0f, footerY));
            const ImVec2 footer = ImGui::GetCursorScreenPos();
            const float footerWidth = ImGui::GetContentRegionAvail().x;
            ImGui::InvisibleButton("##sidebar-footer", ImVec2(footerWidth, 57.0f));
            draw->AddRectFilled(footer, ImVec2(footer.x + footerWidth, footer.y + 57.0f), ImGui::GetColorU32(Colors.Surface), 11.0f);
            const ImVec4 status = live.PlayerReady ? Colors.Good : Colors.Warning;
            draw->AddCircleFilled(ImVec2(footer.x + 16.0f, footer.y + 19.0f), 4.0f, ImGui::GetColorU32(status), 16);
            draw->AddText(MediumFont ? MediumFont : ImGui::GetFont(), MediumFont ? MediumFont->LegacySize - 1.0f : ImGui::GetFontSize(),
                ImVec2(footer.x + 29.0f, footer.y + 10.0f), ImGui::GetColorU32(Colors.Text), live.PlayerReady ? "Sistem hazir" : "Baglanti bekleniyor");
            draw->AddText(ImVec2(footer.x + 16.0f, footer.y + 34.0f), ImGui::GetColorU32(Colors.Muted), "v0.2  /  DX11 + MONO");
            ImGui::EndChild();
        }
    }

    void Initialize() {
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        if (GetFileAttributesW(L"C:\\Windows\\Fonts\\segoeui.ttf") != INVALID_FILE_ATTRIBUTES) {
            BodyFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f);
            MediumFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 16.0f);
            DisplayFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 25.0f);
        }
        if (!BodyFont) BodyFont = io.Fonts->AddFontDefault();
        if (!MediumFont) MediumFont = BodyFont;
        if (!DisplayFont) DisplayFont = BodyFont;
        io.FontDefault = BodyFont;

        auto& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(0.0f, 0.0f);
        style.FramePadding = ImVec2(11.0f, 7.0f);
        style.ItemSpacing = ImVec2(10.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        style.WindowRounding = 16.0f;
        style.ChildRounding = 12.0f;
        style.FrameRounding = 8.0f;
        style.PopupRounding = 10.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 8.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.ScrollbarSize = 8.0f;
        style.GrabMinSize = 9.0f;
        {
            std::scoped_lock lock(AppState::ModelMutex);
            Page = AppState::Options.StartPage;
        }
        PreviousPage = Page;
    }

    void Draw() {
        if (!AppState::MenuOpen.load()) return;
        AppState::Snapshot live;
        AppState::Settings settings;
        {
            std::scoped_lock lock(AppState::ModelMutex);
            live = AppState::Live;
            settings = AppState::Options;
        }
        Colors = MakePalette(settings.AccentTheme);
        MotionReduced = settings.ReducedMotion;
        ApplyPalette();

        ImGui::SetNextWindowSize(DefaultWindowSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(900.0f, 580.0f), ImVec2(1280.0f, 820.0f));
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        const auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (!ImGui::Begin("##streamer-daily-internal", nullptr, flags)) { ImGui::End(); return; }

        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), ImGui::GetColorU32(Colors.Background), 16.0f);
        draw->AddRectFilled(ImVec2(windowPos.x, windowPos.y), ImVec2(windowPos.x + SidebarWidth, windowPos.y + windowSize.y), ImGui::GetColorU32(Colors.Sidebar), 16.0f);
        draw->AddRectFilled(ImVec2(windowPos.x + SidebarWidth - 16.0f, windowPos.y), ImVec2(windowPos.x + SidebarWidth, windowPos.y + windowSize.y), ImGui::GetColorU32(Colors.Sidebar));
        draw->AddLine(ImVec2(windowPos.x + SidebarWidth, windowPos.y + 20.0f), ImVec2(windowPos.x + SidebarWidth, windowPos.y + windowSize.y - 20.0f),
            ImGui::GetColorU32(Alpha(Colors.Border, 0.72f)), 1.0f);
        draw->AddRectFilledMultiColor(ImVec2(windowPos.x + SidebarWidth, windowPos.y), ImVec2(windowPos.x + windowSize.x, windowPos.y + 2.0f),
            ImGui::GetColorU32(Colors.Accent), ImGui::GetColorU32(Colors.AccentAlt), ImGui::GetColorU32(Colors.AccentAlt), ImGui::GetColorU32(Colors.Accent));
        draw->AddRect(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), ImGui::GetColorU32(Colors.Border), 16.0f, 1.0f, 0);

        DrawSidebar(live);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::BeginChild("main", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        DrawHeader(live);

        if (Page != PreviousPage) { PreviousPage = Page; PageTransition = MotionReduced ? 1.0f : 0.0f; }
        if (!MotionReduced && PageTransition < 1.0f) {
            PageTransition += (1.0f - PageTransition) * (1.0f - std::exp(-12.0f * std::min(ImGui::GetIO().DeltaTime, 0.05f)));
            if (PageTransition > 0.995f) PageTransition = 1.0f;
        }
        if (MotionReduced) PageTransition = 1.0f;

        ImGui::SetCursorPos(ImVec2(24.0f, HeaderHeight + 4.0f + (1.0f - PageTransition) * 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * (0.45f + PageTransition * 0.55f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::BeginChild("page", ImVec2(-18.0f, -18.0f), ImGuiChildFlags_None, ImGuiWindowFlags_None);
        ImGui::PopStyleVar();
        switch (Page) {
            case 0: DrawOverview(settings, live); break;
            case 1: DrawCharacter(settings, live); break;
            case 2: DrawCareer(settings, live); break;
            case 3: DrawStream(settings, live); break;
            case 4: DrawMovement(settings, live); break;
            case 5: DrawSystem(settings); break;
            default: Page = 0; break;
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::End();

        settings.StartPage = Page;
        Commit(settings);
    }
}
