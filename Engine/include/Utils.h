#pragma once
#include <cmath> 
#include <iostream>

struct Vector3;

struct Vector2 {
	float x, y;

	Vector2() : x(0), y(0) {}
	Vector2(float x, float y) : x(x), y(y) {}


	float length() const {
		return sqrt(x * x + y * y);
	}


	Vector2 operator+(const Vector2& other) const {
		return Vector2(x + other.x, y + other.y);
	}

	Vector2& operator+=(const Vector2& other) {
		x += other.x;
		y += other.y;
		return *this;
	}
	Vector2 operator-(const Vector2& other) const {
		return Vector2(x - other.x, y - other.y);
	}

	Vector2& operator-=(const Vector2& other) {
		x -= other.x;
		y -= other.y;
		return *this;
	}

	template<typename U>
	Vector2& operator+=(const U& other) {
		x += other;
		y += other;
		return *this;
	}

	template<typename U>
	Vector2& operator-=(const U& other) {
		x -= other;
		y -= other;
		return *this;
	}

	template<typename U>
	Vector2& operator*=(const U& other) {
		x *= other;
		y *= other;
		return *this;
	}

	template<typename U>
	Vector2& operator/=(const U& other) {
		x /= other;
		y /= other;
		return *this;
	}

	template<typename U>
	Vector2 operator*(const U& other) const {
		return Vector2(x * other, y * other);
	}

	template<typename U>
	Vector2 operator/(const U& other) const {
		return Vector2(x / other, y / other);
	}

	Vector2& operator=(const Vector2& other) {
		if (this == &other)
			return *this;

		x = other.x;
		y = other.y;
		return *this;
	}

	bool operator==(const Vector2& other) {
		return (x==other.x && y==other.y);
	}

	friend std::ostream& operator<<(std::ostream& os, const Vector2& vec) {
		os << '(' << vec.x << ", " << vec.y << ')';
		return os;
	}

	Vector3 toVector3() const;
};


struct Vector3 {
    float x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    float length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    Vector3& operator+=(const Vector3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    Vector3& operator-=(const Vector3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    template<typename U>
    Vector3& operator+=(const U& scalar) {
        x += scalar;
        y += scalar;
        z += scalar;
        return *this;
    }

    template<typename U>
    Vector3& operator-=(const U& scalar) {
        x -= scalar;
        y -= scalar;
        z -= scalar;
        return *this;
    }

    template<typename U>
    Vector3& operator*=(const U& scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    template<typename U>
    Vector3& operator/=(const U& scalar) {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    template<typename U>
    Vector3 operator*(const U& scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }

    template<typename U>
    Vector3 operator/(const U& scalar) const {
        return Vector3(x / scalar, y / scalar, z / scalar);
    }

    Vector3& operator=(const Vector3& other) {
        if (this == &other)
            return *this;
        x = other.x;
        y = other.y;
        z = other.z;
        return *this;
    }

    bool operator==(const Vector3& other) const {
        return (x == other.x && y == other.y && z == other.z);
    }

    friend std::ostream& operator<<(std::ostream& os, const Vector3& vec) {
        os << '(' << vec.x << ", " << vec.y << ", " << vec.z << ')';
        return os;
    }

	Vector2 toVector2() const {
        return Vector2(x, y);
    }
};

inline Vector3 Vector2::toVector3() const {
    return Vector3(x, y, 0);
}
