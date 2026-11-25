// FastVocal Comp DSP engine (optical-style leveling compressor, no added colour).
// Signal flow (per sample, stereo linked):
//   sidechain filter -> rectifier -> level follower -> static law (table) -> two-stage gain memory -> gain
// The switches below select detector variants; FastVocal Comp uses one fixed set (see PluginProcessor.cpp).
// Note: only comments differ from the engine of the commercial build; the code is identical.
#pragma once

#include "StaticLaw.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace opto
{

struct Switches
{
    bool symmetric = true;       // P1: full-wave rectifier
    bool smoothLevel = true;     // P2: slow symmetric energy follower
    bool lfHold = true;          // P3: hold the crest for one 50 Hz period
    bool flatSidechain = false;  // P5: no bass emphasis in the detector
    bool lookahead = false;      // P6: 5 ms audio delay
    bool fastAttack = false;     // 2 ms attack for the energy follower (instead of 5 ms)
    bool gapHold = false;        // freeze detector and gain while the input stays below -45 dBFS
    bool rest = false;           // after 150 ms of silence, glide (50 ms) to the rest reduction

    int calibrationIndex() const { return (symmetric ? 1 : 0) | (smoothLevel ? 2 : 0) | (lfHold ? 4 : 0) | (flatSidechain ? 8 : 0) | (fastAttack ? 16 : 0); }
};

struct Params
{
    double detRel = 0.0025;
    double fastAtt = 0.00101;
    double fastRel = 0.06957;
    double slowAtt = 0.4555;
    double slowRel = 0.2250;
    double k0 = 0.7447;
    double s0 = 2.5034;
    double pk = 1.1836;
    double relAmt = 0.0;
};

constexpr double shelfF0 = 300.0, shelfDb = 5.0, holdSeconds = 0.020, lookaheadSeconds = 0.005;
constexpr double smoothAtt = 0.005, smoothRel = 0.020;
constexpr double listenHalfSeconds = 0.005, listenBoostDb = 30.0;
// dB subtracted from the detector level, index = symmetric | smoothLevel << 1 | lfHold << 2
// (keeps the amount of compression on real music the same for every detector variant)
constexpr double musicOffsetDb[8] = { 0.0, 1.10, -1.70, -1.10, 1.50, 2.60, 1.60, 2.60 };

inline double coefFor (double tau, double sr) { return tau > 0.0 ? 1.0 - std::exp (-1.0 / (tau * sr)) : 1.0; }

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        computeShelf();
        holdLength = (int) std::lround (holdSeconds * sr);
        qVal.assign ((size_t) holdLength + 2, 0.0);
        qIdx.assign ((size_t) holdLength + 2, 0);
        lookaheadLength = (int) (lookaheadSeconds * sr);   // truncation
        aGateRel = coefFor (0.020, sr);
        aRest = coefFor (0.05, sr);
        aTrack = coefFor (1.0, sr);
        minSilence = (long long) (0.15 * sr);
        gateThreshold = std::pow (10.0, -45.0 / 20.0);
        listenHalf = (int) (listenHalfSeconds * sr);
        listenG.assign ((size_t) (2 * listenHalf + 1), 0.0);
        listenL.assign (listenG.size(), 0.0);
        listenR.assign (listenG.size(), 0.0);
        delayL.assign ((size_t) std::max (lookaheadLength, 1), 0.0);
        delayR.assign ((size_t) std::max (lookaheadLength, 1), 0.0);
        aDetRel = coefFor (params.detRel, sr);
        aSmAtt = coefFor (smoothAtt, sr);
        aSmAttFast = coefFor (0.002, sr);
        aSmRel = coefFor (smoothRel, sr);
        aFa = coefFor (params.fastAtt, sr);
        aSa = coefFor (params.slowAtt, sr);
        setReleaseScale (50.0);                       // calibration does not depend on Amount
        calibrateAll();
        reset();
    }

    void reset()
    {
        z1l = z2l = z1r = z2r = 0.0;
        p = p2 = F = S = 0.0;
        gate = 0.0;
        silenceCount = minSilence;                               // the track starts as a long silence
        R = restStart;
        restTracked = false;
        qHead = qCount = 0;
        holdCounter = 0;
        std::fill (delayL.begin(), delayL.end(), 0.0);
        std::fill (delayR.begin(), delayR.end(), 0.0);
        delayIndex = 0;
        std::fill (listenG.begin(), listenG.end(), 0.0);
        std::fill (listenL.begin(), listenL.end(), 0.0);
        std::fill (listenR.begin(), listenR.end(), 0.0);
        listenIndex = 0;
        listenSum = 0.0;
    }

    void setSwitches (const Switches& s) { sw = s; }
    // Rest reduction used before any signal has been heard (automatic make-up / 0.9 for the current Compression)
    void setRestStart (double r0)
    {
        restStart = r0;
        if (! restTracked)
            R = r0;
    }

    void setAmount (double amount, int mode)
    {
        buildRow (amount, mode);
        setReleaseScale (amount);
    }
    int latencySamples() const { return sw.lookahead ? lookaheadLength : 0; }
    int lookaheadLengthSamples() const { return lookaheadLength; }
    int listenLatencySamples() const { return listenHalf; }
    double lastReductionDb() const { return lastGr; }
    double lastDryLeft() const { return lastDryL; }
    double lastDryRight() const { return lastDryR; }

    // Processes one stereo sample pair in place. outGain is linear.
    // listen: 0 = normal, 1 = added distortion only, 2 = added distortion +30 dB (modes 1 and 2 add listenHalf samples of delay).
    inline void processSample (double& left, double& right, double outGain, int listen = 0)
    {
        const int ci = sw.calibrationIndex();
        const double gr = computeReduction (left, right, calOffset[(size_t) ci] - musicOffsetDb[ci & 7], sw);
        const double g = std::pow (10.0, -gr / 20.0);
        double outL = left, outR = right;
        if (sw.lookahead)
        {
            outL = delayL[(size_t) delayIndex]; outR = delayR[(size_t) delayIndex];
            delayL[(size_t) delayIndex] = left; delayR[(size_t) delayIndex] = right;
            delayIndex = (delayIndex + 1) % lookaheadLength;
        }
        // always keep the listen buffers running so switching modes does not click
        const int W = (int) listenG.size();
        listenSum += g - listenG[(size_t) listenIndex];
        listenG[(size_t) listenIndex] = g; listenL[(size_t) listenIndex] = outL; listenR[(size_t) listenIndex] = outR;
        const int past = (listenIndex - listenHalf + W) % W;
        const double fast = listenG[(size_t) past] - listenSum / W;
        const double pastL = listenL[(size_t) past], pastR = listenR[(size_t) past];
        listenIndex = (listenIndex + 1) % W;
        if (listenIndex == 0)                                    // exact recompute once per window (no drift)
        {
            listenSum = 0.0;
            for (double v : listenG) listenSum += v;
        }
        if (listen == 0)
        {
            left = outL * g * outGain;
            right = outR * g * outGain;
        }
        else
        {
            const double boost = listen == 2 ? std::pow (10.0, listenBoostDb / 20.0) : 1.0;
            left = pastL * fast * outGain * boost;
            right = pastR * fast * outGain * boost;
            if (listen == 2)                                     // ear/speaker safety: the boosted mode never exceeds 0 dBFS
            {
                left = std::clamp (left, -1.0, 1.0);
                right = std::clamp (right, -1.0, 1.0);
            }
        }
        lastGr = gr;
        lastDryL = outL;                                         // input, delayed like the processed sound (for BYPASS)
        lastDryR = outR;
    }

