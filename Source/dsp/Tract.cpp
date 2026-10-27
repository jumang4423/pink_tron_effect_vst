#include "Tract.h"

namespace pink
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kBaseSectionRate = 44100.0;
}

void Tract::prepare (double sampleRate)
{
    sampleRateValue = std::max (8000.0, sampleRate);
    lengthScale = sampleRateValue / kBaseSectionRate;
    maxLength = (int) std::ceil (88.0 * lengthScale) + 8;
    damping = std::pow (0.999, 1.0 / lengthScale); // same decay per second at any rate

    right.assign ((std::size_t) maxLength, 0.0);
    rightJunction.assign ((std::size_t) maxLength + 1, 0.0);
    left.assign ((std::size_t) maxLength, 0.0);
    leftJunction.assign ((std::size_t) maxLength + 1, 0.0);
    reflection.assign ((std::size_t) maxLength + 1, 0.0);
    reflectionNew.assign ((std::size_t) maxLength + 1, 0.0);
    amplitude.assign ((std::size_t) maxLength, 0.0);
    amplitudeMax.assign ((std::size_t) maxLength, 0.0);
    diameter.assign ((std::size_t) maxLength, 0.0);
    diameterRest.assign ((std::size_t) maxLength, 0.0);

    nose.prepare (maxLength);
    transients.clear();
    obstructionLast = -1;
    obstructionNew = -1;

    length = 0;
    tongueIndex = 12.9;
    tongueDiameter = 2.43;
    updateTractLength ((int) std::llround (44.0 * lengthScale));
    updateReflection();
}

void Tract::reset()
{
    std::fill (right.begin(), right.end(), 0.0);
    std::fill (rightJunction.begin(), rightJunction.end(), 0.0);
    std::fill (left.begin(), left.end(), 0.0);
    std::fill (leftJunction.begin(), leftJunction.end(), 0.0);

    nose.reset();
    transients.clear();
}

void Tract::updateTractLength (int newLength)
{
    const double tongueRatio = (length > 0) ? tongueIndex / (double) length : (12.9 / 44.0);

    length = std::clamp (newLength, 4, maxLength);
    bladeStart = (int) std::floor ((10.0 / 44.0) * (double) length);
    tipStart = (int) std::floor ((32.0 / 44.0) * (double) length);
    lipStart = (int) std::floor ((39.0 / 44.0) * (double) length);

    tongueIndexMin = (double) bladeStart + 2.0;
    tongueIndexMax = (double) tipStart - 3.0;

    // The reference resets the tongue to 12.9/44 * length here; we keep the
    // tongue where the user put it (relative position) instead.
    tongueIndex = tongueRatio * (double) length;

    nose.onTractUpdate (length, velumTarget);

    for (int index = 0; index < length; ++index)
    {
        double value;
        if ((double) index < (7.0 / 44.0) * (double) length - 0.5)
            value = 0.6;
        else if ((double) index < (12.0 / 44.0) * (double) length)
            value = 1.1;
        else
            value = 1.5;

        diameter[(std::size_t) index] = value;
        diameterRest[(std::size_t) index] = value;
    }
}

void Tract::setTongueDiameter (double v) noexcept
{
    tongueDiameter = clampd (v, tongueDiameterMin, tongueDiameterMax);
}

void Tract::setTongueIndex (double v) noexcept
{
    const double interpolated = clampd ((tongueDiameter - tongueDiameterMin) / (tongueDiameterMax - tongueDiameterMin), 0.0, 1.0);
    const double inverted = 1.0 - interpolated;
    const double straightened = std::pow (inverted, 0.58) - 0.2 * (std::pow (inverted, 2.0) - inverted);

    const double center = (tongueIndexMin + tongueIndexMax) / 2.0;
    const double centerOffset = straightened * (tongueIndexMax - tongueIndexMin) / 2.0;

    tongueIndex = clampd (v, center - centerOffset, center + centerOffset);
}

double Tract::process (const ParamSamples& p, double bufferInterpolation, double seconds)
{
    const int currentTractLength = (int) std::llround (p.tractLength * lengthScale);
    if (currentTractLength != length)
        updateTractLength (currentTractLength);

    setTongueDiameter (p.tongueDiameter);
    setTongueIndex (p.tongueIndex);

    processTransients (seconds);
    processConstrictions (previousConstrictions, p);

    const bool updateAmplitudes = (++amplitudeCounter % 10) == 0;

    double outputSample = processLips (p, bufferInterpolation, updateAmplitudes);
    outputSample += processNose (bufferInterpolation, updateAmplitudes);

    if (! std::isfinite (outputSample))
    {
        reset();
        outputSample = 0.0;
    }

    return outputSample;
}

