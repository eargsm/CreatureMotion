#pragma once

namespace Maths
{
	using Vec3 = RE::NiPoint3;

	inline constexpr float kPi = 3.14159265f;

	inline float Flat(const Vec3& a_v) { return std::hypot(a_v.x, a_v.y); }

	inline Vec3 Unit(const Vec3& a_v, const Vec3& a_fallback)
	{
		const float length = a_v.Length();
		return length > 1e-4f ? a_v * (1.0f / length) : a_fallback;
	}

	inline float Ramp(float a_value, float a_from, float a_to)
	{
		if (a_to <= a_from) {
			return a_value >= a_to ? 1.0f : 0.0f;
		}
		const float t = std::clamp((a_value - a_from) / (a_to - a_from), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	inline Vec3 Heading(float a_yaw) { return { std::sin(a_yaw), std::cos(a_yaw), 0.0f }; }

	inline float Yaw(const Vec3& a_v) { return std::atan2(a_v.x, a_v.y); }

	inline float Random(float a_low, float a_high)
	{
		static std::mt19937 engine{ std::random_device{}() };
		return std::uniform_real_distribution<float>(a_low, (std::max)(a_high, a_low))(engine);
	}
}
