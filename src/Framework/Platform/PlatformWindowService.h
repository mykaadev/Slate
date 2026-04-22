#pragma once

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

    private:
        void* m_nativeHandle = nullptr;
    };
}
