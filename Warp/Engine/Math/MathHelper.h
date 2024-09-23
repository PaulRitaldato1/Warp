#pragma once

#include <random>

namespace MathHelper
{
	constexpr float Pi = 3.1415926535f;

	inline float Randf(const float min, const float max)
	{
		std::random_device rd;
		std::mt19937 engine(rd());
		std::uniform_real_distribution<float> dist(min, max);

		return dist(engine);
	}

	inline int Rand(const int min, const int max)
	{
		std::random_device rd;
		std::mt19937 engine(rd());
		std::uniform_int_distribution<int> dist(min, max);

		return dist(engine);
	}

	template <typename T>
	inline T Min(const T& a, const T& b)
	{
		return a < b ? a : b;
	}

	template<typename T>
	inline T Max(const T& a, const T& b)
	{
		return a > b ? a : b;
	}

	template<typename T>
	inline T Lerp(const T& a, const T& b, float t)
	{
		return a + (b - a) * t;
	}

	template<typename T>
	inline T Clamp(const T& x, const T& low, const T& high)
	{
		return x < low ? low : (x > high ? high : x);
	}

	inline float AngleFromXY(const float x, const float y)
	{
		float theta = 0.0f;

		// Quadrant I or IV
		if (x >= 0.0f)
		{
			// If x = 0, then atanf(y/x) = +pi/2 if y > 0
			//                atanf(y/x) = -pi/2 if y < 0
			theta = atanf(y / x); // in [-pi/2, +pi/2]

			if (theta < 0.0f)
				theta += 2.0f * Pi; // in [0, 2*pi).
		}

		// Quadrant II or III
		else
			theta = atanf(y / x) + Pi; // in [0, 2*pi).

		return theta;
	}
}