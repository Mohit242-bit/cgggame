#ifndef LIGHTING_LIGHTING_PASS_H
#define LIGHTING_LIGHTING_PASS_H

#include <vector>
#include "cocos2d.h"
#include "Lighting/LightManager.h"
#include "Lighting/Reservoir.h"

class MapBase;

namespace Lighting {

// Orchestrates one frame of the simplified 2D ReSTIR pipeline over a fixed tile grid:
//   1. candidate generation (gather nearby lights per tile)
//   2. temporal reuse, confidence-decayed by ELCD
//   3. AHTR: tiles with more than one candidate get a one-level adaptive refinement pass
//   4. one visibility ("shadow ray") test for the tile's final chosen light
//   5. render the surviving tiles as a soft additive overlay, and report debug stats
//
// This is a scoped-down, CPU, per-tile analogue of the FA1 survey's GPU/per-pixel
// pipeline -- correct in structure, sized for a handful of combat lights rather than
// millions, which is what this 2D sprite game actually needs.
class LightingPass {
public:
    explicit LightingPass(MapBase* map);
    ~LightingPass();

    LightManager& GetLightManager() { return lightManager_; }

    void Update(float dt);

private:
    struct Tile {
        cocos2d::Rect bounds;
        Reservoir reservoir;
        Reservoir prevReservoir;
        bool hasLight = false;
    };

    void BuildTilesIfNeeded();
    void ComputeFrame();
    void Render();

    MapBase* map_ = nullptr;
    LightManager lightManager_;
    std::vector<Tile> tiles_;
    int cols_ = 0;
    int rows_ = 0;

    cocos2d::DrawNode* overlay_ = nullptr;
    cocos2d::Label* hud_ = nullptr;
    cocos2d::EventListenerKeyboard* keyListener_ = nullptr;

    bool debugVisible_ = false;
    bool elcdEnabled_ = true;
    bool ahtrEnabled_ = true;

    int shadowTestsThisFrame_ = 0;
    int activeLightsThisFrame_ = 0;
    int currentGhostStreak_ = 0;
    int maxGhostStreak_ = 0;
};

}

#endif
