#ifndef GUN_BOMB_H
#define GUN_BOMB_H

#include "cocos2d.h"
#include "GunBase.h"
USING_NS_CC;

// The "special weapons explosive kit" referenced in the project README --
// fires a Bomb (Weapons/Bullet/Bomb.h) instead of a Bullet. Hand choreography
// (RaiseHand/HoldingOn/Delay/Change/GunThrow) mirrors Gun_Glock's, since that
// part of holding/switching a one-handed weapon isn't bomb-specific.
class Gun_Bomb : public GunBase
{
public:

	static Gun_Bomb* create();

	Gun_Bomb* clone() override;
	Sprite* RightGun() override;

	bool init() override;

	void Shot(MapBase* map, bool right);

	void Change(GunBase* throwgun, bool right);

	Sequence* RaiseHand(bool withgun);

	void Delay(bool right);

	Sequence* HoldingOn(bool withgun);

	void SetBullet() override;

	void GunThrow(GunBase* throwgun, bool flipped, bool right);

	void update(float dt) override;

};

#endif
