#pragma once

#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <string>
#include <string_view>

// Slate's design system: one set of tokens and components so every screen reads as one app.
// Colors always come from the live theme palette in SlateUi.h, so presets and theme mods restyle everything.
namespace Software::Slate::UI
{
    namespace Tokens
    {
        // Spacing scale
        constexpr float SpaceXS = 4.0f;
        constexpr float SpaceS = 8.0f;
        constexpr float SpaceM = 12.0f;
        constexpr float SpaceL = 16.0f;
        constexpr float SpaceXL = 24.0f;
        constexpr float Space2XL = 36.0f;
        // Corner radii
        constexpr float RadiusS = 3.0f;
        constexpr float RadiusM = 5.0f;
        constexpr float RadiusL = 6.0f;
        // Height of the bottom status bar and the key hint row above it
        constexpr float StatusBarHeight = 30.0f;
        constexpr float HintRowHeight = 30.0f;
        // Space every screen leaves free at the bottom for the hint row and status bar
        constexpr float FooterHeight = StatusBarHeight + HintRowHeight + 6.0f;
    }

    // Blends two colors; alpha comes from the first unless keepAlpha is false
    ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t);
    // Returns a color with a new alpha
    ImVec4 Alpha(ImVec4 color, float alpha);
    ImU32 U32(const ImVec4& color);

    // Surface levels: 0 page, 1 raised bars, 2 cards and overlays, 3 hovered or pressed
    ImVec4 Surface(int level);
    // Hairline border color; strength 1 is the default card border
    ImVec4 Border(float strength = 1.0f);
    // Secondary and faint text
    ImVec4 TextSecondary();
    ImVec4 TextFaint();

    // Pushes the palette into ImGui's own widget colors and shapes; call once per frame
    void ApplyImGuiTheme();

    // Keycaps: a key drawn as a small key, used by every hint in the app
    float KeycapWidth(std::string_view label);
    float KeycapHeight();
    void DrawKeycap(ImDrawList* drawList, ImVec2 position, std::string_view label, const ImVec4& accent);
    // Keycap followed by a label, laid out as one item
    void KeyHint(std::string_view key, std::string_view label, const ImVec4& accent = Amber);
    float KeyHintWidth(std::string_view key, std::string_view label);

    // Pills: compact status text on a tinted background
    float PillWidth(std::string_view text);
    float PillHeight();
    void DrawPill(ImDrawList* drawList, ImVec2 position, std::string_view text, const ImVec4& color, bool solid = false);
    void Pill(std::string_view text, const ImVec4& color, bool solid = false);

    // Section label: a small square, the label, and a faint rule to the end of the line
    void SectionLabel(const char* text, const ImVec4& color, bool rule = true);

    // A selectable row: hover and selection backgrounds plus an accent bar. Returns true when clicked.
    bool ListRow(const char* id, bool selected, float height, const ImVec4& accent, ImVec2* rowMin = nullptr,
                 ImVec2* rowMax = nullptr, float width = 0.0f);

    // Cards: rounded, bordered surfaces used for overlays and panels
    void BeginCard(const char* id, ImVec2 size, const ImVec4& accent = Muted, float padding = Tokens::SpaceL);
    void EndCard();
    // Card title set into the card's top border like a tmux pane title, with an optional subtitle on the right
    void CardTitle(const char* title, const ImVec4& accent = Cyan, const char* subtitle = nullptr);

    // Header used at the top of screens: title, optional subtitle, optional right text, and a hairline
    void ScreenHeader(const char* title, const char* subtitle = nullptr, const char* right = nullptr,
                      const ImVec4& accent = Cyan);

    // The slab mark: a rounded tile with three lines of text, Slate's logo
    void DrawLogoMark(ImDrawList* drawList, ImVec2 center, float size, float alpha = 1.0f);
    // The lowercase wordmark with a blinking block cursor, centered in the current window
    void Wordmark(float size, double timeSeconds);

    // Turns cursor blinking on or off for every branded cursor
    void SetCursorBlink(bool enabled);
    // True when a blinking cursor should be drawn at this time
    bool CursorVisible(double timeSeconds);

    // The block-letter "slate" logo, drawn from cells like an ANSI shadow font
    ImVec2 BlockLogoSize(float cell);
    void DrawBlockLogo(ImDrawList* drawList, ImVec2 position, float cell, double timeSeconds, float alpha = 1.0f);
    // Draws the block logo centered in the current window and advances the cursor past it
    void BlockLogo(float cell, double timeSeconds);

    // One powerline segment: a filled block with an arrow edge. Returns the x where the next segment starts.
    float PowerlineSegment(ImDrawList* drawList, float x, float top, float height, std::string_view text,
                           const ImVec4& background, const ImVec4& foreground, bool pointRight = true);
    float PowerlineWidth(std::string_view text, float height);

    // Faint CRT scanlines over the whole viewport: 0 off, 1 subtle, 2 strong
    void DrawScanlines(int level);

    // Shortens text with "..." so it fits in maxWidth at the current font size
    std::string Ellipsize(std::string_view text, float maxWidth);
    // Shows a path with the home folder as ~ and forward slashes
    std::string FriendlyPath(const std::string& path);
}
