#include "Platform/PlatformWindowService.h"

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <dwmapi.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <utility>

namespace Software::Platform
{
    void PlatformWindowService::SetChromeColors(float backgroundR, float backgroundG, float backgroundB,
                                                float textR, float textG, float textB) const
    {
#if defined(_WIN32)
        if (!m_nativeHandle)
        {
            return;
        }
        const auto byte = [](float value) {
            return static_cast<BYTE>(std::clamp(static_cast<int>(std::lround(value * 255.0f)), 0, 255));
        };
        const HWND window = static_cast<HWND>(m_nativeHandle);
        const COLORREF caption = RGB(byte(backgroundR), byte(backgroundG), byte(backgroundB));
        const COLORREF text = RGB(byte(textR), byte(textG), byte(textB));
        const COLORREF border = RGB(byte(backgroundR * 0.8f + textR * 0.2f), byte(backgroundG * 0.8f + textG * 0.2f),
                                    byte(backgroundB * 0.8f + textB * 0.2f));
        // Light palettes get a light caption; dark palettes keep the immersive dark frame
        const BOOL dark = (backgroundR + backgroundG + backgroundB) < 1.5f ? TRUE : FALSE;
        DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        DwmSetWindowAttribute(window, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
        DwmSetWindowAttribute(window, DWMWA_TEXT_COLOR, &text, sizeof(text));
        DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR, &border, sizeof(border));
#else
        (void)backgroundR;
        (void)backgroundG;
        (void)backgroundB;
        (void)textR;
        (void)textG;
        (void)textB;
#endif
    }

    namespace
    {
        // Monospace families and the file names they install under
        const std::vector<std::pair<const char*, std::vector<const char*>>>& KnownFontFamilies()
        {
            static const std::vector<std::pair<const char*, std::vector<const char*>>> families{
                {"Cascadia Mono", {"CascadiaMono.ttf"}},
                {"Cascadia Code", {"CascadiaCode.ttf"}},
                {"Consolas", {"consola.ttf"}},
                {"JetBrains Mono", {"JetBrainsMono-Regular.ttf", "JetBrainsMono[wght].ttf"}},
                {"Fira Code", {"FiraCode-Regular.ttf", "FiraCode[wght].ttf"}},
                {"Iosevka", {"Iosevka-Regular.ttf", "iosevka-regular.ttf"}},
                {"Source Code Pro", {"SourceCodePro-Regular.ttf"}},
                {"IBM Plex Mono", {"IBMPlexMono-Regular.ttf"}},
                {"Hack", {"Hack-Regular.ttf"}},
                {"Ubuntu Mono", {"UbuntuMono-Regular.ttf", "UbuntuMono-R.ttf"}},
                {"Lucida Console", {"lucon.ttf"}},
                {"Courier New", {"cour.ttf"}},
            };
            return families;
        }

        std::vector<std::filesystem::path> FontDirectories()
        {
            std::vector<std::filesystem::path> directories;
#if defined(_WIN32)
            directories.emplace_back("C:/Windows/Fonts");
            char* localAppData = nullptr;
            std::size_t length = 0;
            if (_dupenv_s(&localAppData, &length, "LOCALAPPDATA") == 0 && localAppData)
            {
                directories.emplace_back(std::filesystem::path(localAppData) / "Microsoft" / "Windows" / "Fonts");
                std::free(localAppData);
            }
#endif
            return directories;
        }
    }

    std::vector<std::string> PlatformWindowService::AvailableFontFamilies()
    {
        std::vector<std::string> families;
        for (const auto& [family, files] : KnownFontFamilies())
        {
            if (!FontFileForFamily(family).empty())
            {
                families.emplace_back(family);
            }
        }
        return families;
    }

    std::string PlatformWindowService::FontFileForFamily(const std::string& family)
    {
        for (const auto& [name, files] : KnownFontFamilies())
        {
            if (family != name)
            {
                continue;
            }
            for (const auto& directory : FontDirectories())
            {
                for (const char* file : files)
                {
                    std::error_code ec;
                    const auto path = directory / file;
                    if (std::filesystem::exists(path, ec))
                    {
                        return path.string();
                    }
                }
            }
        }
        return {};
    }
}
