#pragma once

#include "PinkMath.h"

namespace pink
{
// Direct port of Nose.js
class Nose
{
public:
    void prepare (int tractMaxLength)
    {
        maxLength = (int) std::floor ((28.0 / 44.0) * (double) tractMaxLength);
        left.assign ((std::size_t) maxLength, 0.0);
        leftJunction.assign ((std::size_t) maxLength + 1, 0.0);
        right.assign ((std::size_t) maxLength, 0.0);
        rightJunction.assign ((std::size_t) maxLength + 1, 0.0);
        reflection.assign ((std::size_t) maxLength + 1, 0.0);
        diameter.assign ((std::size_t) maxLength, 0.0);
        amplitude.assign ((std::size_t) maxLength, 0.0);
        amplitudeMax.assign ((std::size_t) maxLength, 0.0);
        reflectionValue = 0.0;
        reflectionNew = 0.0;
    }

    void onTractUpdate (int tractLength, double velumTarget)
    {
        length = (int) std::floor ((28.0 / 44.0) * (double) tractLength);
        start = tractLength - length + 1;

        for (int index = 0; index < length && index < maxLength; ++index)
        {
            const double interpolation = (double) index / (double) length;
            const double value = interpolation < 0.5 ? 0.4 + 1.6 * (2.0 * interpolation)
                                                     : 0.5 + 1.5 * (2.0 - 2.0 * interpolation);
            diameter[(std::size_t) index] = std::min (value, 1.9);
        }

        for (int index = 0; index < length && index < maxLength; ++index)
        {
            const double a = diameter[(std::size_t) index] * diameter[(std::size_t) index];
            amplitude[(std::size_t) index] = a;

            if (index > 0)
            {
                const double prev = amplitude[(std::size_t) index - 1];
                reflection[(std::size_t) index] = (prev - a) / (prev + a);
            }
        }

        if (length > 0)
            diameter[0] = velumTarget;
    }

    void reset()
    {
        std::fill (left.begin(), left.end(), 0.0);
        std::fill (leftJunction.begin(), leftJunction.end(), 0.0);
        std::fill (right.begin(), right.end(), 0.0);
        std::fill (rightJunction.begin(), rightJunction.end(), 0.0);
    }

    int maxLength = 0;
    int length = 0;
    int start = 0;
    double fade = 1.0;
    double offset = 0.8;

    std::vector<double> left, leftJunction, right, rightJunction;
    std::vector<double> reflection, diameter, amplitude, amplitudeMax;
    double reflectionValue = 0.0;
    double reflectionNew = 0.0;
};

// Port of Transient.js. The web version computes Math.pow(-2, t*exp) which is
// NaN in JS (negative base, fractional exponent); the original Pink Trombone
// used 2^(-exp*t). We use the original formula on purpose.
struct Transient
{
    Transient (int pos, double seconds)
        : position (pos), startTime (seconds)
    {
    }

    double amplitude() const noexcept { return strength * std::pow (2.0, -timeAlive * exponent); }
    bool isAlive() const noexcept { return timeAlive < lifetime; }
    void update (double seconds) noexcept { timeAlive = seconds - startTime; }

    int position;
    double startTime;
    double timeAlive = 0.0;
    double lifetime = 0.2;
    double strength = 0.3;
    double exponent = 200.0;
};
} // namespace pink
