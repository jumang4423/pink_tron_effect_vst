#pragma once

#include "Nose.h"
#include "PinkMath.h"

namespace pink
{
struct Constriction
{
    double index = 0.0;
    double diameter = 0.0;
};

// Mirrors the (sparse) constrictions array of the web version: slots that are
// not enabled are "holes" and are treated like the tongue in the shaping loop.
struct ConstrictionSet
{
    static constexpr int kSlots = 4;

    std::array<std::optional<Constriction>, kSlots> slots {};
    int length = 0; // index of the last engaged slot + 1 (JS sparse array length)
    double tongueIndex = 0.0;
    double tongueDiameter = 0.0;

    void addSlot (int slot, Constriction c)
    {
        if (slot < 0 || slot >= kSlots)
            return;

        slots[(std::size_t) slot] = c;
        length = std::max (length, slot + 1);
    }

    std::optional<Constriction> get (int slot) const
    {
        if (slot < 0 || slot >= length || slot >= kSlots)
            return std::nullopt;

        return slots[(std::size_t) slot];
    }
};

// Direct port of Tract.js (see docs/pink-trombone-analysis.md)
class Tract
{
public:
    struct ParamSamples
    {
        double tractLength = 44.0;
        double tongueIndex = 12.9;
        double tongueDiameter = 2.43;
        double glottis = 0.0;        // excitation sample (the incoming audio in effect mode)
        double noiseModulator = 0.0; // provided by the glottis stage (aspiration noise model)
    };

    void prepare (double sampleRate);
    void reset();

    double process (const ParamSamples& p, double bufferInterpolation, double seconds);
    void update (double seconds, const ConstrictionSet& constrictions);

    // Nasal / velum override: 0 = original (derived from constrictions), > 0 opens the velum.
    void setVelumOverride (double v) noexcept { velumOverride = clampd (v, 0.0, 1.0); }

    // Read-only view for the UI (valid after update()).
    const std::vector<double>& diameters() const noexcept { return diameter; }
    int getLength() const noexcept { return length; }
    int getNoseStart() const noexcept { return nose.start; }
    double getTongueIndex() const noexcept { return tongueIndex; }
    double getTongueDiameter() const noexcept { return tongueDiameter; }
    double getVelumTarget() const noexcept { return velumTarget; }

    void setTongueIndex (double v) noexcept;
    void setTongueDiameter (double v) noexcept;

    static constexpr double tongueDiameterMin = 2.05;
    static constexpr double tongueDiameterMax = 3.5;

private:
    void updateTractLength (int newLength);
    void processTransients (double seconds);
    void processConstrictions (const ConstrictionSet& constrictions, const ParamSamples& p);
    double processLips (const ParamSamples& p, double bufferInterpolation, bool updateAmplitudes);
    double processNose (double bufferInterpolation, bool updateAmplitudes);
    void updateTract ();
    void updateTransients (double seconds);
    void updateDiameterRest ();
    void updateConstrictions (const ConstrictionSet& constrictions);
    void updateReflection ();

    int length = 0;
    int maxLength = 0;
    double lengthScale = 1.0;
    double sampleRateValue = 44100.0;
    double damping = 0.999;

    int bladeStart = 0, tipStart = 0, lipStart = 0;

    double lipReflection = -0.85;
    double glottisReflection = 0.75;
    double velumTarget = 0.01;
    double velumOverride = 0.0;
    double lastVelumOverride = -1.0;
    double gridOffset = 1.7;

    std::vector<double> right, rightJunction, left, leftJunction;
    double leftReflectionValue = 0.0, leftReflectionNew = 0.0;
    double rightReflectionValue = 0.0, rightReflectionNew = 0.0;
    std::vector<double> reflection, reflectionNew;
    std::vector<double> amplitude, amplitudeMax;
    std::vector<double> diameter, diameterRest;

    double tongueDiameter = 2.43;
    double tongueIndex = 12.9;
    double tongueIndexMin = 12.0;
    double tongueIndexMax = 29.0;

    Nose nose;

    std::vector<Transient> transients;
    int obstructionLast = -1;
    int obstructionNew = -1;

    ConstrictionSet previousConstrictions;
    bool hasPreviousConstrictions = false;

    int amplitudeCounter = 0;
};
} // namespace pink
