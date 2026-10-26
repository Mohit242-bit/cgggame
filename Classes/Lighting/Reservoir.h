#ifndef LIGHTING_RESERVOIR_H
#define LIGHTING_RESERVOIR_H

#include "cocos2d.h"
#include <algorithm>

namespace Lighting {

// A weighted reservoir (RIS), the core data structure of ReSTIR (Bitterli et al. 2020).
// Each tile keeps one of these: instead of testing every candidate light, candidates are
// stochastically folded in one at a time so the reservoir ends up holding a single
// representative light whose probability of being chosen is proportional to its weight.
struct Reservoir {
    int lightId = -1;
    cocos2d::Vec2 position;
    cocos2d::Color4F color = cocos2d::Color4F(0.f, 0.f, 0.f, 0.f);
    float radius = 0.f;
    float weightSum = 0.f;
    float M = 0.f; // number of samples folded in so far

    // Candidate generation pass: fold one new candidate light into the reservoir.
    bool Update(int id, float weight, const cocos2d::Vec2& pos, const cocos2d::Color4F& c, float r)
    {
        if (weight <= 0.f) return false;
        weightSum += weight;
        M += 1.f;
        float rnd = cocos2d::RandomHelper::random_real<float>(0.f, 1.f);
        if (rnd * weightSum <= weight) {
            lightId = id;
            position = pos;
            color = c;
            radius = r;
            return true;
        }
        return false;
    }

    // Temporal reuse pass: fold in a (possibly confidence-decayed) previous-frame reservoir.
    // confidenceScale is ELCD's kappa_l(t) -- 1.0 reproduces plain ReSTIR, 0.0 drops a sample
    // whose light has since expired instead of letting it linger (the ghost-frame bug).
    void CombineTemporal(const Reservoir& prev, float confidenceScale, float mCap)
    {
        if (prev.lightId < 0 || prev.M <= 0.f || confidenceScale <= 0.f) return;

        float cappedM = std::min(prev.M, mCap);
        float w = prev.weightSum * (cappedM / prev.M) * confidenceScale;
        if (w <= 0.f) return;

        weightSum += w;
        M += cappedM;
        float rnd = cocos2d::RandomHelper::random_real<float>(0.f, 1.f);
        if (rnd * weightSum <= w) {
            lightId = prev.lightId;
            position = prev.position;
            color = prev.color;
            radius = prev.radius;
        }
    }

    // Unbiased RIS estimator of the selected sample's contribution.
    float FinalWeight() const
    {
        return M > 0.f ? weightSum / M : 0.f;
    }
};

}

#endif
