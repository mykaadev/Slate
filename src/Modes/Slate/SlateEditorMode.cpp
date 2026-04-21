#include "Modes/Slate/SlateEditorMode.h"

#include "App/Slate/PathUtils.h"
#include "App/Slate/SlateEditorContext.h"
#include "App/Slate/SlateModeIds.h"
#include "App/Slate/SlateUiState.h"
#include "App/Slate/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    namespace
    {
        const char* MonthName(int month)
        {
            static constexpr std::array<const char*, 12> Names{{
                "January", "February", "March",     "April",   "May",      "June",
                "July",    "August",   "September", "October", "November", "December",
            }};
            if (month < 1 || month > 12)
            {
                return "Month";
            }
            return Names[static_cast<std::size_t>(month - 1)];
        }

        void CenterCursorForWidth(float width)
        {
            const float x = ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - width) * 0.5f;
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), x));
        }

        std::string JournalDateLabel(const Software::Slate::JournalService& journal,
                                     const Software::Slate::fs::path& relativePath)
        {
            int year = 0;
            int month = 0;
            int day = 0;
            if (!journal.ParseJournalDate(relativePath, &year, &month, &day))
            {
                return {};
            }

            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%d %s %d", day, MonthName(month), year);
            return buffer;
        }
    }

    const char* SlateEditorMode::ModeName() const
    {
        return Software::Slate::ModeIds::Editor;
    }

    void SlateEditorMode::DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput)
    {
        auto& workspace = WorkspaceContext(context);
        auto& editor = EditorContext(context);
        auto& ui = UiState(context);
        if (!workspace.HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }

        auto* document = workspace.Documents().Active();
        if (!document)
        {
            ShowHome(context);
            return;
        }

        std::string dropError;
        const int inserted = editor.ProcessDroppedFiles(context.droppedFiles,
                                                        workspace.Documents(),
                                                        workspace.Assets(),
                                                        context.frame.elapsedSeconds,
                                                        &dropError);
        if (inserted > 0)
        {
            SetStatus("inserted " + std::to_string(inserted) + " image link(s)");
        }
        else if (!dropError.empty())
        {
            SetError(dropError);
        }

        if (handleInput)
        {
            if (ui.editorView == Software::Slate::SlateEditorView::Outline)
            {
                ui.visibleHeadings = editor.ParseHeadings(workspace.Documents());
                ui.navigation.SetCount(ui.visibleHeadings.size());
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
                {
                    ui.navigation.MoveNext();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
                {
                    ui.navigation.MovePrevious();
                }
                if (IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter))
                {
                    if (ui.navigation.HasSelection() && !ui.visibleHeadings.empty())
                    {
                        editor.JumpToLine(ui.visibleHeadings[ui.navigation.Selected()].line);
                    }
                    ui.editorView = Software::Slate::SlateEditorView::Document;
                }
                if (IsKeyPressed(ImGuiKey_Escape))
                {
                    ui.editorView = Software::Slate::SlateEditorView::Document;
                }
            }
            else
            {
                if (IsCtrlPressed(ImGuiKey_F) && workspace.Documents().HasOpenDocument())
                {
                    BeginSearchOverlay(true, context, SearchOverlayScope::Document);
                }
                else if (IsCtrlPressed(ImGuiKey_V) && workspace.Assets().HasClipboardImage() && workspace.Documents().HasOpenDocument())
                {
                    Software::Slate::fs::path assetRelative;
                    std::string error;
                    if (workspace.Assets().SaveClipboardImageAsset(document->relativePath, &assetRelative, &error))
                    {
                        editor.InsertTextAtCursor(workspace.Documents(),
                                                  Software::Slate::AssetService::MarkdownImageLink(document->relativePath,
                                                                                                   assetRelative) +
                                                      document->lineEnding,
                                                  context.frame.elapsedSeconds);
                        SetStatus("pasted image");
                    }
                    else
                    {
                        SetError(error);
                    }
                }
                else if (editor.IsTextFocused())
                {
                    if (IsKeyPressed(ImGuiKey_Escape))
                    {
                        editor.SetTextFocused(false);
                    }
                }
                else
                {
                    if (!ImGui::GetIO().KeyShift && IsKeyPressed(ImGuiKey_Slash))
                    {
                        BeginSearchOverlay(true, context);
                    }
                    else if (IsKeyPressed(ImGuiKey_O))
                    {
                        ui.navigation.Reset();
                        ui.editorView = Software::Slate::SlateEditorView::Outline;
                    }
                    else if (IsKeyPressed(ImGuiKey_Escape))
                    {
                        ShowHome(context);
                        return;
                    }
                }
            }
        }

        if (ui.editorView == Software::Slate::SlateEditorView::Outline)
        {
            DrawOutline(context);
        }
        else
        {
            DrawEditor(context, *document);
        }
    }

    std::string SlateEditorMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        const auto& ui = UiState(const_cast<Software::Core::Runtime::AppContext&>(context));
        return ui.editorView == Software::Slate::SlateEditorView::Outline
                   ? "(up/down) move   (enter) jump   (esc) editor"
                   : "(o) outline   (ctrl+f) find   (ctrl+s) save   (esc) home";
    }

    void SlateEditorMode::DrawEditor(Software::Core::Runtime::AppContext& context,
                                     Software::Slate::DocumentService::Document& document)
    {
        auto& editor = EditorContext(context);

        ImGui::TextColored(Muted, "%s", document.relativePath.generic_string().c_str());
        ImGui::SameLine();
        ImGui::TextColored(document.conflict ? Red : (document.dirty ? Amber : Green), "%s",
                           document.conflict ? "external change" : (document.dirty ? "dirty" : "saved"));

        if (editor.Journal().IsJournalPath(document.relativePath))
        {
            ImGui::Dummy(ImVec2(1.0f, 10.0f));
            DrawJournalCompanion(context, document);
            ImGui::Dummy(ImVec2(1.0f, 12.0f));
        }

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::BeginChild("LiveMarkdownEditorPage", ImVec2(0.0f, avail.y - 52.0f), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        DrawLiveMarkdownEditor(context, document);
        ImGui::EndChild();
    }

    void SlateEditorMode::DrawOutline(Software::Core::Runtime::AppContext& context)
    {
        auto& ui = UiState(context);
        ui.visibleHeadings = EditorContext(context).ParseHeadings(WorkspaceContext(context).Documents());
        ui.navigation.SetCount(ui.visibleHeadings.size());

        ImGui::TextColored(Cyan, "outline");
        ImGui::Separator();

        if (ui.visibleHeadings.empty())
        {
            ImGui::TextColored(Muted, "no headings");
            return;
        }

        for (std::size_t i = 0; i < ui.visibleHeadings.size(); ++i)
        {
            const auto& heading = ui.visibleHeadings[i];
            const bool selected = i == ui.navigation.Selected();
            std::string indent(static_cast<std::size_t>(std::max(0, heading.level - 1)) * 2, ' ');
            ImGui::TextColored(selected ? Green : Primary, "%s %s%s  line %zu", selected ? ">" : " ", indent.c_str(),
                               heading.title.c_str(), heading.line);
        }
    }

    void SlateEditorMode::DrawJournalCompanion(Software::Core::Runtime::AppContext& context,
                                               const Software::Slate::DocumentService::Document& document)
    {
        auto& workspace = WorkspaceContext(context);
        auto& editor = EditorContext(context);
        const auto* summary = editor.CurrentMonthSummary(workspace.Workspace(), &document, context.frame.elapsedSeconds);
        if (!summary)
        {
            return;
        }

        const std::string dateLabel = JournalDateLabel(editor.Journal(), document.relativePath);
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const bool stackPanels = availableWidth < 700.0f;
        const float calendarWidth = 176.0f;
        const float calendarHeight = 148.0f;
        const float graphWidth = stackPanels
                                     ? std::min(availableWidth, 360.0f)
                                     : std::min(420.0f, std::max(260.0f, availableWidth - calendarWidth - 22.0f));
        const float graphHeight = 116.0f;

        if (stackPanels)
        {
            CenterCursorForWidth(calendarWidth);
            ImGui::BeginChild("JournalCalendarPanel", ImVec2(calendarWidth, calendarHeight), false,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);
            DrawJournalCalendar(*summary);
            ImGui::EndChild();

            ImGui::Dummy(ImVec2(1.0f, 8.0f));
            CenterCursorForWidth(graphWidth);
            ImGui::BeginChild("JournalGraphPanel", ImVec2(graphWidth, graphHeight), false,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);
            DrawJournalActivityGraph(*summary);
            ImGui::EndChild();
        }
        else
        {
            const float rowWidth = calendarWidth + 22.0f + graphWidth;
            CenterCursorForWidth(rowWidth);
            ImGui::BeginGroup();
            ImGui::BeginChild("JournalCalendarPanel", ImVec2(calendarWidth, calendarHeight), false,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);
            DrawJournalCalendar(*summary);
            ImGui::EndChild();
            ImGui::SameLine(0.0f, 22.0f);
            ImGui::BeginChild("JournalGraphPanel", ImVec2(graphWidth, graphHeight), false,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);
            DrawJournalActivityGraph(*summary);
            ImGui::EndChild();
            ImGui::EndGroup();
        }

        ImGui::Dummy(ImVec2(1.0f, 10.0f));
        if (!dateLabel.empty())
        {
            ImGui::TextColored(Cyan, "%s", dateLabel.c_str());
        }
        else
        {
            char title[64];
            std::snprintf(title, sizeof(title), "%s %d", MonthName(summary->month), summary->year);
            ImGui::TextColored(Cyan, "%s", title);
        }
        ImGui::TextColored(Primary, "%s", summary->prompt.c_str());
    }

    void SlateEditorMode::DrawJournalCalendar(const Software::Slate::JournalMonthSummary& summary)
    {
        static constexpr std::array<const char*, 7> Weekdays{{"M", "T", "W", "T", "F", "S", "S"}};
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2.0f, 1.0f));
        if (!ImGui::BeginTable("JournalMonthCalendar", 7,
                               ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings |
                                   ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoPadInnerX))
        {
            ImGui::PopStyleVar();
            return;
        }

        for (int column = 0; column < 7; ++column)
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 24.0f);
        }

        for (const char* weekday : Weekdays)
        {
            ImGui::TableNextColumn();
            ImGui::TextColored(Muted, "%s", weekday);
        }

        int cell = 0;
        int day = 1;
        while (day <= summary.daysInMonth)
        {
            ImGui::TableNextRow();
            for (int column = 0; column < 7; ++column, ++cell)
            {
                ImGui::TableSetColumnIndex(column);
                if (cell < summary.firstWeekdayMondayBased || day > summary.daysInMonth)
                {
                    ImGui::TextUnformatted(" ");
                    continue;
                }

                const auto& daySummary = summary.days[static_cast<std::size_t>(day - 1)];
                ImVec4 dayColor = daySummary.hasContent ? Green : Muted;
                if (day == summary.currentDay)
                {
                    dayColor = Cyan;
                }
                if (day == summary.activeDay)
                {
                    dayColor = Amber;
                }

                char dayBuffer[8];
                std::snprintf(dayBuffer, sizeof(dayBuffer), "%2d", day);
                ImGui::TextColored(dayColor, "%s", dayBuffer);
                ++day;
            }
        }

        ImGui::EndTable();
        ImGui::PopStyleVar();
    }

    void SlateEditorMode::DrawJournalActivityGraph(const Software::Slate::JournalMonthSummary& summary)
    {
        const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float labelHeight = 18.0f;
        const float graphHeight = std::max(52.0f, ImGui::GetContentRegionAvail().y - labelHeight);
        const float paddingX = 10.0f;
        const float paddingY = 8.0f;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const ImVec2 size(width, graphHeight + labelHeight);
        ImGui::InvisibleButton("JournalActivityGraph", size);

        auto* drawList = ImGui::GetWindowDrawList();
        const float baselineY = start.y + graphHeight;
        drawList->AddLine(ImVec2(start.x, baselineY), ImVec2(start.x + width, baselineY),
                          ImGui::ColorConvertFloat4ToU32(Muted));

        int maxWords = 0;
        for (const auto& daySummary : summary.days)
        {
            maxWords = std::max(maxWords, daySummary.wordCount);
        }

        std::vector<ImVec2> points;
        points.reserve(summary.days.size());
        const float plotWidth = std::max(1.0f, width - paddingX * 2.0f);
        const float plotHeight = std::max(1.0f, graphHeight - paddingY * 2.0f);
        for (std::size_t i = 0; i < summary.days.size(); ++i)
        {
            const float t = summary.days.size() > 1 ? static_cast<float>(i) / static_cast<float>(summary.days.size() - 1) : 0.0f;
            const float x = start.x + paddingX + plotWidth * t;
            const float normalized = maxWords > 0
                                         ? static_cast<float>(summary.days[i].wordCount) / static_cast<float>(maxWords)
                                         : 0.0f;
            const float y = start.y + paddingY + plotHeight - normalized * plotHeight;
            points.emplace_back(x, y);
        }

        const ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(maxWords > 0 ? Green : Muted);
        if (points.size() > 1)
        {
            drawList->AddPolyline(points.data(), static_cast<int>(points.size()), lineColor, ImDrawFlags_None, 2.5f);
        }
        else if (!points.empty())
        {
            drawList->AddCircleFilled(points.front(), 3.0f, lineColor);
        }

        const auto drawMarker = [&](int day, const ImVec4& color, float radius) {
            if (day <= 0 || day > static_cast<int>(points.size()))
            {
                return;
            }
            drawList->AddCircleFilled(points[static_cast<std::size_t>(day - 1)], radius,
                                      ImGui::ColorConvertFloat4ToU32(color));
        };

        drawMarker(summary.currentDay, Cyan, 3.5f);
        drawMarker(summary.activeDay, Amber, 4.0f);

        char label[8];
        std::snprintf(label, sizeof(label), "%d", 1);
        drawList->AddText(ImVec2(start.x + paddingX, baselineY + 2.0f), ImGui::ColorConvertFloat4ToU32(Muted), label);
        std::snprintf(label, sizeof(label), "%d", summary.daysInMonth);
        const ImVec2 endSize = ImGui::CalcTextSize(label);
        drawList->AddText(ImVec2(start.x + width - paddingX - endSize.x, baselineY + 2.0f),
                          ImGui::ColorConvertFloat4ToU32(Muted), label);
    }

    void SlateEditorMode::DrawLiveMarkdownEditor(Software::Core::Runtime::AppContext& context,
                                                 Software::Slate::DocumentService::Document& document)
    {
        auto& editorContext = EditorContext(context);
        auto& editor = editorContext.Editor();
        editor.EnsureLoaded(document.text, document.lineEnding);
        const auto& lines = editor.Lines();

        bool inCodeFence = false;
        bool inFrontmatter = !lines.empty() && Software::Slate::PathUtils::Trim(lines.front()) == "---";
        bool checkingFrontmatter = inFrontmatter;

        const float pageWidth = std::min(ImGui::GetContentRegionAvail().x, 980.0f);
        const float pageStartX = std::max(0.0f, (ImGui::GetContentRegionAvail().x - pageWidth) * 0.5f);
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (pageWidth > 1.0f)
            {
                ImGui::SetCursorPosX(pageStartX);
            }

            const std::string trimmed = Software::Slate::PathUtils::Trim(lines[i]);
            const bool lineIsFrontmatter = checkingFrontmatter;

            ImGui::PushID(static_cast<int>(i));
            ImGui::BeginGroup();
            if (i == editor.ActiveLine())
            {
                if (editor.FocusRequested())
                {
                    ImGui::SetKeyboardFocusHere();
                    editor.ClearFocusRequest();
                }

                ImGui::SetNextItemWidth(pageWidth);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.095f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, Primary);
                int cursor = editor.CaretColumn();
                const int cursorBeforeInput = cursor;
                const int lineSizeBeforeInput = static_cast<int>(editor.ActiveLineText().size());
                const TextInputResult result = InputTextString("##LiveMarkdownLine", editor.ActiveLineText(),
                                                               ImGuiInputTextFlags_EnterReturnsTrue,
                                                               &cursor,
                                                               editor.RequestedCursorColumn());
                editor.SetCaretColumn(cursor);
                editor.ClearRequestedCursorColumn();
                editorContext.SetTextFocused(ImGui::IsItemActive());
                ImGui::PopStyleColor(2);

                if (result.edited)
                {
                    editorContext.CommitToActiveDocument(WorkspaceContext(context).Documents(), context.frame.elapsedSeconds);
                }

                if (result.submitted)
                {
                    editor.SplitActiveLine();
                    editorContext.CommitToActiveDocument(WorkspaceContext(context).Documents(), context.frame.elapsedSeconds);
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (editorContext.IsTextFocused() && ImGui::IsKeyPressed(ImGuiKey_Backspace, true) &&
                    cursorBeforeInput <= 0 && editor.ActiveLine() > 0)
                {
                    if (editor.MergeActiveLineWithPrevious())
                    {
                        editorContext.CommitToActiveDocument(WorkspaceContext(context).Documents(), context.frame.elapsedSeconds);
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (editorContext.IsTextFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete, true) &&
                    cursorBeforeInput >= lineSizeBeforeInput)
                {
                    if (editor.MergeActiveLineWithNext())
                    {
                        editorContext.CommitToActiveDocument(WorkspaceContext(context).Documents(), context.frame.elapsedSeconds);
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (editorContext.IsTextFocused() && ImGui::IsKeyPressed(ImGuiKey_UpArrow, true) && editor.ActiveLine() > 0)
                {
                    editor.MoveActiveLine(-1);
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (editorContext.IsTextFocused() && ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) &&
                    editor.ActiveLine() + 1 < lines.size())
                {
                    editor.MoveActiveLine(1);
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }
            }
            else
            {
                DrawMarkdownLine(lines[i], inCodeFence, lineIsFrontmatter);
            }

            ImGui::EndGroup();
            if (ImGui::IsItemClicked())
            {
                editor.SetActiveLine(i, static_cast<int>(lines[i].size()));
                editorContext.SetTextFocused(true);
            }
            ImGui::PopID();

            if (checkingFrontmatter && i > 0 && trimmed == "---")
            {
                checkingFrontmatter = false;
            }
            else if (!checkingFrontmatter && (trimmed.rfind("```", 0) == 0 || trimmed.rfind("~~~", 0) == 0))
            {
                inCodeFence = !inCodeFence;
            }
        }
    }
}


