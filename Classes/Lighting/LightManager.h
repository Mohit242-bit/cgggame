#ifndef LIGHTING_LIGHT_MANAGER_H
#define LIGHTING_LIGHT_MANAGER_H

#include <vector>
#include "Lighting/LightSource.h"

namespace Lighting {

// Owns the set of currently-alive ephemeral lights for one match. Not a singleton:
// MapBase owns one instance (via LightingPass) so state resets cleanly between games.
class LightManager {
public:
    void SpawnMuzzleFlash(const cocos2d::Vec2& position);
    void SpawnBulletGlow(const cocos2d::Vec2& position);
    void SpawnExplosion(const cocos2d::Vec2& position);

    // Ages every light by dt and removes anything whose lifetime has run out.
    void Update(float dt);

    const std::vector<LightSource>& GetLights() const { return lights_; }

    // nullptr if the light has expired/been removed -- used by ELCD to detect that.
    const LightSource* FindById(int id) const;

private:
    int SpawnLight(const cocos2d::Vec2& position, LightType type, float lifetime, float radius, const cocos2d::Color4F& color);

    std::vector<LightSource> lights_;
    int nextId_ = 1;
};

}

#endif
