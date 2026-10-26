#include"Scene/BackgroundChoise.h"
#include"Scene/StartScene.h"
#include"Scene/PlayerSetup.h"
#include"Manager/GameManager.h"
#include<algorithm>
#include<vector>
#include<time.h>

bool BackgroundChoise::init() {

	if (!Scene::init())
	{
		return false;
	}

	auto visibleSize = Director::getInstance()->getVisibleSize();
	Vec2 origin = Director::getInstance()->getVisibleOrigin();
	

	auto background = Sprite::create("customGameMenu/background1.png");
	background->setAnchorPoint(Vec2::ANCHOR_BOTTOM_LEFT);
	background->setPosition(origin.x, origin.y);
	auto contentSize = background->getContentSize();
	background->setScale(visibleSize.width / contentSize.width, visibleSize.height / contentSize.height);;
	this->addChild(background);

	


	auto inputBox = Sprite::create("customGameMenu/word.png");
	editbox1 = EditBox::create(Size(inputBox->getContentSize().width, inputBox->getContentSize().height), Scale9Sprite::create("customGameMenu/word.png"));
	
	editbox1->setPosition(Vec2(755.5, 1440-1045.5));
	editbox1->setPlaceHolder("");
	editbox1->setFontColor(Color3B::BLACK);
	editbox1->setInputMode(EditBox::InputMode::NUMERIC);
	editbox1->setMaxLength(5);
	editbox1->setReturnType(EditBox::KeyboardReturnType::DONE);
	editbox1->setText(std::to_string(UserManager::getInstance()->Live).c_str());
	editbox1->setDelegate(this);
	editbox1->setTouchEnabled(true);
	editbox1->setTag(1);
	this->addChild(editbox1, 2);



	auto BackToMenuButton = ui::Button::create("customGameMenu/backUnselected.png", "customGameMenu/backSelected.png");
	BackToMenuButton->setScale(1.0f);
	BackToMenuButton->addTouchEventListener(CC_CALLBACK_2(BackgroundChoise::buttonCallBack, this));
	BackToMenuButton->setPosition(Vec2(origin.x + visibleSize.width * 522.5 / 2560, origin.y + 1440-1230));
	this->addChild(BackToMenuButton);
	//continue
	auto Continue = ui::Button::create("customGameMenu/continueUnselected.png", "customGameMenu/continueSelected.png");
	Continue->setScale(1.0f);
	Continue->addTouchEventListener(CC_CALLBACK_2(BackgroundChoise::buttonCotinue, this));
	Continue->setPosition(Vec2(origin.x + visibleSize.width * 1690 / 2560, origin.y + 1440 - 1230));
	this->addChild(Continue);


	auto Random = Button::create("customGameMenu/randomUnselected.png", "customGameMenu/randomSelected.png");
	Random->setPosition(Vec2(origin.x + visibleSize.width * 717.5 / 2560, 1440 - 420));

	auto spacestation = Button::create("customGameMenu/spacestationUnselected.png", "customGameMenu/spacestationSelected.png");
	spacestation->setPosition(Vec2(origin.x + visibleSize.width * 522.5 / 2560, 1440-530));

	auto sunsetcity = Button::create("customGameMenu/sunsetcityUnselected.png", "customGameMenu/sunsetcitySelected.png");
	sunsetcity->setPosition(Vec2(origin.x + visibleSize.width * 522.5 / 2560, 1440-610 ));

	// Note: a third map ("Seele") existed here but its preview art has illegible
	// placeholder flavor text baked into the image, so it's left out of the
	// rotation entirely rather than shipped half-finished.

	Random->addClickEventListener([=](Ref* sender) {
		Random->loadTextureNormal("customGameMenu/randomSelected.png");
		spacestation->loadTextureNormal("customGameMenu/spacestationUnselected.png");
		sunsetcity->loadTextureNormal("customGameMenu/sunsetcityUnselected.png");
		srand((unsigned)time(0));
		int num = rand() % 3;
		switch (num) {
		case 0: {
			Sprite* pic5 = Sprite::create("customGameMenu/defaultPreview.png");
			pic5->setPosition(Vec2(origin.x + visibleSize.width * 1683 / 2560, 1440 - 735));
			this->addChild(pic5);
			break;
			}
		case 1: {
			Sprite* pic0 = Sprite::create("customGameMenu/spacestationPreview.png");
			pic0->setPosition(Vec2(origin.x + visibleSize.width * 1683 / 2560, 1440 - 735));
			this->addChild(pic0);
			break;
			}
		case 2: {
			Sprite* pic1 = Sprite::create("customGameMenu/sunsetcityPreview.png");
			pic1->setPosition(Vec2(origin.x + visibleSize.width * 1683 / 2560, 1440 - 735));
			this->addChild(pic1);
			break;
			}
		}
		GameManager::BackChoise = num;
		});

	spacestation->addClickEventListener([=](Ref* sender) {

		spacestation->loadTextureNormal("customGameMenu/spacestationSelected.png");
		spacestation->setLocalZOrder(2);
		Random->loadTextureNormal("customGameMenu/randomUnselected.png");
		sunsetcity->loadTextureNormal("customGameMenu/sunsetcityUnselected.png");
		sunsetcity->setLocalZOrder(0);
		auto pic = Sprite::create("customGameMenu/spacestationPreview.png");
		pic->setPosition(Vec2(origin.x + visibleSize.width * 1683 / 2560, 1440 - 735));
		this->addChild(pic);
		GameManager::BackChoise = 1;
		});

	sunsetcity->addClickEventListener([=](Ref* sender) {

		spacestation->loadTextureNormal("customGameMenu/spacestationUnselected.png");
		spacestation->setLocalZOrder(0);
		Random->loadTextureNormal("customGameMenu/randomUnselected.png");
		sunsetcity->loadTextureNormal("customGameMenu/sunsetcitySelected.png");
		sunsetcity->setLocalZOrder(2);
		auto pic = Sprite::create("customGameMenu/sunsetcityPreview.png");
		pic->setPosition(Vec2(origin.x + visibleSize.width * 1683 / 2560, 1440 - 735));
		this->addChild(pic);
		GameManager::BackChoise = 2;
		});
	this->addChild(Random);
	this->addChild(spacestation);
	this->addChild(sunsetcity);





	blacksprite = Sprite::create("customGameMenu/70buttonYello.png");
	blacksprite->setAnchorPoint(Point(0.5, 0.5));
	this->addChild(blacksprite,1);
	blacksprite->setVisible(false);

	auto _mouseListener = EventListenerMouse::create();

	_mouseListener->onMouseMove = CC_CALLBACK_1(BackgroundChoise::onMouseMove, this);

	_eventDispatcher->addEventListenerWithSceneGraphPriority(_mouseListener, this);

	return true;
}

