#pragma once

#include <functional>
#include <string>
#include <vector>

namespace Software::Platform
{
    class PlatformWindowService
    {
    public:
        explicit PlatformWindowService(void* nativeHandle = nullptr) : m_nativeHandle(nativeHandle)
        {
        }

        void* NativeHandle() const
        {
            return m_nativeHandle;
        }

        // Colors the native title bar and border to match the app; no-op where unsupported
        void SetChromeColors(float backgroundR, float backgroundG, float backgroundB,
                             float textR, float textG, float textB) const;

        // Wires the frame capture implemented by the platform layer
        void SetCaptureHandler(std::function<void(const std::string&)> handler)
        {
            m_capture = std::move(handler);
        }

        // Saves the next rendered frame as a 32-bit BMP file
        bool RequestCapture(const std::string& path) const
        {
            if (!m_capture)
            {
                return false;
            }
            m_capture(path);
            return true;
        }

        // Wires the runtime font switch implemented by the platform layer
        void SetFontHandler(std::function<void(const std::string&)> handler)
        {
            m_font = std::move(handler);
        }

        // Switches the app font to an installed monospace family; unknown families are ignored
        void RequestFontFamily(const std::string& family) const
        {
            if (m_font)
            {
                m_font(family);
            }
        }

        // Monospace families Slate knows how to load that are installed on this machine
        static std::vector<std::string> AvailableFontFamilies();
        // Returns the font file for a family, or empty when it is not installed
        static std::string FontFileForFamily(const std::string& family);

    private:
        void* m_nativeHandle = nullptr;
        std::function<void(const std::string&)> m_capture;
        std::function<void(const std::string&)> m_font;
    };
}
