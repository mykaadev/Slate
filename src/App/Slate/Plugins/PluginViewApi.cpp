#include "App/Slate/Plugins/PluginInstance.h"

#include "App/Slate/Editor/Live/InlineLayout.h"
#include "App/Slate/Editor/SlateEditorContext.h"
#include "App/Slate/Extensions/SlateExtensionRegistry.h"
#include "App/Slate/Input/ShortcutService.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/Todos/TodoService.h"
#include "App/Slate/UI/SlateUi.h"

#include "Core/Runtime/AppContext.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "lauxlib.h"
#include "lua.h"

#include <algorithm>
#include <cstdio>
#include <memory>

namespace Software::Slate
{
    namespace
    {
        using namespace Software::Slate::UI;

        constexpr const char* UiTableKey = "slate.ui";
        constexpr const char* InstanceKey = "slate.instance";

        PluginInstance& Self(lua_State* L)
        {
            lua_getfield(L, LUA_REGISTRYINDEX, InstanceKey);
            auto* plugin = static_cast<PluginInstance*>(lua_touserdata(L, -1));
            lua_pop(L, 1);
            return *plugin;
        }

        template <typename T>
        std::shared_ptr<T> Service(lua_State* L)
        {
            auto* app = Self(L).manager->App();
            auto service = app ? app->services.Resolve<T>() : nullptr;
            if (!service)
            {
                luaL_error(L, "slate service is not available yet");
            }
            return service;
        }

        std::string CheckString(lua_State* L, int index)
        {
            std::size_t length = 0;
            const char* text = luaL_checklstring(L, index, &length);
            return std::string(text, length);
        }

        std::string FieldString(lua_State* L, int table, const char* key, const std::string& fallback = {})
        {
            if (!lua_istable(L, table))
            {
                return fallback;
            }
            lua_getfield(L, table, key);
            std::string value = fallback;
            if (lua_type(L, -1) == LUA_TSTRING || lua_type(L, -1) == LUA_TNUMBER)
            {
                value = lua_tostring(L, -1);
            }
            lua_pop(L, 1);
            return value;
        }

        float FieldNumber(lua_State* L, int table, const char* key, float fallback)
        {
            if (!lua_istable(L, table))
            {
                return fallback;
            }
            lua_getfield(L, table, key);
            const float value = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : fallback;
            lua_pop(L, 1);
            return value;
        }

        bool FieldBool(lua_State* L, int table, const char* key, bool fallback)
        {
            if (!lua_istable(L, table))
            {
                return fallback;
            }
            lua_getfield(L, table, key);
            const bool value = lua_isnil(L, -1) ? fallback : lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
            return value;
        }

        // Theme colors by name, so plugins follow whatever palette the user picks
        ImVec4 ThemeColor(const std::string& name, ImVec4 fallback = Primary)
        {
            if (name == "primary" || name == "text") return Primary;
            if (name == "muted") return Muted;
            if (name == "cyan" || name == "accent") return Cyan;
            if (name == "amber" || name == "warning") return Amber;
            if (name == "green" || name == "success") return Green;
            if (name == "red" || name == "danger") return Red;
            if (name == "panel") return Panel;
            if (name == "background") return Background;
            if (name == "link") return UI::MarkdownLink;
            if (name == "heading" || name == "heading1") return MarkdownHeading1;
            if (name == "heading2") return MarkdownHeading2;
            if (name == "heading3") return MarkdownHeading3;
            if (name == "code") return MarkdownCode;
            if (name == "bullet") return MarkdownBullet;
            if (name == "image" || name == "pink") return MarkdownImage;
            return fallback;
        }

        ImVec4 Fade(ImVec4 color, float alpha)
        {
            color.w *= alpha;
            return color;
        }

        ImU32 U32(const ImVec4& color)
        {
            return ImGui::ColorConvertFloat4ToU32(color);
        }

        // ---------------------------------------------------------------- ui widgets

        int Ui_Text(lua_State* L)
        {
            const std::string text = CheckString(L, 1);
            ImVec4 color = Primary;
            bool wrap = true;
            if (lua_type(L, 2) == LUA_TSTRING)
            {
                color = ThemeColor(lua_tostring(L, 2));
            }
            else if (lua_istable(L, 2))
            {
                color = ThemeColor(FieldString(L, 2, "color", "primary"));
                wrap = FieldBool(L, 2, "wrap", true);
            }
            if (wrap)
            {
                ImGui::PushTextWrapPos(0.0f);
            }
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(text.c_str());
            ImGui::PopStyleColor();
            if (wrap)
            {
                ImGui::PopTextWrapPos();
            }
            return 0;
        }

