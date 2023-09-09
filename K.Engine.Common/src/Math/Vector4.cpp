#include <Math/Vector.hpp>

namespace KDot
{
    Vector4::Vector4()
    {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
        w = 0.0f;
    }
    Vector4::Vector4(float xcomp, float ycomp, float zcomp, float wcomp)
    {
        x = xcomp;
        y = ycomp;
        z = zcomp;
        w = wcomp;
    }

    Vector4 Vector4::operator+(const Vector4& other) const
    {
        return Vector4(x + other.x, y + other.y, z + other.z, w + other.w);
    }
} // namespace KDot
