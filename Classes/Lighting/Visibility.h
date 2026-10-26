#ifndef LIGHTING_VISIBILITY_H
#define LIGHTING_VISIBILITY_H

#include <vector>
#include "cocos2d.h"

namespace Lighting {

// The 2D "shadow ray": true if the straight line from a to b is not blocked by any
// platform row in MapBase::Floor. Platform rows live at y = floorBase + floorHeight*rowIndex
// and each row is a flat list of [x_start, x_end] solid spans (same data MapBase/CharacterBase
// already use for stand/fall collision via InTheBoundary).
bool IsVisible(const cocos2d::Vec2& a, const cocos2d::Vec2& b,
               const std::vector<std::vector<float>>& floorRows,
               float floorBase, float floorHeight);

}

#endif
