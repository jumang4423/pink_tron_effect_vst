#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

// Small helpers that mirror the semantics of the original Pink Trombone
// JavaScript (see reference/pink-trombone, GPL-3.0).
namespace pink
{
inline double clampd (double v, double lo, double hi) noexcept
{
    return v <= lo ? lo : (v < hi ? v : hi);
}

inline double lerp (double t, double from, double to) noexcept
{
    return from * (1.0 - t) + to * t;
}

// Deterministic xorshift random (used for the white noise generator).
class Random
{
public:
    explicit Random (unsigned int seedValue = 0x9E3779B9u) : state (seedValue | 1u) {}

    // [-1, 1]
    double nextBipolar() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (double) state / 2147483648.0 - 1.0;
    }

private:
    unsigned int state;
};

// One-pole parameter smoother (20 ms default).
class Smoother
{
public:
    void prepare (double sampleRate, double timeSeconds = 0.02) noexcept
    {
        coeff = 1.0 - std::exp (-1.0 / (std::max (1.0, sampleRate) * std::max (0.001, timeSeconds)));
    }

    void setTarget (double v) noexcept { target = v; }
    void snap (double v) noexcept
    {
        target = v;
        current = v;
    }

    void setTimeIfNeeded (double currentValue) noexcept
    {
        if (! initialised)
        {
            snap (currentValue);
            initialised = true;
        }
    }

    double next() noexcept
    {
        current += (target - current) * coeff;
        return current;
    }

    double getCurrent() const noexcept { return current; }
    double getTarget() const noexcept { return target; }

private:
    double coeff = 1.0, current = 0.0, target = 0.0;
    bool initialised = false;
};

// RBJ band-pass section, matching the two filters the web version puts in
// front of the noise source (500 Hz / 1000 Hz, Q = 0.5).
class Biquad
{
public:
    void reset() noexcept { z1 = z2 = 0.0; }

    void setBandPass (double freq, double qFactor, double sampleRate) noexcept
    {
        const double w0 = 2.0 * 3.14159265358979323846 * (freq / sampleRate);
        const double alpha = std::sin (w0) / (2.0 * qFactor);
        const double a0 = 1.0 + alpha;

        b0 = alpha / a0;
        b1 = 0.0;
        b2 = -alpha / a0;
        a1 = (-2.0 * std::cos (w0)) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    double b0 = 0.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, z1 = 0.0, z2 = 0.0;
};
} // namespace pink
