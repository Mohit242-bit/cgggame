#include "Lighting/LightingPass.h"
#include "Lighting/Visibility.h"
#include "Map/MapBase.h"
#include <cstdio>
#include <algorithm>

namespace Lighting {

namespace {

bool TileCatchesLight(const cocos2d::Rect& bounds, const LightSource& light)
{
    cocos2d::Rect expanded(bounds.origin.x - light.radius, bounds.origin.y - light.radius,
                            bounds.size.width + 2.f * light.radius, bounds.size.height + 2.f * light.radius);
    return expanded.containsPoint(light.position);
}

cocos2d::Rect Quadrant(const cocos2d::Rect& bounds, int index)
{
    float hw = bounds.size.width / 2.f;
    float hh = bounds.size.height / 2.f;
    float x = bounds.origin.x + (index % 2) * hw;
    float y = bounds.origin.y + (index / 2) * hh;
    return cocos2d::Rect(x, y, hw, hh);
}

// Candidate-generation RIS over whatever lights overlap `bounds`.
Reservoir BuildCandidateReservoir(const std::vector<LightSource>& lights, const cocos2d::Rect& bounds,
                                   std::vector<const LightSource*>* outCandidates)
{
    Reservoir r;
    for (auto& light : lights) {
        if (!TileCatchesLight(bounds, light)) continue;
        if (outCandidates) outCandidates->push_back(&light);
        float weight = light.radius; // simple target-pdf proxy: bigger/closer-feeling lights win more often
        r.Update(light.id, weight, light.position, light.color, light.radius);
    }
    return r;
}

}

LightingPass::LightingPass(MapBase* map) : map_(map)
{
    BuildTilesIfNeeded();

    overlay_ = cocos2d::DrawNode::create();
    map_->platform->addChild(overlay_, 50);

    auto visibleSize = cocos2d::Director::getInstance()->getVisibleSize();
    hud_ = cocos2d::Label::createWithSystemFont("", "Arial", 28);
    hud_->setAnchorPoint(cocos2d::Vec2(0.f, 1.f));
    hud_->setPosition(cocos2d::Vec2(24.f, visibleSize.height - 24.f));
    hud_->setTextColor(cocos2d::Color4B(255, 255, 255, 255));
    hud_->setVisible(debugVisible_);
    map_->addChild(hud_, 1000);

    keyListener_ = cocos2d::EventListenerKeyboard::create();
    keyListener_->onKeyPressed = [this](cocos2d::EventKeyboard::KeyCode code, cocos2d::Event*) {
        using Key = cocos2d::EventKeyboard::KeyCode;
        if (code == Key::KEY_F1) {
            debugVisible_ = !debugVisible_;
            hud_->setVisible(debugVisible_);
        } else if (code == Key::KEY_F2) {
            elcdEnabled_ = !elcdEnabled_;
        } else if (code == Key::KEY_F3) {
            ahtrEnabled_ = !ahtrEnabled_;
        }
    };
    map_->getEventDispatcher()->addEventListenerWithSceneGraphPriority(keyListener_, map_);
}

LightingPass::~LightingPass()
{
}

void LightingPass::BuildTilesIfNeeded()
{
    if (!tiles_.empty()) return;

    cocos2d::Size area = map_->platform->getContentSize();
    if (area.width <= 0.f || area.height <= 0.f) {
        area = cocos2d::Director::getInstance()->getVisibleSize();
    }

    cols_ = 16;
    rows_ = 9;
    float tileW = area.width / static_cast<float>(cols_);
    float tileH = area.height / static_cast<float>(rows_);

    tiles_.resize(static_cast<size_t>(cols_) * rows_);
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            Tile& tile = tiles_[r * cols_ + c];
            tile.bounds = cocos2d::Rect(c * tileW, r * tileH, tileW, tileH);
        }
    }
}

void LightingPass::Update(float dt)
{
    lightManager_.Update(dt);
    ComputeFrame();
    Render();
}

void LightingPass::ComputeFrame()
{
    shadowTestsThisFrame_ = 0;
    activeLightsThisFrame_ = static_cast<int>(lightManager_.GetLights().size());
    bool anyGhostThisFrame = false;

    const auto& lights = lightManager_.GetLights();

    for (auto& tile : tiles_) {
        std::vector<const LightSource*> candidates;
        Reservoir combined = BuildCandidateReservoir(lights, tile.bounds, &candidates);

        // AHTR: a tile with more than one competing light is high-variance -- spend one
        // extra level of resolution on it (a 2x2 refinement) instead of settling for the
        // coarse-grid pick, rather than spending that budget on every tile uniformly.
        if (ahtrEnabled_ && candidates.size() > 1) {
            Reservoir best = combined;
            for (int q = 0; q < 4; ++q) {
                cocos2d::Rect subBounds = Quadrant(tile.bounds, q);
                Reservoir sub = BuildCandidateReservoir(lights, subBounds, nullptr);
                if (sub.FinalWeight() > best.FinalWeight()) best = sub;
            }
            combined = best;
        }

        // Temporal reuse, confidence-decayed by ELCD so an expired light's old reservoir
        // sample contributes zero weight instead of lingering for ~M_cap frames.
        if (tile.prevReservoir.lightId >= 0) {
            float confidence = 1.f;
            if (elcdEnabled_) {
                const LightSource* prevLight = lightManager_.FindById(tile.prevReservoir.lightId);
                confidence = prevLight ? prevLight->confidence() : 0.f;
            }
            combined.CombineTemporal(tile.prevReservoir, confidence, 20.f);
        }

        tile.hasLight = false;
        if (combined.lightId >= 0 && combined.FinalWeight() > 0.0001f) {
            ++shadowTestsThisFrame_;
            cocos2d::Vec2 tileCenter = tile.bounds.origin + cocos2d::Vec2(tile.bounds.size.width / 2.f, tile.bounds.size.height / 2.f);
            bool visible = IsVisible(tileCenter, combined.position, map_->Floor, map_->floor_base, map_->floor_height);
            if (visible) {
                tile.hasLight = true;
                if (lightManager_.FindById(combined.lightId) == nullptr) {
                    anyGhostThisFrame = true;
                }
            } else {
                combined.weightSum = 0.f;
            }
        }

        tile.reservoir = combined;
        tile.prevReservoir = combined;
    }

    if (anyGhostThisFrame) {
        ++currentGhostStreak_;
        maxGhostStreak_ = std::max(maxGhostStreak_, currentGhostStreak_);
    } else {
        currentGhostStreak_ = 0;
    }
}

void LightingPass::Render()
{
    overlay_->clear();
    for (auto& tile : tiles_) {
        if (!tile.hasLight) continue;
        float alpha = cocos2d::clampf(tile.reservoir.FinalWeight() / 60.f, 0.08f, 0.35f);
        cocos2d::Color4F c = tile.reservoir.color;
        c.a = alpha;
        overlay_->drawDot(tile.reservoir.position, tile.reservoir.radius * 0.35f, c);
    }

    if (!debugVisible_) return;

    char buf[320];
    std::snprintf(buf, sizeof(buf),
        "Lighting Debug (F1 toggle)\n"
        "ELCD [F2]: %s   AHTR [F3]: %s\n"
        "Active lights: %d\n"
        "Shadow tests this frame: %d / %d tiles\n"
        "Ghost streak: %d frames (max %d)",
        elcdEnabled_ ? "ON" : "OFF", ahtrEnabled_ ? "ON" : "OFF",
        activeLightsThisFrame_, shadowTestsThisFrame_, static_cast<int>(tiles_.size()),
        currentGhostStreak_, maxGhostStreak_);
    hud_->setString(buf);
}

}
