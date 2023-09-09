#include <Window.hpp>
#include <Platform.hpp>

namespace KDot
{
    Scope<Window> Window::Create(const InitOptions& options)
    {
        return CreateScope<JavascriptWindow>(options);
    }
}