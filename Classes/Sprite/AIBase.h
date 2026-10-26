#ifndef AI_BASE_H
#define AI_BASE_H

#include "cocos2d.h"
#include "CharacterBase.h"

USING_NS_CC;

class AIBase : public CharacterBase
{
public:

	bool init(int tag, MapBase* map);

	virtual void initBody();

	void update(float dt) override;

	void MoveEvent();

	void ShotEvent();
	void reset();

	int find_thisStep();

	bool Readytodie(CharacterBase* player);
	bool opponentReadytodie();

	void jumpTofloor(int, int);

	void FindFloor();

protected:
	bool shotLasttime;
	bool findPackage;
};

#endif
