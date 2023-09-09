#include "Math/Vector.hpp"
#include <stdio.h>
#include <math.h>
namespace KDot
{
    Vector3::Vector3(int xcomp, int ycomp, int zcomp)
    {
        x = static_cast<float>(xcomp);
        y = static_cast<float>(ycomp);
        z = static_cast<float>(zcomp);
    }

    Vector3::Vector3(float xcomp, float ycomp, float zcomp)
    {
        x = xcomp;
        y = ycomp;
        z = zcomp;
    }

    Vector3::Vector3()
    {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
    }

    Vector3::Vector3(const Vector3& other)
    {
        x = other.x;
        y = other.y;
        z = other.z;
    }
    
    Vector3 Vector3::Up = Vector3(0, 1, 0);
    Vector3 Vector3::Forward = Vector3(0, 0, 1);
    Vector3 Vector3::Right = Vector3(1, 0, 0);
    Vector3 Vector3::Down = Vector3(0, -1, 0);
    Vector3 Vector3::Back = Vector3(0, 0, -1);
    Vector3 Vector3::Left = Vector3(-1, 0, 0);
    Vector3 Vector3::Zero = Vector3(0, 0, 0);
    Vector3 Vector3::One = Vector3(1, 1, 1);

    
    Vector3 Vector3::operator+(const Vector3& other) const
    {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    Vector3 Vector3::operator-(const Vector3& other) const
    {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    Vector3 Vector3::operator*(const Vector3& other) const
    {
        return Vector3(x * other.x, y * other.y, z * other.z);
    }

    Vector3 Vector3::operator/(const Vector3& other) const
    {
        return Vector3(x / other.x, y / other.y, z / other.z);
    }

    Vector3 Vector3::operator+(float other) const
    {
        return Vector3(x + other, y + other, z + other);
    }

    Vector3 Vector3::operator-(float other) const
    {
        return Vector3(x - other, y - other, z - other);
    }

    Vector3 Vector3::operator*(float other) const
    {
        return Vector3(x * other, y * other, z * other);
    }

    Vector3 Vector3::operator/(float other) const
    {
        return Vector3(x / other, y / other, z / other);
    }

    Vector3 Vector3::Multiply(const Vector3& other) const
    {
        return Vector3(x * other.x, y * other.y, z * other.z);
    }
    Vector3 Vector3::Divide(const Vector3& other) const
    {
        return Vector3(x / other.x, y / other.y, z / other.z);
    }

    Vector3 Vector3::Multiply(float other) const
    {
        return Vector3(x * other, y * other, z * other);
    }
    Vector3 Vector3::Divide(float other) const
    {
        return Vector3(x / other, y / other, z / other);
    }

    Vector3 Vector3::operator-() const
    {
        return Vector3(-x, -y, -z);
    }

    Vector3& Vector3::operator+=(const Vector3& other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    Vector3& Vector3::operator-=(const Vector3& other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
    Vector3& Vector3::operator*=(const Vector3& other)
    {
        x *= other.x;
        y *= other.y;
        z *= other.z;
        return *this;
    }
    Vector3& Vector3::operator/=(const Vector3& other)
    {
        x /= other.x;
        y /= other.y;
        z /= other.z;
        return *this;
    }

    Vector3& Vector3::operator+=(float other)
    {
        x += other;
        y += other;
        z += other;
        return *this;
    }
    Vector3& Vector3::operator-=(float other)
    {
        x -= other;
        y -= other;
        z -= other;
        return *this;
    }
    Vector3& Vector3::operator*=(float other)
    {
        x *= other;
        y *= other;
        z *= other;
        return *this;
    }
    Vector3& Vector3::operator/=(float other)
    {
        x /= other;
        y /= other;
        z /= other;
        return *this;
    }

    bool Vector3::operator==(const Vector3& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
    bool Vector3::operator!=(const Vector3& other) const
    {
        return x != other.x || y != other.y || z != other.z;
    }

    float Vector3::operator[](int index) const
    {
        float arr[3] = { x, y, z };
        return arr[index];
    }

    float Vector3::Magnitude() const
    {
        return sqrtf(x * x + y * y + z * z);
    }
    float Vector3::SqrMagnitude() const
    {
        return x * x + y * y + z * z;
    }

    Vector3 Vector3::Normalize() const
    {
        float magnitude = Magnitude();
        return Vector3(x / magnitude, y / magnitude, z / magnitude);
    }
    Vector3 Vector3::Normalize(float magnitude) const
    {
        magnitude = Magnitude();
        return Vector3(x / magnitude, y / magnitude, z / magnitude);
    }

    float Vector3::Dot(const Vector3& other) const
    {
        return x * other.x +  y * other.y + z * other.z;
    }
    Vector3 Vector3::Cross(const Vector3& other) const
    {
        return Vector3(y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x);
    }

    Vector3 Vector3::Lerp(const Vector3& other, float t) const
    {
        return Vector3(x + (other.x - x) * t, y + (other.y - y) * t, z + (other.z - z) * t);
    }
    Vector3 Vector3::Slerp(const Vector3& other, float t) const
    {
        float dot = Dot(other);
        if (dot < 0)
        {
            return operator*(-1).Slerp(other.operator*(-1), t).operator*(-1);
        }
        if (dot > 0.9995f)
        {
            return Lerp(other, t);
        }
        float theta = acosf(dot);
        float sinTheta = sinf(theta);
        float a = sinf((1 - t) * theta) / sinTheta;
        float b = sinf(t * theta) / sinTheta;
        return operator*(a).operator+=(other.operator*(b));
    }

    std::string Vector3::ToString() const
    {
        return "(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
    }



}