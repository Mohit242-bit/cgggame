#include"Scene/Credits.h"
#include"Scene/StartScene.h"

Scene* Credits::createScene()
{
	return Credits::create();
}
bool Credits::init()
{
	if (!Scene::init())
	{
		return false;
	}
	auto visibleSize = Director::getInstance()->getVisibleSize();
	Vec2 origin = Director::getInstance()->getVisibleOrigin();

	auto background = LayerColor::create(Color4B(24, 24, 28, 255));
	this->addChild(background);

	auto title = Label::createWithSystemFont("CREDITS", "Arial", 90);
	title->setTextColor(Color4B::WHITE);
	title->setPosition(Vec2(origin.x + visibleSize.width / 2, origin.y + visibleSize.height - 180));
	this->addChild(title);

	auto developedBy = Label::createWithSystemFont("Developed by Mohit Rawat", "Arial", 56);
	developedBy->setTextColor(Color4B(200, 200, 200, 255));
	developedBy->setPosition(Vec2(origin.x + visibleSize.width / 2, origin.y + visibleSize.height / 2));
	this->addChild(developedBy);

	auto engineCredit = Label::createWithSystemFont("Built with Cocos2d-x", "Arial", 40);
	engineCredit->setTextColor(Color4B(140, 140, 140, 255));
	engineCredit->setPosition(Vec2(origin.x + visibleSize.width / 2, origin.y + visibleSize.height / 2 - 90));
	this->addChild(engineCredit);

	auto BackToMenuButton = ui::Button::create("staff/back.png", "staff/back.png");
	BackToMenuButton->setScale(1.0f);
	BackToMenuButton->addTouchEventListener(CC_CALLBACK_2(Credits::buttonCallBack, this));
	BackToMenuButton->setPosition(Vec2(origin.x + visibleSize.width * 2222 / 2560, origin.y + 1440 - 1292));
	this->addChild(BackToMenuButton);

	return true;
}

void Credits::buttonCallBack(Ref* ref, cocos2d::ui::Widget::TouchEventType type)
{
	switch (type) {
	case Widget::TouchEventType::ENDED:
		Director::getInstance()->replaceScene(Transition::create(0.5f, StartScene::create()));
		break;
	default:
		break;
	}


}