        int Ui_Muted(lua_State* L)
        {
            lua_settop(L, 1);
            lua_pushstring(L, "muted");
            return Ui_Text(L);
        }

        int Ui_Heading(lua_State* L)
        {
            const std::string text = CheckString(L, 1);
            const int level = static_cast<int>(luaL_optinteger(L, 2, 1));
            const float base = ImGui::GetFontSize();
            const float size = Live::HeadingFontSize(std::clamp(level, 1, 6), base, true);
            const ImVec4 color = level <= 1 ? MarkdownHeading1 : (level == 2 ? MarkdownHeading2 : MarkdownHeading3);
            ImGui::PushFont(nullptr, size);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::PopFont();
            return 0;
        }

        // Renders Markdown with the same inline renderer the editor uses
        int Ui_Markdown(lua_State* L)
        {
            const std::string text = CheckString(L, 1);
            const float width = std::max(40.0f, ImGui::GetContentRegionAvail().x);
            auto* drawList = ImGui::GetWindowDrawList();
            std::size_t start = 0;
            while (start <= text.size())
            {
                const std::size_t end = text.find('\n', start);
                const std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
                Live::InlineOptions options;
                options.fontSize = ImGui::GetFontSize();
                options.wrapWidth = width;
                options.lineGap = 3.0f;
                const Live::InlineLayout layout = Live::LayoutInline(line, options);
                const ImVec2 origin = ImGui::GetCursorScreenPos();
                Live::DrawInline(drawList, layout, line, origin, options);
                ImGui::Dummy(ImVec2(width, std::max(layout.height, options.fontSize)));
                if (end == std::string::npos)
                {
                    break;
                }
                start = end + 1;
            }
            return 0;
        }

        int Ui_Button(lua_State* L)
        {
            const std::string label = CheckString(L, 1);
            const ImVec4 color = ThemeColor(FieldString(L, 2, "color", "cyan"), Cyan);
            const float width = FieldNumber(L, 2, "width", 0.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, Fade(color, 0.12f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Fade(color, 0.24f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Fade(color, 0.34f));
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 5.0f));
            const bool clicked = ImGui::Button(label.c_str(), ImVec2(width, 0.0f));
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);
            lua_pushboolean(L, clicked);
            return 1;
        }

