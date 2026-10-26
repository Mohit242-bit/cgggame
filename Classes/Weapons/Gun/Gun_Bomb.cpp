#include "Gun_Bomb.h"
#include "Sprite/CharacterBase.h"
#include "Weapons/Bullet/Bomb.h"

Gun_Bomb* Gun_Bomb::create()
{
    auto gun = new Gun_Bomb();
    if (gun && gun->init()) {
        return gun;
    }
    CC_SAFE_DELETE(gun);
    return NULL;
}

Gun_Bomb* Gun_Bomb::clone()
{
    return Gun_Bomb::create();
}

Sprite* Gun_Bomb::RightGun()
{
    return Sprite::createWithSpriteFrameName("bomb.png");
}

bool Gun_Bomb::init()
{
    if (!GunBase::init())
        return false;

    gun_right = Sprite::createWithSpriteFrameName("bomb.png");

    anchor = Vec2(0.5, 0.5);
    initRotation = 30.0f;
    shotInterval = 1.2;
    recoilSpeed = 100;
    bulletSpeed = 900;
    hitSpeed = 0;
    bulletClip = 3;
    bulletCount = 0;

    gun_right->setAnchorPoint(anchor);
    gun_right->setRotation(initRotation);
    return true;
}

void Gun_Bomb::Shot(MapBase* map, bool right)
{
    GunBase::Shot(map, right);
    gun_right->stopAllActions();
    CallFunc* onshot = CallFunc::create(CC_CALLBACK_0(GunBase::SetShot, this));
    CallFunc* shot = CallFunc::create(CC_CALLBACK_0(GunBase::SetBullet, this));
    CallFunc* onfire = CallFunc::create([&]() {fire = !fire; });
    auto aim = RotateTo::create(0, 0);
    auto up = RotateTo::create(0.1, -30);
    auto down = RotateTo::create(0.1, 0);
    auto delay = RotateTo::create(0.9, 0);
    auto back = RotateTo::create(0.3, 30);
    auto seq_shot = Sequence::create(onshot, onfire, aim, shot, up, down, onfire, delay, back, onshot, nullptr);
    gun_right->runAction(seq_shot);
}

void Gun_Bomb::Change(GunBase* throwgun, bool right)
{
    if (right) {
        GunBase::Change(throwgun, right);
        if (throwgun->gun_right)GunThrow(throwgun, this->isFlippedX(), right);
        gun_right->stopAllActions();
        CallFunc* onchange = CallFunc::create([&]() {onShot = !onShot; });
        CallFunc* Change = CallFunc::create([&]() {change = !change; });
        CallFunc* disappear = CallFunc::create([&]() {gun_right->setVisible(false); });
        CallFunc* appear = CallFunc::create([&]() {gun_right->setVisible(true); });

        auto moveback = RotateTo::create(0, 0);
        auto throwaway = RotateTo::create(0.15, -30);
        auto movedown = RotateTo::create(0.3, initRotation);
        auto moveup1 = RotateTo::create(0.1, -100);
        auto moveup2 = RotateTo::create(0.1, 130);
        auto moveup3 = RotateTo::create(0.1, 0);
        auto delay = RotateTo::create(1, 0);
        auto back = RotateTo::create(0.3, 30);
        auto seq_change = Sequence::create(onchange, Change, disappear, moveback, throwaway, movedown, appear, moveup1, moveup2, moveup3, Change, delay, back, onchange, nullptr);
        gun_right->runAction(seq_change);
    }
    else {
        if (throwgun->gun_left) GunThrow(throwgun, this->isFlippedX(), right);
    }
}

Sequence* Gun_Bomb::RaiseHand(bool withgun)
{
    auto delay = MoveBy::create(1, Vec2(0, 0));
    auto down = EaseSineOut::create(MoveTo::create(0.3, Vec2(0, 0)));
    if (withgun) {
        return Sequence::create(MoveTo::create(0, Vec2(70, 14)), delay, down, nullptr);
    }
    else {
        return Sequence::create(MoveTo::create(0, Vec2(15, -5)), delay, down, nullptr);
    }
}

void Gun_Bomb::Delay(bool right)
{
    gun_right->setRotation(0);
    CallFunc* onshot = CallFunc::create(CC_CALLBACK_0(GunBase::SetShot, this));
    auto aim = RotateTo::create(0, 0);
    auto delay = RotateTo::create(0.9, 0);
    auto back = RotateTo::create(0.3, 30);
    auto seq_shot = Sequence::create(onshot, aim, delay, back, onshot, nullptr);
    gun_right->runAction(seq_shot);
}

Sequence* Gun_Bomb::HoldingOn(bool withgun)
{
    auto delay = MoveBy::create(0.9, Vec2(0, 0));
    auto down = EaseSineOut::create(MoveTo::create(0.3, Vec2(0, 0)));
    if (withgun) {
        return Sequence::create(MoveTo::create(0, Vec2(70, 14)), delay, down, nullptr);
    }
    else {
        return Sequence::create(MoveTo::create(0, Vec2(15, -5)), delay, down, nullptr);
    }
}

void Gun_Bomb::SetBullet()
{
    GunBase::SetBullet();
    Bomb::create(player, GetPositionToBackground(true) + Vec2(50, 30));
}

void Gun_Bomb::GunThrow(GunBase* throwgun, bool flipped, bool right)
{
    right ? gunshadow_right = throwgun->RightGun() : gunshadow_left = throwgun->LeftGun();
    auto gunshadow = right ? gunshadow_right : gunshadow_left;
    gunshadow->setAnchorPoint(Vec2(0.5, 0.5));
    float speed = 1000;
    gunshadow_vx = flipped ? -speed : speed;
    gunshadow_vy = speed;
    gunshadow_vx += player->x_speed;
    gunshadow_vy += player->y_speed;
    gunshadow->setFlippedX(flipped);
    Vec2 anchor = throwgun->anchor;
    flipped ? anchor.x = 1 - anchor.x : anchor.x = anchor.x;
    gunshadow->setAnchorPoint(anchor);
    flipped ? gunshadow->setRotation(30) : gunshadow->setRotation(-30);
    gunshadow->setPosition(GetPositionToBackground(right));
    map->platform->addChild(gunshadow, 1);
    float angle = rand() % 100 + 130;
    auto rotate = RepeatForever::create(RotateBy::create(0.5, flipped ? angle : -angle));
    gunshadow->runAction(rotate);
}

void Gun_Bomb::update(float dt)
{
    GunBase::update(dt);
    if (gunshadow_right || gunshadow_left) {
        gunshadow_vy -= dt * 4000;
    }
    if (gunshadow_right) {
        Vec2 point = gunshadow_right->getPosition();
        point.x += gunshadow_vx * dt;
        point.y += gunshadow_vy * dt;
        gunshadow_right->setPosition(point);
        if (point.y < map->death_line) {
            gunshadow_right->removeFromParent();
            gunshadow_right = nullptr;
        }
    }
    if (gunshadow_left) {
        Vec2 point = gunshadow_left->getPosition();
        point.x += gunshadow_vx * 1.1 * dt;
        point.y += gunshadow_vy * 1.1 * dt;
        gunshadow_left->setPosition(point);
        if (point.y < map->death_line) {
            gunshadow_left->removeFromParent();
            gunshadow_left = nullptr;
        }
    }
}
