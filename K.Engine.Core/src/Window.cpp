#include <Window.hpp>
#include <Platform.hpp>

namespace KDot
{
    Scope<Window> Window::Create(const InitOptions& options)
    {
#if defined(KE_PLATFORM_WEB)
        return CreateScope<JavascriptWindow>(options);
#else
        return CreateScope<DesktopWindow>(options);
#endif
    }
}
