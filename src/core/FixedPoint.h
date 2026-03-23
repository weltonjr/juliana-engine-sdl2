#pragma once

#include <cstdint>

struct Fixed {
    static constexpr int FRAC_BITS = 16;
    static constexpr int32_t ONE = 1 << FRAC_BITS;

    int32_t raw;

    constexpr Fixed() : raw(0) {}
    constexpr explicit Fixed(int32_t raw_value, bool) : raw(raw_value) {}

    static constexpr Fixed FromRaw(int32_t raw_value) {
        return Fixed(raw_value, true);
    }

    static constexpr Fixed FromInt(int value) {
        return Fixed(static_cast<int32_t>(value) << FRAC_BITS, true);
    }

    static Fixed FromFloat(float value) {
        return Fixed(static_cast<int32_t>(value * ONE), true);
    }

    constexpr int ToInt() const {
        return raw >> FRAC_BITS;
    }

    float ToFloat() const {
        return static_cast<float>(raw) / ONE;
    }

    constexpr Fixed operator+(Fixed other) const {
        return FromRaw(raw + other.raw);
    }

    constexpr Fixed operator-(Fixed other) const {
        return FromRaw(raw - other.raw);
    }

    constexpr Fixed operator*(Fixed other) const {
        return FromRaw(static_cast<int32_t>(
            (static_cast<int64_t>(raw) * other.raw) >> FRAC_BITS
        ));
    }

    constexpr Fixed operator/(Fixed other) const {
        return FromRaw(static_cast<int32_t>(
            (static_cast<int64_t>(raw) << FRAC_BITS) / other.raw
        ));
    }

    constexpr Fixed operator-() const {
        return FromRaw(-raw);
    }

    constexpr Fixed& operator+=(Fixed other) { raw += other.raw; return *this; }
    constexpr Fixed& operator-=(Fixed other) { raw -= other.raw; return *this; }
    constexpr Fixed& operator*=(Fixed other) { *this = *this * other; return *this; }
    constexpr Fixed& operator/=(Fixed other) { *this = *this / other; return *this; }

    constexpr bool operator==(Fixed other) const { return raw == other.raw; }
    constexpr bool operator!=(Fixed other) const { return raw != other.raw; }
    constexpr bool operator<(Fixed other) const { return raw < other.raw; }
    constexpr bool operator>(Fixed other) const { return raw > other.raw; }
    constexpr bool operator<=(Fixed other) const { return raw <= other.raw; }
    constexpr bool operator>=(Fixed other) const { return raw >= other.raw; }

    static constexpr Fixed Zero() { return FromRaw(0); }
};