void BackgroundChoise::buttonCallBack(Ref* ref, cocos2d::ui::Widget::TouchEventType type)
{
	switch (type) {
	case Widget::TouchEventType::ENDED:
		Director::getInstance()->replaceScene(Transition::create(0.5f,StartScene::create()));
		break;
	default:
		break;
	}
}

void BackgroundChoise::buttonCotinue(Ref* ref, cocos2d::ui::Widget::TouchEventType type)
{
	switch (type) {
	case Widget::TouchEventType::ENDED:
		applyLives();
		Director::getInstance()->replaceScene(Transition::create(0.5f,PlayerSetup::create()));
		break;
	default:
		break;
	}
}

void BackgroundChoise::editBoxReturn(EditBox* editBox)
{
	if (editBox == editbox1) {
		applyLives();
		editbox1->setFocused(false);
	}
}

void BackgroundChoise::applyLives()
{
	std::string livesText = editbox1->getText();
	int lives = 10;
	if (!livesText.empty()) {
		lives = 0;
		for (char digit : livesText) {
			if (!isdigit(static_cast<unsigned char>(digit))) {
				lives = 10;
				break;
			}
			lives = lives * 10 + int(digit - '0');
		}
	}
	UserManager::getInstance()->Live = std::max(1, std::min(lives, 10000));
}


//

void BackgroundChoise::onMouseMove(Event* event)
{
	EventMouse* e = (EventMouse*)event;
	Size visibleSize = Director::getInstance()->getVisibleSize();
	Vec2 locationInNode = convertToNodeSpace(Vec2(e->getCursorX(), e->getCursorY()));


	// Only 2 map-list rows exist now (Space Station, Sunset City), so only 2 hover
	// zones are defined -- no more phantom highlight on rows that have no map.
	Rect r1 = Rect(visibleSize.width * 132.5 / 2560, 1440-570, visibleSize.width * 780 / 2560, 80);
	Rect r2 = Rect(visibleSize.width * 132.5 / 2560, 1440-650, visibleSize.width * 780 / 2560, 80);
	CallFunc* setMove = CallFunc::create([&]() {
		move = !move;
		});
	if (r1.containsPoint(locationInNode)) {
		if (blacksprite->isVisible()) {
			if (!move) {
				auto move = EaseSineOut::create(MoveTo::create(0.08, Vec2(visibleSize.width * 522.5 / 2560, 1440 - 530)));
				auto seq = Sequence::create(setMove, move, setMove, nullptr);
				seq->setTag(1);
				blacksprite->stopActionByTag(1);
				blacksprite->runAction(seq);
			}
		}
		else {
			blacksprite->setPosition(Vec2( visibleSize.width * 522.5 / 2560, 1440 - 530));
			blacksprite->setVisible(true);
			auto fadein = FadeIn::create(0.1);
			blacksprite->runAction(fadein);
		}
	}
	else if (r2.containsPoint(locationInNode)) {
		if (blacksprite->isVisible()) {
			if (!move) {
				auto move = EaseSineOut::create(MoveTo::create(0.08, Vec2( visibleSize.width * 522.5 / 2560, 1440 - 610)));
				auto seq = Sequence::create(setMove, move, setMove, nullptr);
				seq->setTag(1);
				blacksprite->stopActionByTag(1);
				blacksprite->runAction(seq);
			}
		}
		else {
			blacksprite->setPosition(Vec2( visibleSize.width * 522.5 / 2560, 1440 - 610));
			blacksprite->setVisible(true);
			auto fadein = FadeIn::create(0.1);
			blacksprite->runAction(fadein);
		}
	}

	else {
		if (blacksprite->isVisible()) {
			CallFunc* func1 = CallFunc::create([&]() {
				blacksprite->setVisible(false);
				});
			auto fadeout = FadeOut::create(0.1f);
			auto seq = Sequence::create(fadeout, func1, nullptr);
			blacksprite->runAction(seq);
		}
	}

}
