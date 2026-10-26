#ifndef MAP_BASE_H
#define MAP_BASE_H

#include "cocos2d.h"
#include "Layer/PlayerStateMenu.h"
#include "Weapons/Bullet/Bullet.h"
#include "Stuff/Blood.h"
#include "Manager/GameManager.h"
#include "Manager/UserManager.h"
#include "Scene/Transition.h"


USING_NS_CC;

class PackageEvent;

class CharacterBase;

namespace Lighting { class LightingPass; }

class MapBase : public Layer
{
public:

	bool init() override;

	~MapBase();

	CREATE_FUNC(MapBase);

	void update(float dt) override;

	// FA2 ephemeral-lighting demo (Classes/Lighting/): fired from Bullet.cpp on bullet
	// spawn. No-ops safely before the lighting pass is set up in initPlayer().
	void SpawnMuzzleFlash(const Vec2& pos);
	void SpawnBulletGlow(const Vec2& pos);
	void SpawnExplosion(const Vec2& pos);

	virtual void ShotEvent();


	virtual void initPlayer();

	bool InTheBoundary(std::vector<float>& floor, float x) const;

	void bindPlayerStateMenu(PlayerStateMenu* playerState);

	void GameOver(CharacterBase* player);

	void Record();

	void FinalizeStanding();

	Sprite* platform;
	Sprite* backLayer;

	float floor_height;
	float floor_base;
	float death_line;
	std::vector<std::vector<float>> Floor;
	std::vector<Bullet*>bullets;
	std::vector<CharacterBase*> players;
	PackageEvent* packageEvent;
	PlayerStateMenu* playerState = nullptr;

	Lighting::LightingPass* lighting = nullptr;
protected:

	bool gameOver = false;
	
	Size visibleSize;

	Size platformSize;

	Vec2 initPlatformPosition;
	Vec2 initbackLayerPosition;

};

#endif