#ifndef BOMB_H
#define BOMB_H

#include "cocos2d.h"

USING_NS_CC;

class CharacterBase;

// A thrown, gravity-arced explosive. Unlike Bullet, it does not register
// itself with MapBase::bullets -- it owns its whole lifecycle (flight,
// fuse timer, area-damage explosion, and the resulting light) itself.
class Bomb : public Sprite
{
public:

	bool init(CharacterBase* player, Vec2 spawnPos);

	void update(float dt) override;

	static Bomb* create(CharacterBase* player, Vec2 spawnPos);

	void Explode();

	Vec2 currentPoint;
	float vx = 0.f;
	float vy = 0.f;
	float fuseTime = 0.f;

	CharacterBase* player;
};

#endif
