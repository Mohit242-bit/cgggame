#include "MapBase.h"
#include "Scene/PlayerSetup.h"
#include "Stuff/Package/PackageEvent.h"
#include "Sprite/CharacterBase.h"
#include "Sprite/AI2.h"
#include "Sprite/Player2.h"
#include "Lighting/LightingPass.h"
#include <fstream>

bool MapBase::init()
{
	if (!Layer::init())
		return false;
	visibleSize = Director::getInstance()->getVisibleSize();

	packageEvent = PackageEvent::create(this);

	return true;
}

MapBase::~MapBase()
{
	delete lighting;
}

void MapBase::update(float dt)
{
	for (auto& player : players) {
		if (player->Live == 0) {
			GameOver(player);
		}
	}
	ShotEvent();
	packageEvent->update(dt);
	packageEvent->PackageUpdate(players);

	if (lighting) lighting->Update(dt);
}

void MapBase::SpawnMuzzleFlash(const Vec2& pos)
{
	if (lighting) lighting->GetLightManager().SpawnMuzzleFlash(pos);
}

void MapBase::SpawnBulletGlow(const Vec2& pos)
{
	if (lighting) lighting->GetLightManager().SpawnBulletGlow(pos);
}

void MapBase::SpawnExplosion(const Vec2& pos)
{
	if (lighting) lighting->GetLightManager().SpawnExplosion(pos);
}

void MapBase::ShotEvent()
{
	std::vector<Bullet*> temp;
	for (auto bullet : bullets) {
		if (bullet)
			temp.push_back(bullet);
	}
	bullets = temp;

	for (auto& bullet : bullets) {

		if (bullet->getPositionX() > 12000 || bullet->getPositionX() < -6000) {
			bullet->removeFromParent();
			bullet = nullptr;
			continue;
		}
		for (auto& player : players) {

			auto rect = player->body->organ->getBoundingBox();
			Vec2 offset = player->getPosition() + player->body->getPosition();
			rect.origin += offset;

			if (rect.containsPoint(bullet->getPosition()) && player->valid && bullet->player != nullptr) {

				if (!(bullet->player->tag == player->tag)) {
					bullet->player->hitCount++;

					player->x_speed += player->defense ? bullet->hitSpeed / 10 : bullet->hitSpeed;
					player->hit = true;

					if(!player->defense) {
						auto blood = Blood::create();
						blood->setPosition(bullet->getPosition());
						platform->addChild(blood, 4);
					}

					bullet->removeFromParent();
					bullet = nullptr;
					break;
				}
			}
		}
	}
}

void MapBase::initPlayer()
{
	auto manager = UserManager::getInstance();
	CharacterBase* player1;
	CharacterBase* player2;

	if (manager->player_type[1])
		player1 = Player2::createWithTag(1, this);
	else
		player1 = AI2::create(1, this);

	if (manager->player_type[2])
		player2 = Player2::createWithTag(2, this);
	else
		player2 = AI2::create(2, this);

	platform->addChild(player1, 1);
	platform->addChild(player2, 2);

	player1->GetOpponent(player2);
	player2->GetOpponent(player1);

	players.push_back(player1);
	players.push_back(player2);

	if (!lighting) lighting = new Lighting::LightingPass(this);
}

bool MapBase::InTheBoundary(std::vector<float>& floor, float x) const
{
	for (int i = 0; i < floor.size() - 1; i += 2) {
		if (x > floor[i] && x < floor[i + 1])
			return true;
	}
	return false;
}

void MapBase::bindPlayerStateMenu(PlayerStateMenu* playerState)
{
	this->playerState = playerState;
	playerState->bindMap(this);
}

void MapBase::GameOver(CharacterBase* player)
{
	Record();
	if (!gameOver) {
		FinalizeStanding();
		auto move = EaseSineOut::create(MoveTo::create(1, initPlatformPosition));
		auto delay = DelayTime::create(1);
		auto func = CallFunc::create([&]() {
			Director::getInstance()->replaceScene(Transition::create(0.5f, PlayerSetup::create()));
			});
		player->runAction(Sequence::create(move, delay, func, nullptr));
	}
	gameOver = true;
}

void MapBase::Record()
{
	auto manager = UserManager::getInstance();
	std::string player1 = manager->player_name[1];
	std::string player2 = manager->player_name[2];
	if (manager->Standing[player1].empty()){
		manager->Standing[player1].resize(7);
		manager->Standing[player1][3] = 1;
	}
	if (manager->Standing[player2].empty()){
		manager->Standing[player2].resize(7);
		manager->Standing[player2][3] = 1;
	}
	manager->Standing[player1][4] = manager->Live - players[1]->Live;
	manager->Standing[player1][5] = players[0]->shotCount;
	manager->Standing[player1][6] = players[0]->hitCount;

	manager->Standing[player2][4] = manager->Live - players[0]->Live;
	manager->Standing[player2][5] = players[1]->shotCount;
	manager->Standing[player2][6] = players[1]->hitCount;

}

// Folds this match's raw Record() stats into each player's cumulative
// leaderboard totals (Standing.cpp) and persists them to StandingFile.txt.
// This used to be done inside the now-removed "Battle Settlement" results
// screen's one-time init(); it must run exactly once per match (GameOver()
// guards this with the gameOver flag since it's re-entered every frame
// during the death animation, unlike Record() which is safely idempotent).
void MapBase::FinalizeStanding()
{
	auto manager = UserManager::getInstance();
	std::string player1 = manager->player_name[1];
	std::string player2 = manager->player_name[2];

	for (const std::string& name : { player1, player2 }) {
		auto& stats = manager->Standing[name];
		if (stats.size() < 7) continue;
		stats[3] += 1;
		double rate = (stats[5] != 0) ? stats[6] / stats[5] : 0;
		double mark = rate * stats[4] * 100;
		stats[0] += mark;
		stats[1] = (stats[1] * (stats[3] - 1) + rate) / stats[3];
		stats[2] = (stats[2] * (stats[3] - 1) + stats[4]) / stats[3];
	}

	if (player1.empty() && player2.empty()) return;

	std::ofstream outClientFile("StandingFile.txt", std::ios::out);
	if (!outClientFile) {
		log("File Write ERROR");
		return;
	}
	for (auto& entry : manager->Standing) {
		outClientFile << entry.first;
		for (double v : entry.second) outClientFile << " " << v;
		outClientFile << std::endl;
	}
	outClientFile.close();
}

