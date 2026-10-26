#include "Bomb.h"
#include "Sprite/CharacterBase.h"
#include "Map/MapBase.h"
#include "Stuff/Blood.h"

namespace {
	const float GRAVITY = -2400.f;
	const float FUSE_TIME = 1.1f;
	const float EXPLOSION_RADIUS = 320.f;
	const float EXPLOSION_FORCE = 1400.f;
	const float THROW_SPEED = 700.f;
}

// Deliberately independent of whichever GunBase the player currently holds --
// the bomb is a standalone action (bound to the dedicated "skill" key), not a
// gun-pickup behavior, so its throw speed is its own constant, not gun->bulletSpeed.
bool Bomb::init(CharacterBase* player, Vec2 spawnPos)
{
	if (!Sprite::init()) {
		return false;
	}
	this->player = player;

	auto bomb = Sprite::createWithSpriteFrameName("bomb.png");
	this->addChild(bomb, 0);

	bool flipped = player->isFlippedX();
	this->setFlippedX(flipped);
	currentPoint = spawnPos;
	this->setPosition(currentPoint);

	vx = (flipped ? -THROW_SPEED : THROW_SPEED) + player->x_speed;
	vy = 900.f + player->y_speed;
	fuseTime = FUSE_TIME;

	player->map->platform->addChild(this, 0);
	this->scheduleUpdate();
	return true;
}

Bomb* Bomb::create(CharacterBase* player, Vec2 spawnPos)
{
	auto bomb = new Bomb();
	if (bomb && bomb->init(player, spawnPos)) {
		bomb->autorelease();
		return bomb;
	}
	CC_SAFE_DELETE(bomb);
	return nullptr;
}

void Bomb::update(float dt)
{
	Node::update(dt);

	vy += GRAVITY * dt;
	currentPoint.x += vx * dt;
	currentPoint.y += vy * dt;
	this->setPosition(currentPoint);
	this->setRotation(this->getRotation() + 360.f * dt);

	fuseTime -= dt;
	if (fuseTime <= 0.f || currentPoint.y < player->map->death_line) {
		Explode();
	}
}

void Bomb::Explode()
{
	auto map = player->map;

	map->SpawnExplosion(currentPoint);

	auto flash = DrawNode::create();
	flash->drawDot(Vec2::ZERO, EXPLOSION_RADIUS * 0.5f, Color4F(1.f, 0.55f, 0.15f, 0.55f));
	flash->setPosition(currentPoint);
	map->platform->addChild(flash, 5);
	flash->runAction(Sequence::create(FadeOut::create(0.35f), RemoveSelf::create(), nullptr));

	for (auto& target : map->players) {
		if (!target->valid || target == player) continue;
		float dist = target->getPosition().distance(currentPoint);
		if (dist > EXPLOSION_RADIUS) continue;

		float falloff = 1.f - (dist / EXPLOSION_RADIUS);
		float dir = target->getPositionX() >= currentPoint.x ? 1.f : -1.f;
		float force = dir * EXPLOSION_FORCE * falloff;
		target->x_speed += target->defense ? force / 10.f : force;
		target->hit = true;
		player->hitCount++;

		if (!target->defense) {
			auto blood = Blood::create();
			blood->setPosition(currentPoint);
			map->platform->addChild(blood, 4);
		}
	}

	this->removeFromParent();
}
