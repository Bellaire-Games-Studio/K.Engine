#include <Math/Vector.hpp>

namespace KDot
{
    Vector2::Vector2(int xcomp, int ycomp)
    {
        x = static_cast<float>(xcomp);
        y = static_cast<float>(ycomp);
    }
    Vector2::Vector2(float xcomp, float ycomp)
    {
        x = xcomp;
        y = ycomp;
    }
    Vector2::Vector2()
    {
        x = 0.0f;
        y = 0.0f;
    }
    Vector2::Vector2(const Vector2& other)
    {
        x = other.x;
        y = other.y;
    }

    Vector2 Vector2::Up = Vector2(0, 1);
    Vector2 Vector2::Right = Vector2(1, 0);
    Vector2 Vector2::Down = Vector2(0, -1);
    Vector2 Vector2::Left = Vector2(-1, 0);
    Vector2 Vector2::Zero = Vector2(0, 0);
    Vector2 Vector2::One = Vector2(1, 1);

    Vector2 Vector2::Multiply(const Vector2& left, const Vector2& right)
    {
        return Vector2(left.x * right.x, left.y * right.y);
    }
    Vector2 Vector2::Multiply(const Vector2& other) const 
    {
        return Vector2(other.x * x, other.y * y);
    }
    Vector2 Vector2::Multiply(const float other) const
    {
        return Vector2(other * x, other * y);
    }
    Vector2 Vector2::Divide(const Vector2& left, const Vector2& right)
    {
        return Vector2(left.x / right.x, left.y / right.y);
    }
    Vector2 Vector2::Divide(const Vector2& other) const
    {
        return Vector2(x / other.x, y / other.y);
    }
    Vector2 Vector2::Divide(const float other) const
    {
        return Vector2(x / other, y / other);
    }
    Vector2 Vector2::operator+(const Vector2& other) const
    {
        return Vector2(x + other.x, y + other.y);
    }
    Vector2 Vector2::operator-(const Vector2& other) const
    {
        return Vector2(x - other.x, y - other.y);
    }
    Vector2 Vector2::operator*(const Vector2& other) const
    {
        return Vector2(x * other.x, y * other.y);
    }
    Vector2 Vector2::operator/(const Vector2& other) const
    {
        return Vector2(x / other.x, y / other.y);
    }
    Vector2 Vector2::operator+(const float other) const
    {
        return Vector2(x + other, y + other);
    }
    Vector2 Vector2::operator-(const float other) const
    {
        return Vector2(x - other, y - other);
    }
    Vector2 Vector2::operator*(const float other) const
    {
        return Vector2(x * other, y * other);
    }
    Vector2 Vector2::operator/(const float other) const
    {
        return Vector2(x /  other, y / other);
    }

    Vector2& Vector2::operator+=(const Vector2& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }
    Vector2& Vector2::operator-=(const Vector2& other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    Vector2& Vector2::operator*=(const Vector2& other)
    {
        x *= other.x;
        y *= other.y;
        return *this;
    }
    Vector2& Vector2::operator/=(const Vector2& other)
    {
        x /= other.x;
        y /= other.y;
        return *this;
    }
    Vector2& Vector2::operator*=(const float other)
    {
        x *= other;
        y *= other;
        return *this;
    }
    Vector2& Vector2::operator/=(const float other)
    {
        x /= other;
        y /= other;
        return *this;
    }
    Vector2& Vector2::operator+=(const float other)
    {
        x += other;
        y += other;
        return *this;
    }
    Vector2& Vector2::operator-=(const float other)
    {
        x -= other;
        y -= other;
        return *this;
    }

    float Vector2::operator[](int index) const
    {
        switch (index)
        {
            case 0:
                return x;
            case 1:
                return y;
            default:
                return 0;
        }
    }

}