        int Ui_Link(lua_State* L)
        {
            const std::string label = CheckString(L, 1);
            const ImVec4 color = ThemeColor(FieldString(L, 2, "color", "link"), UI::MarkdownLink);
            const ImVec2 size = ImGui::CalcTextSize(label.c_str());
            const ImVec2 position = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::InvisibleButton(label.c_str(), size);
            const bool hovered = ImGui::IsItemHovered();
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddText(position, U32(hovered ? color : Fade(color, 0.9f)), label.c_str());
            if (hovered)
            {
                drawList->AddLine(ImVec2(position.x, position.y + size.y), ImVec2(position.x + size.x, position.y + size.y), U32(color), 1.0f);
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            lua_pushboolean(L, clicked);
            return 1;
        }

        int Ui_Separator(lua_State*)
        {
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            ImGui::GetWindowDrawList()->AddLine(ImVec2(start.x, start.y + 4.0f), ImVec2(start.x + width, start.y + 4.0f),
                                                U32(Fade(Muted, 0.22f)), 1.0f);
            ImGui::Dummy(ImVec2(width, 9.0f));
            return 0;
        }

        int Ui_Space(lua_State* L)
        {
            ImGui::Dummy(ImVec2(1.0f, static_cast<float>(luaL_optnumber(L, 1, 8.0))));
            return 0;
        }

        int Ui_Dummy(lua_State* L)
        {
            ImGui::Dummy(ImVec2(static_cast<float>(luaL_checknumber(L, 1)), static_cast<float>(luaL_optnumber(L, 2, 1.0))));
            return 0;
        }

        int Ui_SameLine(lua_State* L)
        {
            ImGui::SameLine(0.0f, static_cast<float>(luaL_optnumber(L, 1, -1.0)));
            return 0;
        }

        int Ui_Width(lua_State* L)
        {
            lua_pushnumber(L, ImGui::GetContentRegionAvail().x);
            return 1;
        }

        int Ui_Height(lua_State* L)
        {
            lua_pushnumber(L, ImGui::GetContentRegionAvail().y);
            return 1;
        }

        int Ui_BeginPanel(lua_State* L)
        {
            const std::string id = CheckString(L, 1);
            const float width = static_cast<float>(luaL_optnumber(L, 2, 0.0));
            const float height = static_cast<float>(luaL_optnumber(L, 3, 0.0));
            const std::string background = FieldString(L, 4, "background", "");
            const float padding = FieldNumber(L, 4, "padding", 10.0f);
            const bool border = FieldBool(L, 4, "border", false);
            const ImVec4 highlight = ThemeColor(FieldString(L, 4, "highlight", ""), ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, background.empty() ? ImVec4(0, 0, 0, 0) : Fade(ThemeColor(background), 0.6f));
            ImGui::PushStyleColor(ImGuiCol_Border, highlight.w > 0.0f ? Fade(highlight, 0.6f) : Fade(Muted, 0.2f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, border || highlight.w > 0.0f ? 1.0f : 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
            ImGui::BeginChild(id.c_str(), ImVec2(width, height),
                              ImGuiChildFlags_AlwaysUseWindowPadding | (border || highlight.w > 0.0f ? ImGuiChildFlags_Borders : 0));
            return 0;
        }

        int Ui_EndPanel(lua_State*)
        {
            ImGui::EndChild();
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(2);
            return 0;
        }

        // A clickable, draggable card: the building block of boards and lists
        int Ui_Card(lua_State* L)
        {
            luaL_checktype(L, 1, LUA_TTABLE);
            const std::string id = FieldString(L, 1, "id");
            const std::string title = FieldString(L, 1, "title");
            const std::string body = FieldString(L, 1, "body");
            const std::string badge = FieldString(L, 1, "badge");
            const ImVec4 accent = ThemeColor(FieldString(L, 1, "accent", "muted"), Muted);
            const ImVec4 badgeColor = ThemeColor(FieldString(L, 1, "badge_color", "muted"), Muted);
            const bool selected = FieldBool(L, 1, "selected", false);
            const std::string dragType = FieldString(L, 1, "drag_type");
            const std::string dragPayload = FieldString(L, 1, "drag_payload", id);
            const float width = std::max(60.0f, FieldNumber(L, 1, "width", ImGui::GetContentRegionAvail().x));

            const float pad = 10.0f;
            const float badgeWidth = badge.empty() ? 0.0f : ImGui::CalcTextSize(badge.c_str()).x * 0.86f + 14.0f;
            const float textWidth = std::max(20.0f, width - pad * 2.0f - 6.0f - (badgeWidth > 0.0f ? badgeWidth + 6.0f : 0.0f));
            const ImVec2 titleSize = ImGui::CalcTextSize(title.c_str(), nullptr, false, textWidth);
            const std::string bodyLine = body.substr(0, body.find('\n'));
            const float small = ImGui::GetFontSize() * 0.86f;
            const float bodyHeight = bodyLine.empty() ? 0.0f : small + 6.0f;
            const float height = pad * 2.0f + titleSize.y + bodyHeight;

            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + width, min.y + height);
            ImGui::PushID(id.c_str());
            ImGui::InvisibleButton("##card", ImVec2(width, height), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
            const bool hovered = ImGui::IsItemHovered();
            // Clicks count on release and only when the mouse did not drag, so dragging never opens a card
            const bool dragged = ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] > 36.0f;
            const bool clicked = ImGui::IsItemDeactivated() && hovered && !dragged;
            const bool rightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
            const bool doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

            auto* drawList = ImGui::GetWindowDrawList();
            ImVec4 fill = Panel;
            fill = ImVec4(fill.x + (Muted.x - fill.x) * (hovered ? 0.14f : 0.07f), fill.y + (Muted.y - fill.y) * (hovered ? 0.14f : 0.07f),
                          fill.z + (Muted.z - fill.z) * (hovered ? 0.14f : 0.07f), 1.0f);
            drawList->AddRectFilled(min, max, U32(fill), 7.0f);
            drawList->AddRect(min, max, U32(selected ? Fade(Cyan, 0.8f) : Fade(Muted, hovered ? 0.35f : 0.16f)), 7.0f, 0, selected ? 1.5f : 1.0f);
            drawList->AddRectFilled(ImVec2(min.x, min.y + 6.0f), ImVec2(min.x + 3.0f, max.y - 6.0f), U32(accent), 1.5f);
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(min.x + pad + 4.0f, min.y + pad), U32(Primary), title.c_str(),
                              nullptr, textWidth);
            if (!bodyLine.empty())
            {
                drawList->PushClipRect(min, ImVec2(max.x - pad, max.y), true);
                drawList->AddText(ImGui::GetFont(), small, ImVec2(min.x + pad + 4.0f, min.y + pad + titleSize.y + 4.0f), U32(Muted),
                                  bodyLine.c_str());
                drawList->PopClipRect();
            }
            if (!badge.empty())
            {
                const ImVec2 badgeMin(max.x - pad - badgeWidth, min.y + pad - 1.0f);
                const ImVec2 badgeMax(max.x - pad, badgeMin.y + small + 6.0f);
                drawList->AddRectFilled(badgeMin, badgeMax, U32(Fade(badgeColor, 0.16f)), 8.0f);
                drawList->AddText(ImGui::GetFont(), small, ImVec2(badgeMin.x + 7.0f, badgeMin.y + 3.0f), U32(badgeColor), badge.c_str());
            }

            if (!dragType.empty() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload(dragType.c_str(), dragPayload.c_str(), dragPayload.size() + 1);
                ImGui::TextColored(Primary, "%s", title.c_str());
                ImGui::EndDragDropSource();
            }
            if (hovered)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            ImGui::PopID();
            // Spacing without adding an item, so tooltip and drop_target still apply to the card
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);

            if (doubleClicked)
            {
                lua_pushstring(L, "double");
            }
            else if (clicked)
            {
                lua_pushstring(L, "click");
            }
            else if (rightClicked)
            {
                lua_pushstring(L, "right");
            }
            else
            {
                lua_pushnil(L);
            }
            return 1;
        }