void Tract::processTransients (double seconds)
{
    for (int index = (int) transients.size() - 1; index >= 0; --index)
    {
        auto& transient = transients[(std::size_t) index];

        if (transient.position >= 0 && transient.position < maxLength)
            left[(std::size_t) transient.position] += transient.amplitude();

        transient.update (seconds);

        if (! transient.isAlive())
            transients.erase (transients.begin() + index);
    }
}

void Tract::processConstrictions (const ConstrictionSet& constrictions, const ParamSamples& p)
{
    // Fricative noise generated at constrictions (Tract.js _processConstrictions)
    for (int slot = 0; slot < constrictions.length; ++slot)
    {
        const auto c = constrictions.get (slot);
        if (! c.has_value())
            continue;

        if (c->index >= 2.0 && c->index <= (double) length && c->diameter > 0.0)
        {
            double noise = p.glottis;
            noise *= p.noiseModulator * 0.66;

            const double thinness = clampd (8.0 * (0.7 - c->diameter), 0.0, 1.0);
            const double openness = clampd (30.0 * (c->diameter - 0.3), 0.0, 1.0);
            const double ness = thinness * openness;
            noise *= ness / 2.0;

            const int lowerIndex = (int) std::floor (c->index);
            const double lowerWeight = c->index - (double) lowerIndex;
            const double lowerNoise = noise * lowerWeight;

            const int upperIndex = lowerIndex + 1;
            const double upperWeight = (double) upperIndex - c->index;
            const double upperNoise = noise * upperWeight;

            const int i0 = lowerIndex + 1;
            const int i1 = upperIndex + 1;

            if (i0 >= 0 && i0 < maxLength)
            {
                right[(std::size_t) i0] += lowerNoise;
                left[(std::size_t) i0] += lowerNoise;
            }

            if (i1 >= 0 && i1 < maxLength)
            {
                right[(std::size_t) i1] += upperNoise;
                left[(std::size_t) i1] += upperNoise;
            }
        }
    }
}

double Tract::processLips (const ParamSamples& p, double bufferInterpolation, bool updateAmplitudes)
{
    rightJunction[0] = left[0] * glottisReflection + p.glottis;
    leftJunction[(std::size_t) length] = right[(std::size_t) (length - 1)] * lipReflection;

    for (int index = 1; index < length; ++index)
    {
        const double interpolation = lerp (bufferInterpolation,
                                           reflection[(std::size_t) index],
                                           reflectionNew[(std::size_t) index]);
        const double offset = interpolation * (right[(std::size_t) index - 1] + left[(std::size_t) index]);

        rightJunction[(std::size_t) index] = right[(std::size_t) index - 1] - offset;
        leftJunction[(std::size_t) index] = left[(std::size_t) index] + offset;
    }

    const double leftInterpolation = lerp (bufferInterpolation, leftReflectionNew, leftReflectionValue);
    leftJunction[(std::size_t) nose.start] = leftInterpolation * right[(std::size_t) (nose.start - 1)]
                                             + (leftInterpolation + 1.0) * (nose.left[0] + left[(std::size_t) nose.start]);

    const double rightInterpolation = lerp (bufferInterpolation, rightReflectionNew, rightReflectionValue);
    rightJunction[(std::size_t) nose.start] = rightInterpolation * left[(std::size_t) nose.start]
                                              + (rightInterpolation + 1.0) * (nose.left[0] + right[(std::size_t) (nose.start - 1)]);

    const double noseInterpolation = lerp (bufferInterpolation, nose.reflectionNew, nose.reflectionValue);
    nose.rightJunction[0] = noseInterpolation * nose.left[0]
                            + (noseInterpolation + 1.0) * (left[(std::size_t) nose.start] + right[(std::size_t) (nose.start - 1)]);

    for (int index = 0; index < length; ++index)
    {
        right[(std::size_t) index] = rightJunction[(std::size_t) index] * damping;
        left[(std::size_t) index] = leftJunction[(std::size_t) index + 1] * damping;

        if (updateAmplitudes)
        {
            const double sum = std::abs (left[(std::size_t) index] + right[(std::size_t) index]);
            amplitudeMax[(std::size_t) index] = sum > amplitudeMax[(std::size_t) index]
                                                    ? sum
                                                    : amplitudeMax[(std::size_t) index] * 0.999;
        }
    }

    return right[(std::size_t) (length - 1)];
}

