#include "Setting.h"
#include"StartScene.h"
#include "Manager/GameManager.h"

Scene* Setting::createScene()
{
	return Setting::create();
}
bool Setting::init()
{
	if (!Scene::init())
	{
		return false;
	}
	auto visibleSize = Director::getInstance()->getVisibleSize();
	Vec2 origin = Director::getInstance()->getVisibleOrigin();

	auto background = Sprite::create("optionMenu/back.png");
	background->setAnchorPoint(Vec2::ANCHOR_BOTTOM_LEFT);
	background->setPosition(origin.x, origin.y);
	auto contentSize = background->getContentSize();
	background->setScale(visibleSize.width / contentSize.width, visibleSize.height / contentSize.height);;
	this->addChild(background);

	auto BackToMenuButton = ui::Button::create( "optionMenu/backtoUnselected.png", "optionMenu/backtoSelected.png");
	BackToMenuButton->setScale(1.0f);
	BackToMenuButton->addTouchEventListener(CC_CALLBACK_2(Setting::buttonCallBack, this));
	BackToMenuButton->setPosition(Vec2(origin.x + visibleSize.width * 2085/2560, origin.y +1440-1175));
	this->addChild(BackToMenuButton);




	auto onMusic = Button::create("optionMenu/onSelected.png", "optionMenu/onUnselected.png");
	onMusic->setPosition(Vec2(origin.x + visibleSize.width * 1935/2560, 1440 - 585));
	
	auto offMusic = Button::create("optionMenu/offUnselected.png", "optionMenu/offSelected.png");
	offMusic->setPosition(Vec2(origin.x + visibleSize.width * 2235/2560, 1440 - 585));


	auto instance_music = CocosDenshion::SimpleAudioEngine::getInstance();
	float volume_music = instance_music->getBackgroundMusicVolume();
	if (volume_music == 0) {
		onMusic->loadTextureNormal("optionMenu/onUnselected.png");
		offMusic->loadTextureNormal("optionMenu/offSelected.png");
	}



	onMusic->addClickEventListener([=](Ref* sender) {                      
		SimpleAudioEngine::getInstance()->setBackgroundMusicVolume(1.0f);
		onMusic->loadTextureNormal("optionMenu/onSelected.png");
		offMusic->loadTextureNormal("optionMenu/offUnselected.png");
		});
	
	offMusic->addClickEventListener([=](Ref* sender) {
		SimpleAudioEngine::getInstance()->setBackgroundMusicVolume(0.0f);
		onMusic->loadTextureNormal("optionMenu/onUnselected.png");
		offMusic->loadTextureNormal("optionMenu/offSelected.png");
		});

	this->addChild(onMusic);
	this->addChild(offMusic);




	auto onSound = Button::create("optionMenu/onSelected.png", "optionMenu/onUnselected.png");
	onSound->setPosition(Vec2(origin.x + visibleSize.width * 1935 / 2560, 1440 - 890));
	
	auto offSound = Button::create("optionMenu/offUnselected.png", "optionMenu/offSelected.png");
	offSound->setPosition(Vec2(origin.x + visibleSize.width * 2235 / 2560, 1440 - 890));
	
	auto instance_sound = CocosDenshion::SimpleAudioEngine::getInstance();
	float volume_sound = instance_sound->getEffectsVolume();
	if (volume_sound == 0) {
		onSound->loadTextureNormal("optionMenu/onUnselected.png");
		offSound->loadTextureNormal("optionMenu/offSelected.png");
	}



	onSound->addClickEventListener([=](Ref* sender) {
		SimpleAudioEngine::getInstance()->setEffectsVolume(1.0f);
		onSound->loadTextureNormal("optionMenu/onSelected.png");
		offSound->loadTextureNormal("optionMenu/offUnselected.png");
		});

	offSound->addClickEventListener([=](Ref* sender) {
		SimpleAudioEngine::getInstance()->setEffectsVolume(0.0f);
		onSound->loadTextureNormal("optionMenu/onUnselected.png");
		offSound->loadTextureNormal("optionMenu/offSelected.png");
		});

	this->addChild(onSound);
	this->addChild(offSound);

	SetupKeyBinding(origin, visibleSize);

	return true;

}

namespace {
	// Matches GameManager::map1/map2's hardcoded defaults exactly. Used so the
	// overlay only ever shows text once a binding genuinely differs from what's
	// already drawn into optionMenu/back.png -- never for "rebound to the same key."
	EventKeyboard::KeyCode DefaultKeyFor(int player, const std::string& action)
	{
		using Key = EventKeyboard::KeyCode;
		if (player == 1) {
			if (action == "up") return Key::KEY_UP_ARROW;
			if (action == "down") return Key::KEY_DOWN_ARROW;
			if (action == "left") return Key::KEY_LEFT_ARROW;
			if (action == "right") return Key::KEY_RIGHT_ARROW;
			if (action == "shot") return Key::KEY_1;
			return Key::KEY_2; // "skill"
		}
		if (action == "up") return Key::KEY_W;
		if (action == "down") return Key::KEY_S;
		if (action == "left") return Key::KEY_A;
		if (action == "right") return Key::KEY_D;
		if (action == "shot") return Key::KEY_J;
		return Key::KEY_K; // "skill"
	}
}

