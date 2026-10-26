#ifndef LIGHTING_LIGHT_SOURCE_H
#define LIGHTING_LIGHT_SOURCE_H

#include "cocos2d.h"

// Self-contained module (FA2 continuation of the FA1 ReSTIR/ELCD/AHTR research survey).
// Nothing outside Classes/Lighting/ depends on internals here beyond the small
// MapBase::SpawnMuzzleFlash/SpawnBulletGlow wrappers declared in Map/MapBase.h.
namespace Lighting {

enum class LightType {
    MuzzleFlash,
    BulletGlow,
    Explosion
};

// An ephemeral 2D light: a muzzle flash or bullet glow. Lifetimes are expressed in
// seconds but chosen to match the frame-count budget from the FA1 poster (3-frame
// muzzle flash, 1-frame bullet glow at 60fps).
struct LightSource {
    int id = -1;
    cocos2d::Vec2 position;
    cocos2d::Color4F color = cocos2d::Color4F::WHITE;
    float radius = 0.f;
    float initialLifetime = 0.f;
    float remainingLifetime = 0.f;
    LightType type = LightType::BulletGlow;

    // kappa_l(t) from the ELCD theorem: 1.0 when freshly spawned, 0.0 once expired.
    float confidence() const {
        if (initialLifetime <= 0.f) return 0.f;
        return cocos2d::clampf(remainingLifetime / initialLifetime, 0.f, 1.f);
    }
};

}

#endif