double Tract::processNose (double bufferInterpolation, bool updateAmplitudes)
{
    (void) bufferInterpolation;

    nose.leftJunction[(std::size_t) nose.length] = nose.right[(std::size_t) (nose.length - 1)] * lipReflection;

    for (int index = 1; index < nose.length; ++index)
    {
        const double offset = nose.reflection[(std::size_t) index]
                              * (nose.left[(std::size_t) index] + nose.right[(std::size_t) index - 1]);

        nose.leftJunction[(std::size_t) index] = nose.left[(std::size_t) index] + offset;
        nose.rightJunction[(std::size_t) index] = nose.right[(std::size_t) index - 1] - offset;
    }

    for (int index = 0; index < nose.length; ++index)
    {
        nose.left[(std::size_t) index] = nose.leftJunction[(std::size_t) index + 1] * nose.fade;
        nose.right[(std::size_t) index] = nose.rightJunction[(std::size_t) index] * nose.fade;

        if (updateAmplitudes)
        {
            const double sum = std::abs (nose.left[(std::size_t) index] + nose.right[(std::size_t) index]);
            nose.amplitudeMax[(std::size_t) index] = sum > nose.amplitudeMax[(std::size_t) index]
                                                         ? sum
                                                         : nose.amplitudeMax[(std::size_t) index] * 0.999;
        }
    }

    return nose.right[(std::size_t) (nose.length - 1)];
}

void Tract::update (double seconds, const ConstrictionSet& constrictions)
{
    updateTract();
    updateTransients (seconds);

    if (nose.length > 0)
    {
        nose.diameter[0] = velumTarget;
        nose.amplitude[0] = velumTarget * velumTarget;
    }

    updateReflection();
    updateConstrictions (constrictions);
}

void Tract::updateTract()
{
    obstructionNew = -1;

    for (int index = 0; index < length; ++index)
        if (diameter[(std::size_t) index] <= 0.0)
            obstructionNew = index;
}

void Tract::updateTransients (double seconds)
{
    if (nose.amplitude[0] < 0.05)
    {
        if (obstructionLast > -1 && obstructionNew == -1)
            transients.emplace_back (obstructionLast, seconds);

        obstructionLast = obstructionNew;
    }
}

void Tract::updateDiameterRest()
{
    for (int index = bladeStart; index < lipStart; ++index)
    {
        const double interpolation = (tongueIndex - (double) index) / (double) (tipStart - bladeStart);
        const double angle = 1.1 * kPi * interpolation;
        const double dia = 2.0 + (tongueDiameter - 2.0) / 1.5;

        double curve = (1.5 - dia + gridOffset) * std::cos (angle);

        if (index == bladeStart - 2 || index == lipStart - 1)
            curve *= 0.8;

        if (index == bladeStart || index == lipStart - 2)
            curve *= 0.94;

        diameterRest[(std::size_t) index] = 1.5 - curve;
    }
}

