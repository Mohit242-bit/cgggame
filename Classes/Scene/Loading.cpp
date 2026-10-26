#include "Loading.h"

Scene* Loading::createScene()
{
    auto scene = Scene::create();
    auto layer = Loading::create();
    scene->addChild(layer);
    return scene;
}

bool Loading::init()
{
    if (!Layer::init())
        return false;
    isloaddone = false;
    
    schedule(CC_SCHEDULE_SELECTOR(Loading::logic), 0.1f);
    return true;
}

void Loading::loadResources()
{
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("test.plist");
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("space.plist");
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("Player1.plist");
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("MG_gtl.plist");
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("body_fx.plist");
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("blood.plist");
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("bodyColored.plist");
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("face_and_hat.plist");
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("items.plist");
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("superJumpHalo.plist");
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("colorSpots.plist");
    CCLOG("done");

    isloaddone = true;
}

void Loading::logic(float dt)
{
    CocosDenshion::SimpleAudioEngine::getInstance()->preloadBackgroundMusic("Music/Start.mp3");
    CocosDenshion::SimpleAudioEngine::getInstance()->preloadBackgroundMusic("Music/Game.mp3");

    if (!isloaddone) {
        loadResources();
    }
    if (isloaddone) {
        
        Director::getInstance()->replaceScene(TransitionFade::create(1.f, StartScene::create()));
    }
}