private:
    double computeReduction (double a, double b, double cal, const Switches& s)
    {
        // gap hold: follower of the raw input peak (instant attack, 20 ms release)
        const double pkIn = std::max (std::abs (a), std::abs (b));
        gate = pkIn > gate ? pkIn : gate + aGateRel * (pkIn - gate);
        const bool silent = gate < gateThreshold;
        silenceCount = silent ? std::min (silenceCount + 1, minSilence + 1) : 0;
        const bool frozen = (s.gapHold && silent) || (s.rest && silenceCount >= minSilence);
        const double pOld = p, p2Old = p2, FOld = F, SOld = S;
        if (! s.flatSidechain)
        {
            const double ya = sh[0] * a + z1l; z1l = sh[1] * a - sh[3] * ya + z2l; z2l = sh[2] * a - sh[4] * ya;
            const double yb = sh[0] * b + z1r; z1r = sh[1] * b - sh[3] * yb + z2r; z2r = sh[2] * b - sh[4] * yb;
            a = ya; b = yb;
        }
        double r = s.symmetric ? std::max (std::abs (a), std::abs (b))
                               : std::max (std::max (a, 0.0), std::max (b, 0.0));
        if (s.lfHold)
            r = runningMax (r);
        const double e = r * r;
        double lev;
        if (s.smoothLevel)
        {
            const double att = s.fastAttack ? aSmAttFast : aSmAtt;
            p += (e > p ? att : aSmRel) * (e - p);
            p2 += att * (p - p2);
            lev = p2;
        }
        else
        {
            p = e > p ? e : p + aDetRel * (e - p);
            lev = p;
        }
        const double ldb = 10.0 * std::log10 (lev + 1e-20) + cal;
        const double T = std::max (0.0, lawLookup (ldb));
        F += (T > F ? aFa : aFr) * (T - F);
        S += (F > S ? aSa : aSr) * (F - S);
        if (frozen)                                              // silence: keep the detector and the gain where they were
        {
            p = pOld; p2 = p2Old; F = FOld; S = SOld;
            if (s.rest)                                          // ... and glide to the rest reduction
            {
                F += aRest * (R - F);
                S += aRest * (R - S);
            }
        }
        const double ks = params.k0 / (1.0 + std::pow (std::max (F, 0.0) / params.s0, params.pk));   // slow share: large for small reductions
        const double gr = (1.0 - ks) * F + ks * S;
        if (s.rest && ! silent)                                  // follow the average reduction while there is signal
        {
            R += aTrack * (gr - R);
            restTracked = true;
        }
        return gr;
    }

    // Sliding maximum over the last holdLength samples (monotonic queue in a fixed ring buffer, no allocation).
    double runningMax (double v)
    {
        const int cap = (int) qVal.size();
        while (qCount > 0 && qVal[(size_t) ((qHead + qCount - 1) % cap)] <= v)
            --qCount;
        const int pos = (qHead + qCount) % cap;
        qVal[(size_t) pos] = v; qIdx[(size_t) pos] = holdCounter; ++qCount;
        while (qIdx[(size_t) qHead] <= holdCounter - holdLength)
        {
            qHead = (qHead + 1) % cap; --qCount;
        }
        ++holdCounter;
        return qVal[(size_t) qHead];
    }

    double lawLookup (double ldb) const
    {
        const int m = law::numLevels;
        const double x = (ldb - law::firstLevelDb) / law::levelStepDb;
        if (x <= 0.0) return row[0];
        if (x >= m - 1) return row[(size_t) m - 1] + (row[(size_t) m - 1] - row[(size_t) m - 2]) * (x - (m - 1));
        const int k = (int) x;
        const double w = x - k;
        return row[(size_t) k] * (1.0 - w) + row[(size_t) k + 1] * w;
    }

    void setReleaseScale (double amount)
    {
        const double rs = std::exp (-params.relAmt * (std::clamp (amount, 0.0, 100.0) - 50.0) / 50.0);
        aFr = coefFor (params.fastRel * rs, sr);
        aSr = coefFor (params.slowRel * rs, sr);
    }

    void buildRow (double amount, int mode)
    {
        const auto& t = mode == 0 ? law::compressTable : law::limitTable;
        const double a = std::clamp (amount, 0.0, 100.0);
        const int i = std::min ((int) (a / law::amountStep), law::numAmounts - 2);
        const double w = (a - i * law::amountStep) / law::amountStep;
        row.resize ((size_t) law::numLevels);
        for (int j = 0; j < law::numLevels; ++j)
            row[(size_t) j] = (1.0 - w) * t[i][j] + w * t[i + 1][j];
    }

    void computeShelf()
    {
        const double A = std::pow (10.0, shelfDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * shelfF0 / sr;
        const double alpha = std::sin (w0) / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / 1.0 - 1.0) + 2.0);
        const double cw = std::cos (w0), sa = 2.0 * std::sqrt (A) * alpha;
        const double b0 = A * ((A + 1) - (A - 1) * cw + sa), b1 = 2 * A * ((A - 1) - (A + 1) * cw), b2 = A * ((A + 1) - (A - 1) * cw - sa);
        const double a0 = (A + 1) + (A - 1) * cw + sa, a1 = -2 * ((A - 1) + (A + 1) * cw), a2 = (A + 1) + (A - 1) * cw - sa;
        sh[0] = b0 / a0; sh[1] = b1 / a0; sh[2] = b2 / a0; sh[3] = a1 / a0; sh[4] = a2 / a0;
    }

    // For each switch combination: offset (dB) so that a steady 1 kHz sine of peak level L is read as L
    // (the static law was measured that way). Identity law: reduction = level + 60 dB, real dynamics.
    void calibrateAll()
    {
        std::vector<double> savedRow = row;
        row.resize ((size_t) law::numLevels);
        for (int j = 0; j < law::numLevels; ++j)
            row[(size_t) j] = law::levelStepDb * j;
        const int n = (int) (4.0 * sr), tail = (int) sr;
        for (int c = 0; c < 32; ++c)
        {
            Switches s;
            s.symmetric = (c & 1) != 0; s.smoothLevel = (c & 2) != 0; s.lfHold = (c & 4) != 0; s.flatSidechain = (c & 8) != 0; s.fastAttack = (c & 16) != 0;
            reset();
            double acc = 0.0;
            const double amp = std::pow (10.0, -20.0 / 20.0);
            for (int i = 0; i < n; ++i)
            {
                const double x = amp * std::sin (2.0 * 3.14159265358979323846 * 1000.0 * i / sr);
                const double gr = computeReduction (x, x, 0.0, s);
                if (i >= n - tail) acc += gr;
            }
            calOffset[(size_t) c] = -20.0 - (acc / tail + law::firstLevelDb);
        }
        row = savedRow;
        reset();
    }

    double sr = 44100.0;
    Params params;
    Switches sw;
    double sh[5] {};
    double z1l = 0, z2l = 0, z1r = 0, z2r = 0, p = 0, p2 = 0, F = 0, S = 0, lastGr = 0;
    double gate = 0, aGateRel = 0, gateThreshold = 0;
    double lastDryL = 0, lastDryR = 0;
    double R = 0, restStart = 0, aRest = 0, aTrack = 0;
    long long silenceCount = 0, minSilence = 6615;
    bool restTracked = false;
    double aDetRel = 0, aSmAtt = 0, aSmAttFast = 0, aSmRel = 0, aFa = 0, aFr = 0, aSa = 0, aSr = 0;
    std::vector<double> row, delayL, delayR;
    std::vector<double> qVal;
    std::vector<long long> qIdx;
    int qHead = 0, qCount = 0;
    long long holdCounter = 0;
    int holdLength = 882, lookaheadLength = 220, delayIndex = 0;
    double calOffset[32] {};
    std::vector<double> listenG, listenL, listenR;
    int listenHalf = 220, listenIndex = 0;
    double listenSum = 0.0;
};

} // namespace opto
