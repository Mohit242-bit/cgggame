#include "Lighting/LightManager.h"
#include <algorithm>

namespace Lighting {

int LightManager::SpawnLight(const cocos2d::Vec2& position, LightType type, float lifetime, float radius, const cocos2d::Color4F& color)
{
    LightSource light;
    light.id = nextId_++;
    light.position = position;
    light.type = type;
    light.initialLifetime = lifetime;
    light.remainingLifetime = lifetime;
    light.radius = radius;
    light.color = color;
    lights_.push_back(light);
    return light.id;
}

void LightManager::SpawnMuzzleFlash(const cocos2d::Vec2& position)
{
    SpawnLight(position, LightType::MuzzleFlash, 3.f / 60.f, 55.f, cocos2d::Color4F(1.f, 0.75f, 0.3f, 1.f));
}

void LightManager::SpawnBulletGlow(const cocos2d::Vec2& position)
{
    SpawnLight(position, LightType::BulletGlow, 1.f / 60.f, 22.f, cocos2d::Color4F(1.f, 0.95f, 0.6f, 1.f));
}

void LightManager::SpawnExplosion(const cocos2d::Vec2& position)
{
    // 45-frame life, as specced on the research poster's "ephemeral light" table.
    SpawnLight(position, LightType::Explosion, 45.f / 60.f, 130.f, cocos2d::Color4F(1.f, 0.45f, 0.12f, 1.f));
}

void LightManager::Update(float dt)
{
    for (auto& light : lights_) {
        light.remainingLifetime -= dt;
    }
    lights_.erase(std::remove_if(lights_.begin(), lights_.end(), [](const LightSource& l) {
        return l.remainingLifetime <= 0.f;
    }), lights_.end());
}

const LightSource* LightManager::FindById(int id) const
{
    for (auto& light : lights_) {
        if (light.id == id) return &light;
    }
    return nullptr;
}

}
