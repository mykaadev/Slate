#pragma once

#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Markdown/JournalService.h"
#include "App/Slate/Editor/SlashCommandService.h"
#include "Modes/Slate/SlateModeBase.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Software::Modes::Slate
{
    // Draws the note screen: header, live editor, outline, and journal companion
    class SlateEditorMode final : public SlateModeBase
    {
    private:
        // Returns the mode id
        const char* ModeName() const override;
        // Draws the editor mode
        void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) override;
        // Returns helper text for the editor mode
        std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const override;
        // Reports whether the editor should get focus back after overlays close
        bool WantsNativeEditorVisible(const Software::Core::Runtime::AppContext& context) const override;

        // Draws the breadcrumb header above the page
        void DrawHeader(Software::Core::Runtime::AppContext& context, const Software::Slate::DocumentService::Document& document);
        // Draws the full editor layout
        void DrawEditor(Software::Core::Runtime::AppContext& context,
                        Software::Slate::DocumentService::Document& document,
                        bool handleInput);
        // Draws the outline content as a full screen list
        void DrawOutline(Software::Core::Runtime::AppContext& context);
        // Draws the outline side panel
        void DrawOutlinePanel(Software::Core::Runtime::AppContext& context);
        // Draws the journal helper panel
        void DrawJournalCompanion(Software::Core::Runtime::AppContext& context,
                                  const Software::Slate::DocumentService::Document& document,
                                  float layoutWidth = 0.0f,
                                  float textInset = 0.0f,
                                  float textWidth = 0.0f);
        // Draws the month calendar view
        void DrawJournalCalendar(const Software::Slate::JournalMonthSummary& summary);
        // Draws the month activity graph
        void DrawJournalActivityGraph(const Software::Slate::JournalMonthSummary& summary);
        // Draws the slash-command palette under the caret
        void DrawSlashCommandPalette(Software::Core::Runtime::AppContext& context,
                                     const std::vector<Software::Slate::SlashCommandDefinition>& commands);
        // Applies one slash command to the active editor line.
        bool ApplySlashCommand(Software::Core::Runtime::AppContext& context,
                               Software::Slate::DocumentService::Document& document,
                               const Software::Slate::SlashCommandDefinition& command);
        // Returns slash commands contributed by plugins and modules.
        std::vector<Software::Slate::SlashCommandDefinition> ExtensionSlashCommands(
            Software::Core::Runtime::AppContext& context) const;
        // Opens a clicked link: web links in the browser, notes in the editor.
        void OpenLinkTarget(Software::Core::Runtime::AppContext& context, const std::string& target, bool isWikiLink);
        // Resolves an image target used in the open note to a file path
        Software::Slate::fs::path ResolveImage(Software::Core::Runtime::AppContext& context, const std::string& target);

        // Selected row inside the inline slash-command palette.
        int m_slashCommandIndex = 0;
        // Last typed slash query; used to reset selection when the query changes.
        std::string m_slashCommandQuery;
        // True when Escape has hidden the current slash menu until the query changes.
        bool m_slashCommandSuppressed = false;
        // Query hidden by Escape until the user changes the slash text.
        std::string m_slashCommandSuppressedQuery;
        // Link or tag clicked inside the editor, handled after the editor finishes drawing
        std::string m_pendingLink;
        bool m_pendingLinkIsWiki = false;
        std::string m_pendingTag;
        // Image targets resolved by file name search, per note
        std::unordered_map<std::string, Software::Slate::fs::path> m_imagePaths;
        Software::Slate::fs::path m_imagePathsNote;
    };
}
