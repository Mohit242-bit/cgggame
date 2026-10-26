#ifndef _SETTING_
#define _SETTING_
#include "cocos2d.h"
#include "ui/CocosGUI.h"
#include "Transition.h"
#include "editor-support/cocostudio/SimpleAudioEngine.h"
#include <array>

using namespace CocosDenshion;
using namespace cocos2d;
using namespace ui;

class Setting : public Scene
{
public:
	static cocos2d::Scene* createScene();

	virtual bool init();

	// a selector callback
	void buttonCallBack(Ref* ref, cocos2d::ui::Widget::TouchEventType type);

	// implement the "static create()" method manually
	CREATE_FUNC(Setting);

private:
	struct KeyBindSlot {
		int player;              // 1 or 2 -- which of GameManager::map1/map2
		std::string action;      // "up","down","left","right","shot","skill"
		Vec2 pos;                // centre of the baked key icon, in 2560x1440 image space
		Label* overlay = nullptr; // only shown once rebound away from the default
	};

	std::array<KeyBindSlot, 12> keySlots;
	KeyBindSlot* rebindingSlot = nullptr;

	void SetupKeyBinding(const Vec2& origin, const Size& visibleSize);
	std::string KeyCodeToString(EventKeyboard::KeyCode code);
	void onKeyPressed(EventKeyboard::KeyCode keycode, Event* event);
};
#endif
