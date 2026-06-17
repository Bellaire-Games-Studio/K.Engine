// The ECS is a header-only template library (see ECS.hpp). This translation
// unit exists so the build globs at least one source file for the module and so
// the header is compiled stand-alone, catching errors early.
#include "EntitityComponentSystem/ECS.hpp"

namespace KDot
{
    namespace ecs
    {
        static_assert(IndexOf(Make(123u, 7u)) == 123u, "ECS entity index packing is broken");
        static_assert(VersionOf(Make(123u, 7u)) == 7u, "ECS entity version packing is broken");
    }
}