void Setting::SetupKeyBinding(const Vec2& origin, const Size& visibleSize)
{
	// Centres of the key icons already baked into optionMenu/back.png, in the
	// game's usual 2560x1440 design-image coordinate space (X from left, Y from top).
	keySlots = { {
		{1, "up",    Vec2(473.6f,  669.4f)},
		{1, "left",  Vec2(325.1f,  819.2f)},
		{1, "down",  Vec2(473.6f,  819.2f)},
		{1, "right", Vec2(624.6f,  819.2f)},
		{1, "shot",  Vec2(325.1f, 1049.6f)},
		{1, "skill", Vec2(473.6f, 1049.6f)},

		{2, "up",    Vec2(1278.7f,  669.4f)},
		{2, "left",  Vec2(1130.2f,  819.2f)},
		{2, "down",  Vec2(1278.7f,  819.2f)},
		{2, "right", Vec2(1427.2f,  819.2f)},
		{2, "shot",  Vec2(1130.2f, 1049.6f)},
		{2, "skill", Vec2(1278.7f, 1049.6f)},
	} };

	for (auto& slot : keySlots) {
		Vec2 screenPos = Vec2(origin.x + visibleSize.width * slot.pos.x / 2560.f,
			origin.y + 1440.f - slot.pos.y);

		// A small overlay label showing the currently-bound key, only drawn
		// once it differs from the default baked into the background art.
		auto& currentMap = (slot.player == 1) ? GameManager::map1 : GameManager::map2;
		EventKeyboard::KeyCode current = currentMap[slot.action];
		std::string initialText = (current != DefaultKeyFor(slot.player, slot.action))
			? KeyCodeToString(current) : "";

		slot.overlay = Label::createWithSystemFont(initialText, "Arial", 34);
		slot.overlay->setTextColor(Color4B(255, 210, 60, 255));
		slot.overlay->setPosition(screenPos + Vec2(0, -58));
		this->addChild(slot.overlay, 2);

		// Invisible clickable hit-box over the baked key icon.
		auto hit = Node::create();
		hit->setContentSize(Size(100, 100));
		hit->setAnchorPoint(Vec2(0.5f, 0.5f));
		hit->setPosition(screenPos);
		this->addChild(hit, 2);

		KeyBindSlot* slotPtr = &slot;
		auto touchListener = EventListenerTouchOneByOne::create();
		touchListener->setSwallowTouches(true);
		touchListener->onTouchBegan = [hit](Touch* touch, Event*) {
			Vec2 local = hit->convertToNodeSpace(touch->getLocation());
			return Rect(0, 0, 100, 100).containsPoint(local);
			};
		touchListener->onTouchEnded = [this, slotPtr](Touch*, Event*) {
			rebindingSlot = slotPtr;
			slotPtr->overlay->setString("press a key...");
			};
		this->getEventDispatcher()->addEventListenerWithSceneGraphPriority(touchListener, hit);
	}

	auto keyListener = EventListenerKeyboard::create();
	keyListener->onKeyPressed = CC_CALLBACK_2(Setting::onKeyPressed, this);
	this->getEventDispatcher()->addEventListenerWithSceneGraphPriority(keyListener, this);
}

void Setting::onKeyPressed(EventKeyboard::KeyCode keycode, Event* event)
{
	if (!rebindingSlot) return;

	auto& targetMap = (rebindingSlot->player == 1) ? GameManager::map1 : GameManager::map2;

	// Refuse a key already bound to a different action for the same player --
	// two actions sharing one key would make them fire together.
	for (auto& entry : targetMap) {
		if (entry.first != rebindingSlot->action && entry.second == keycode) {
			rebindingSlot->overlay->setString("in use, try again");
			return;
		}
	}

	targetMap[rebindingSlot->action] = keycode;
	bool isDefault = (keycode == DefaultKeyFor(rebindingSlot->player, rebindingSlot->action));
	rebindingSlot->overlay->setString(isDefault ? "" : KeyCodeToString(keycode));
	rebindingSlot = nullptr;
}

std::string Setting::KeyCodeToString(EventKeyboard::KeyCode code)
{
	using Key = EventKeyboard::KeyCode;
	if (code >= Key::KEY_A && code <= Key::KEY_Z) {
		char c = 'A' + (static_cast<int>(code) - static_cast<int>(Key::KEY_A));
		return std::string(1, c);
	}
	if (code >= Key::KEY_0 && code <= Key::KEY_9) {
		char c = '0' + (static_cast<int>(code) - static_cast<int>(Key::KEY_0));
		return std::string(1, c);
	}
	switch (code) {
	case Key::KEY_UP_ARROW: return "UP";
	case Key::KEY_DOWN_ARROW: return "DOWN";
	case Key::KEY_LEFT_ARROW: return "LEFT";
	case Key::KEY_RIGHT_ARROW: return "RIGHT";
	case Key::KEY_SPACE: return "SPACE";
	case Key::KEY_LEFT_SHIFT: case Key::KEY_RIGHT_SHIFT: return "SHIFT";
	case Key::KEY_LEFT_CTRL: case Key::KEY_RIGHT_CTRL: return "CTRL";
	case Key::KEY_LEFT_ALT: case Key::KEY_RIGHT_ALT: return "ALT";
	case Key::KEY_ENTER: return "ENTER";
	case Key::KEY_TAB: return "TAB";
	case Key::KEY_ESCAPE: return "ESC";
	case Key::KEY_COMMA: return ",";
	case Key::KEY_PERIOD: return ".";
	case Key::KEY_SEMICOLON: return ";";
	case Key::KEY_QUOTE: return "'";
	default: return "KEY";
	}
}


void Setting::buttonCallBack(Ref* ref, cocos2d::ui::Widget::TouchEventType type)
{
	switch (type) {
	case Widget::TouchEventType::ENDED:
		Director::getInstance()->replaceScene(Transition::create(0.5f,StartScene::create()));
		break;
	default:
		break;
	}
}
