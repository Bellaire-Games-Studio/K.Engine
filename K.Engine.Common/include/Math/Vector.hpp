#pragma once
#include <iostream>

namespace KDot
{
    class Vector3 {
        
        public:
            float x;
            float y;
            float z;

            static Vector3 Up;
            static Vector3 Forward;
            static Vector3 Right;
            static Vector3 Down;
            static Vector3 Back;
            static Vector3 Left;
            static Vector3 Zero;
            static Vector3 One;

            
            Vector3(int x, int y, int z);
            Vector3(float x, float y, float z);
            Vector3();
            Vector3(const Vector3& other);

            

            Vector3 operator+(const Vector3& other) const;
            Vector3 operator-(const Vector3& other) const;
            Vector3 operator*(const Vector3& other) const;
            Vector3 operator/(const Vector3& other) const;

            Vector3 operator+(float other) const;
            Vector3 operator-(float other) const;
            Vector3 operator*(float other) const;
            Vector3 operator/(float other) const;

            Vector3 Multiply(const Vector3& other) const;
            Vector3 Divide(const Vector3& other) const;

            Vector3 Multiply(float other) const;
            Vector3 Divide(float other) const;

            Vector3 operator-() const;

            Vector3& operator+=(const Vector3& other);
            Vector3& operator-=(const Vector3& other);
            Vector3& operator*=(const Vector3& other);
            Vector3& operator/=(const Vector3& other);

            Vector3& operator+=(float other);
            Vector3& operator-=(float other);
            Vector3& operator*=(float other);
            Vector3& operator/=(float other);

            float operator[](int index) const;


            bool operator==(const Vector3& other) const;
            bool operator!=(const Vector3& other) const;

            // Returns the magnitude of the vector
            float Magnitude() const;
            // Returns the square of the magnitude of the vector
            float SqrMagnitude() const;

            // Returns a normalized vector with a magnitude of 1
            Vector3 Normalize() const;
            // Returns a normalized vector with the given magnitude
            Vector3 Normalize(float magnitude) const;
            // Returns the dot product between two vectors
            float Dot(const Vector3& other) const;
            // Returns the angle between two vectors in radians
            Vector3 Cross(const Vector3& other) const;
            // Linear interpolation between two vectors
            Vector3 Lerp(const Vector3& other, float t) const;
            // Spherical linear interpolation between two vectors
            Vector3 Slerp(const Vector3& other, float t) const;
            
            // String representation of the vector
            std::string ToString() const;
            // Returns the vector as an array
            
        protected:
            float data[3];
    };
    class Vector2 {

        public:
            float x;
            float y;

            static Vector2 Up;
            
            static Vector2 Right;
            static Vector2 Down;
            
            static Vector2 Left;
            static Vector2 Zero;
            static Vector2 One;

            Vector2(int x, int y);
            Vector2(float x, float y);
            Vector2();
            Vector2(const Vector2& other);

            

            Vector2 operator+(const Vector2& other) const;
            Vector2 operator-(const Vector2& other) const;
            Vector2 operator*(const Vector2& other) const;
            Vector2 operator/(const Vector2& other) const;

            Vector2 operator+(float other) const;
            Vector2 operator-(float other) const;
            Vector2 operator*(float other) const;
            Vector2 operator/(float other) const;

            Vector2 Multiply(const Vector2& other) const;
            Vector2 Divide(const Vector2& other) const;

            Vector2 Multiply(float other) const;
            Vector2 Divide(float other) const;

            static Vector2 Multiply(const Vector2& left, const Vector2& right);
            static Vector2 Divide(const Vector2& left, const Vector2& right);
            Vector2 operator-() const;

            Vector2& operator+=(const Vector2& other);
            Vector2& operator-=(const Vector2& other);
            Vector2& operator*=(const Vector2& other);
            Vector2& operator/=(const Vector2& other);

            Vector2& operator+=(float other);
            Vector2& operator-=(float other);
            Vector2& operator*=(float other);
            Vector2& operator/=(float other);

            float operator[](int index) const;
    };
    class Vector4 {
            
            public:
                float x;
                float y;
                float z;
                float w;

                Vector4(float x, float y, float z, float w);
                Vector4();

                Vector4 operator+(const Vector4& other) const;

    };

}