void Tract::updateConstrictions (const ConstrictionSet& constrictions)
{
    bool needsUpdate = ! hasPreviousConstrictions
                       || constrictions.tongueIndex != previousConstrictions.tongueIndex
                       || constrictions.tongueDiameter != previousConstrictions.tongueDiameter
                       || velumOverride != lastVelumOverride;

    if (! needsUpdate)
    {
        const int maxIndex = std::max (previousConstrictions.length, constrictions.length);

        for (int index = 0; index < maxIndex; ++index)
        {
            const auto a = constrictions.get (index);
            const auto b = previousConstrictions.get (index);

            if (a.has_value() && b.has_value())
            {
                if (a->index != b->index || a->diameter != b->diameter)
                {
                    needsUpdate = true;
                    break;
                }
            }
            else if (a.has_value() != b.has_value())
            {
                needsUpdate = true;
                break;
            }
        }
    }

    if (! needsUpdate)
        return;

    updateDiameterRest();

    for (int index = 0; index < length; ++index)
        diameter[(std::size_t) index] = diameterRest[(std::size_t) index];

    velumTarget = 0.01;

    for (int slotIndex = -1; slotIndex < constrictions.length; ++slotIndex)
    {
        Constriction c;

        if (slotIndex < 0)
        {
            c.index = tongueIndex;
            c.diameter = tongueDiameter;
        }
        else
        {
            const auto s = constrictions.get (slotIndex);
            if (s.has_value())
            {
                c = *s;
            }
            else
            {
                c.index = tongueIndex;
                c.diameter = tongueDiameter;
            }
        }

        if (c.index > (double) nose.start && c.diameter < -nose.offset)
            velumTarget = 0.4;

        if (c.diameter < -0.85 - nose.offset)
            continue;

        double newTractDiameter = std::max (0.0, c.diameter - 0.3);

        if (newTractDiameter < 3.0)
        {
            double tractIndexRange = 2.0;

            const double normalizedIndex = c.index / (double) length;

            constexpr double lowerIndex = 25.0 / 44.0;
            constexpr double lowerIndexRange = 10.0;
            const double upperIndex = (double) tipStart / (double) length;
            constexpr double upperIndexRange = 5.0;
            const double indexRange = upperIndex - lowerIndex;

            if (normalizedIndex < lowerIndex)
                tractIndexRange = lowerIndexRange;
            else if (normalizedIndex >= upperIndex)
                tractIndexRange = upperIndexRange;
            else
                tractIndexRange = lowerIndexRange - (upperIndexRange * (normalizedIndex - lowerIndex)) / indexRange;

            tractIndexRange *= (double) length / 44.0;

            const int constrictionIndex = (int) std::llround (c.index);
            const int constrictionIndexRadius = (int) std::ceil (tractIndexRange) + 1;
            const double upperBound = (double) constrictionIndex + tractIndexRange + 1.0;

            for (int tractIndex = constrictionIndex - constrictionIndexRadius;
                 (double) tractIndex < upperBound;
                 ++tractIndex)
            {
                if (tractIndex < 0 || tractIndex >= length)
                    continue;

                const double trackOffset = std::abs ((double) tractIndex - c.index) - 0.5;

                double tractDiameterScalar;
                if (trackOffset <= 0.0)
                    tractDiameterScalar = 0.0;
                else if (trackOffset > tractIndexRange)
                    tractDiameterScalar = 1.0;
                else
                    tractDiameterScalar = 0.5 * (1.0 - std::cos (kPi * trackOffset / tractIndexRange));

                const double difference = diameter[(std::size_t) tractIndex] - newTractDiameter;

                if (difference > 0.0)
                    diameter[(std::size_t) tractIndex] = newTractDiameter + difference * tractDiameterScalar;
            }
        }
    }

    if (velumOverride > 0.0)
        velumTarget = 0.4 * velumOverride;

    lastVelumOverride = velumOverride;

    previousConstrictions = constrictions;
    previousConstrictions.tongueIndex = tongueIndex;
    previousConstrictions.tongueDiameter = tongueDiameter;
    hasPreviousConstrictions = true;
}

void Tract::updateReflection()
{
    for (int index = 0; index < length; ++index)
    {
        const double a = diameter[(std::size_t) index] * diameter[(std::size_t) index];
        amplitude[(std::size_t) index] = a;

        if (index > 0)
        {
            reflection[(std::size_t) index] = reflectionNew[(std::size_t) index];
            reflectionNew[(std::size_t) index] = (a == 0.0)
                                                     ? 0.999
                                                     : (amplitude[(std::size_t) index - 1] - a)
                                                           / (amplitude[(std::size_t) index - 1] + a);
        }
    }

    leftReflectionValue = leftReflectionNew;
    rightReflectionValue = rightReflectionNew;
    nose.reflectionValue = nose.reflectionNew;

    const double sum = amplitude[(std::size_t) nose.start] + amplitude[(std::size_t) nose.start + 1] + nose.amplitude[0];

    if (sum > 1.0e-12)
    {
        leftReflectionNew = (2.0 * amplitude[(std::size_t) nose.start] - sum) / sum;
        rightReflectionNew = (2.0 * amplitude[(std::size_t) nose.start + 1] - sum) / sum;
        nose.reflectionNew = (2.0 * nose.amplitude[0] - sum) / sum;
    }
    else
    {
        leftReflectionNew = 0.0;
        rightReflectionNew = 0.0;
        nose.reflectionNew = 0.0;
    }
}
} // namespace pink