        // Accepts a drag of the given type on the last item (a card or a panel); returns the payload
        int Ui_DropTarget(lua_State* L)
        {
            const std::string type = CheckString(L, 1);
            std::string payload;
            bool dropped = false;
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* data = ImGui::AcceptDragDropPayload(type.c_str()))
                {
                    payload.assign(static_cast<const char*>(data->Data), static_cast<std::size_t>(std::max(0, data->DataSize - 1)));
                    dropped = true;
                }
                ImGui::EndDragDropTarget();
            }
            if (dropped)
            {
                lua_pushlstring(L, payload.data(), payload.size());
            }
            else
            {
                lua_pushnil(L);
            }
            return 1;
        }

        int Ui_Dragging(lua_State* L)
        {
            const std::string type = CheckString(L, 1);
            const ImGuiPayload* payload = ImGui::GetDragDropPayload();
            lua_pushboolean(L, payload && payload->IsDataType(type.c_str()));
            return 1;
        }

        int Ui_Input(lua_State* L)
        {
            const std::string id = CheckString(L, 1);
            std::string text = luaL_optstring(L, 2, "");
            const std::string hint = luaL_optstring(L, 3, "");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, Fade(Panel, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
            text.reserve(text.size() + 256);
            char buffer[1024];
            std::snprintf(buffer, sizeof(buffer), "%s", text.c_str());
            const bool submitted = ImGui::InputTextWithHint(("##" + id).c_str(), hint.c_str(), buffer, sizeof(buffer),
                                                            ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
            lua_pushstring(L, buffer);
            lua_pushboolean(L, submitted);
            return 2;
        }

        int Ui_Checkbox(lua_State* L)
        {
            const std::string label = CheckString(L, 1);
            bool value = lua_toboolean(L, 2) != 0;
            ImGui::PushStyleColor(ImGuiCol_CheckMark, Background);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, value ? MarkdownCheckboxDone : Panel);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            const bool changed = ImGui::Checkbox(label.c_str(), &value);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            lua_pushboolean(L, value);
            lua_pushboolean(L, changed);
            return 2;
        }

        int Ui_Badge(lua_State* L)
        {
            const std::string text = CheckString(L, 1);
            const ImVec4 color = ThemeColor(luaL_optstring(L, 2, "cyan"), Cyan);
            const float small = ImGui::GetFontSize() * 0.86f;
            const ImVec2 size(ImGui::CalcTextSize(text.c_str()).x * 0.86f + 14.0f, small + 6.0f);
            const ImVec2 min = ImGui::GetCursorScreenPos();
            ImGui::Dummy(size);
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y), U32(Fade(color, 0.16f)), 8.0f);
            drawList->AddText(ImGui::GetFont(), small, ImVec2(min.x + 7.0f, min.y + 3.0f), U32(color), text.c_str());
            return 0;
        }

        int Ui_Progress(lua_State* L)
        {
            const float fraction = std::clamp(static_cast<float>(luaL_checknumber(L, 1)), 0.0f, 1.0f);
            const ImVec4 color = ThemeColor(luaL_optstring(L, 2, "green"), Green);
            const float width = ImGui::GetContentRegionAvail().x;
            const ImVec2 min = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(width, 6.0f));
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(min, ImVec2(min.x + width, min.y + 6.0f), U32(Fade(Muted, 0.18f)), 3.0f);
            drawList->AddRectFilled(min, ImVec2(min.x + width * fraction, min.y + 6.0f), U32(color), 3.0f);
            return 0;
        }

        int Ui_Tooltip(lua_State* L)
        {
            const std::string text = CheckString(L, 1);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip("%s", text.c_str());
            }
            return 0;
        }

        int Ui_Hovered(lua_State* L)
        {
            lua_pushboolean(L, ImGui::IsItemHovered());
            return 1;
        }

        int Ui_Key(lua_State* L)
        {
            KeyBinding binding;
            const std::string chord = CheckString(L, 1);
            const ImGuiIO& io = ImGui::GetIO();
            const bool pressed = ShortcutService::ParseBinding(chord, &binding) && !ImGui::IsAnyItemActive() &&
                                 ImGui::IsKeyPressed(binding.key, false) && io.KeyCtrl == binding.ctrl &&
                                 io.KeyShift == binding.shift && io.KeyAlt == binding.alt;
            lua_pushboolean(L, pressed);
            return 1;
        }

        // ---------------------------------------------------------------- views

        int L_ViewRegister(lua_State* L)
        {
            luaL_checktype(L, 1, LUA_TTABLE);
            PluginInstance& plugin = Self(L);
            ViewContribution view;
            view.owner = plugin.info.manifest.id;
            view.id = FieldString(L, 1, "id");
            view.title = FieldString(L, 1, "title", view.id);
            view.location = FieldString(L, 1, "location", "page") == "sidebar" ? ViewLocation::Sidebar : ViewLocation::Page;
            if (view.id.empty())
            {
                return luaL_error(L, "view needs an id");
            }
            lua_getfield(L, 1, "draw");
            if (!lua_isfunction(L, -1))
            {
                return luaL_error(L, "view '%s' needs a draw function", view.id.c_str());
            }
            const int ref = luaL_ref(L, LUA_REGISTRYINDEX);

            // After one failure the view shows its error instead of re-running every frame
            auto failed = std::make_shared<std::string>();
            PluginManager* manager = plugin.manager;
            const std::string pluginId = plugin.info.manifest.id;
            const std::uint64_t generation = plugin.generation;
            view.draw = [manager, pluginId, generation, ref, failed](float width, float height) {
                if (!failed->empty())
                {
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextColored(Red, "%s", failed->c_str());
                    ImGui::PopTextWrapPos();
                    return;
                }
                // A script that errors halfway can leave panels open; ImGui unwinds them back to this point
                ImGuiErrorRecoveryState state;
                ImGui::ErrorRecoveryStoreState(&state);
                const bool ok = manager->CallRef(pluginId, generation, ref, [width, height](PluginInstance& target) {
                    lua_getfield(target.L, LUA_REGISTRYINDEX, UiTableKey);
                    lua_pushnumber(target.L, width);
                    lua_pushnumber(target.L, height);
                    return 3;
                }, 0);
                ImGui::ErrorRecoveryTryToRecoverState(&state);
                if (!ok)
                {
                    PluginInstance* plugin = manager->FindLoaded(pluginId);
                    *failed = plugin ? plugin->info.error : "view failed";
                }
            };

            Service<SlateExtensionRegistry>(L)->SetView(std::move(view));
            return 0;
        }

        int L_ViewOpen(lua_State* L)
        {
            const std::string id = CheckString(L, 1);
            lua_getglobal(L, "slate");
            lua_getfield(L, -1, "command");
            lua_getfield(L, -1, "run");
            lua_pushstring(L, ("slate.view.open " + id).c_str());
            lua_call(L, 1, 0);
            return 0;
        }

        // ---------------------------------------------------------------- todos

        const char* StateName(TodoState state)
        {
            switch (state)
            {
            case TodoState::Research: return "research";
            case TodoState::Doing: return "doing";
            case TodoState::Done: return "done";
            case TodoState::Open:
            default: return "open";
            }
        }

        bool ParseState(const std::string& text, TodoState* state)
        {
            if (text == "open") { *state = TodoState::Open; return true; }
            if (text == "research") { *state = TodoState::Research; return true; }
            if (text == "doing") { *state = TodoState::Doing; return true; }
            if (text == "done") { *state = TodoState::Done; return true; }
            return false;
        }

        const char* PriorityName(TodoPriority priority)
        {
            switch (priority)
            {
            case TodoPriority::Low: return "low";
            case TodoPriority::High: return "high";
            case TodoPriority::Urgent: return "urgent";
            case TodoPriority::Normal:
            default: return "normal";
            }
        }

        bool ParsePriorityName(const std::string& text, TodoPriority* priority)
        {
            if (text == "low") { *priority = TodoPriority::Low; return true; }
            if (text == "normal") { *priority = TodoPriority::Normal; return true; }
            if (text == "high") { *priority = TodoPriority::High; return true; }
            if (text == "urgent") { *priority = TodoPriority::Urgent; return true; }
            return false;
        }

        void ReloadEditorIfTouched(lua_State* L, const TodoMutationResult& result)
        {
            if (!result.touchedActiveDocument)
            {
                return;
            }
            auto workspace = Service<SlateWorkspaceContext>(L);
            Service<SlateEditorContext>(L)->LoadFromActiveDocument(workspace->Documents());
        }

        int L_TodosList(lua_State* L)
        {
            auto workspace = Service<SlateWorkspaceContext>(L);
            if (!workspace->HasWorkspaceLoaded())
            {
                lua_newtable(L);
                return 1;
            }
            const auto todos = Service<TodoService>(L)->CollectTodos(*workspace);
            lua_createtable(L, static_cast<int>(todos.size()), 0);
            for (std::size_t i = 0; i < todos.size(); ++i)
            {
                const auto& todo = todos[i];
                lua_createtable(L, 0, 6);
                lua_pushstring(L, todo.relativePath.generic_u8string().c_str());
                lua_setfield(L, -2, "path");
                lua_pushstring(L, todo.title.c_str());
                lua_setfield(L, -2, "title");
                lua_pushstring(L, todo.description.c_str());
                lua_setfield(L, -2, "description");
                lua_pushstring(L, StateName(todo.state));
                lua_setfield(L, -2, "state");
                lua_pushstring(L, PriorityName(todo.priority));
                lua_setfield(L, -2, "priority");
                lua_createtable(L, static_cast<int>(todo.tags.size()), 0);
                for (std::size_t t = 0; t < todo.tags.size(); ++t)
                {
                    lua_pushstring(L, todo.tags[t].c_str());
                    lua_rawseti(L, -2, static_cast<lua_Integer>(t + 1));
                }
                lua_setfield(L, -2, "tags");
                lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
            }
            return 1;
        }

        int L_TodosCreate(lua_State* L)
        {
            luaL_checktype(L, 1, LUA_TTABLE);
            TodoState state = TodoState::Open;
            TodoPriority priority = TodoPriority::Normal;
            ParseState(FieldString(L, 1, "state", "open"), &state);
            ParsePriorityName(FieldString(L, 1, "priority", "normal"), &priority);
            const std::string title = FieldString(L, 1, "title");
            if (title.empty())
            {
                return luaL_error(L, "todo needs a title");
            }
            auto workspace = Service<SlateWorkspaceContext>(L);
            TodoMutationResult result;
            if (!Service<TodoService>(L)->CreateTodo(*workspace, state, title, FieldString(L, 1, "description"),
                                                     Self(L).manager->App()->frame.elapsedSeconds, &result, priority))
            {
                return luaL_error(L, "could not create todo: %s", result.error.c_str());
            }
            ReloadEditorIfTouched(L, result);
            lua_pushstring(L, result.relativePath.generic_u8string().c_str());
            return 1;
        }

        bool FindTodo(lua_State* L, const std::string& path, TodoTicket* ticket)
        {
            auto workspace = Service<SlateWorkspaceContext>(L);
            for (const auto& todo : Service<TodoService>(L)->CollectTodos(*workspace))
            {
                if (todo.relativePath.generic_u8string() == path)
                {
                    *ticket = todo;
                    return true;
                }
            }
            return false;
        }

        int L_TodosUpdate(lua_State* L)
        {
            const std::string path = CheckString(L, 1);
            luaL_checktype(L, 2, LUA_TTABLE);
            TodoTicket ticket;
            if (!FindTodo(L, path, &ticket))
            {
                return luaL_error(L, "no todo at %s", path.c_str());
            }
            TodoState state = ticket.state;
            TodoPriority priority = ticket.priority;
            ParseState(FieldString(L, 2, "state", StateName(ticket.state)), &state);
            ParsePriorityName(FieldString(L, 2, "priority", PriorityName(ticket.priority)), &priority);
            auto workspace = Service<SlateWorkspaceContext>(L);
            TodoMutationResult result;
            const bool ok = Service<TodoService>(L)->UpdateTodo(*workspace, ticket, state, FieldString(L, 2, "title", ticket.title),
                                                                FieldString(L, 2, "description", ticket.description),
                                                                Self(L).manager->App()->frame.elapsedSeconds, &result, priority);
            if (ok)
            {
                ReloadEditorIfTouched(L, result);
            }
            lua_pushboolean(L, ok);
            return 1;
        }

        int L_TodosDelete(lua_State* L)
        {
            const std::string path = CheckString(L, 1);
            TodoTicket ticket;
            if (!FindTodo(L, path, &ticket))
            {
                lua_pushboolean(L, false);
                return 1;
            }
            auto workspace = Service<SlateWorkspaceContext>(L);
            TodoMutationResult result;
            const bool ok = Service<TodoService>(L)->DeleteTodo(*workspace, ticket, Self(L).manager->App()->frame.elapsedSeconds, &result);
            lua_pushboolean(L, ok);
            return 1;
        }

        int L_TodosStates(lua_State* L)
        {
            lua_createtable(L, 4, 0);
            const char* states[] = {"open", "research", "doing", "done"};
            for (int i = 0; i < 4; ++i)
            {
                lua_pushstring(L, states[i]);
                lua_rawseti(L, -2, i + 1);
            }
            return 1;
        }
    }

    namespace PluginLuaApi
    {
        void InstallViews(PluginInstance& plugin)
        {
            lua_State* L = plugin.L;
            static const luaL_Reg ui[] = {
                {"text", Ui_Text},
                {"muted", Ui_Muted},
                {"heading", Ui_Heading},
                {"markdown", Ui_Markdown},
                {"button", Ui_Button},
                {"link", Ui_Link},
                {"separator", Ui_Separator},
                {"space", Ui_Space},
                {"same_line", Ui_SameLine},
                {"dummy", Ui_Dummy},
                {"width", Ui_Width},
                {"height", Ui_Height},
                {"begin_panel", Ui_BeginPanel},
                {"end_panel", Ui_EndPanel},
                {"card", Ui_Card},
                {"drop_target", Ui_DropTarget},
                {"dragging", Ui_Dragging},
                {"input", Ui_Input},
                {"checkbox", Ui_Checkbox},
                {"badge", Ui_Badge},
                {"progress", Ui_Progress},
                {"tooltip", Ui_Tooltip},
                {"hovered", Ui_Hovered},
                {"key", Ui_Key},
                {nullptr, nullptr},
            };
            lua_newtable(L);
            luaL_setfuncs(L, ui, 0);
            lua_setfield(L, LUA_REGISTRYINDEX, UiTableKey);

            static const luaL_Reg view[] = {{"register", L_ViewRegister}, {"open", L_ViewOpen}, {nullptr, nullptr}};
            static const luaL_Reg todos[] = {
                {"list", L_TodosList},
                {"create", L_TodosCreate},
                {"update", L_TodosUpdate},
                {"delete", L_TodosDelete},
                {"states", L_TodosStates},
                {nullptr, nullptr},
            };
            lua_getglobal(L, "slate");
            lua_newtable(L);
            luaL_setfuncs(L, view, 0);
            lua_setfield(L, -2, "view");
            lua_newtable(L);
            luaL_setfuncs(L, todos, 0);
            lua_setfield(L, -2, "todos");
            lua_pop(L, 1);
        }
    }
}
