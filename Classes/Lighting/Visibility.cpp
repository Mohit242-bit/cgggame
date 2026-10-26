#include "Lighting/Visibility.h"
#include <algorithm>

namespace Lighting {

static bool RowBlocks(const std::vector<float>& row, float x)
{
    for (size_t i = 0; i + 1 < row.size(); i += 2) {
        if (x > row[i] && x < row[i + 1]) return true;
    }
    return false;
}

bool IsVisible(const cocos2d::Vec2& a, const cocos2d::Vec2& b,
               const std::vector<std::vector<float>>& floorRows,
               float floorBase, float floorHeight)
{
    if (floorHeight == 0.f || std::abs(b.y - a.y) < 0.0001f) return true;

    float yLow = std::min(a.y, b.y);
    float yHigh = std::max(a.y, b.y);

    for (size_t row = 0; row < floorRows.size(); ++row) {
        float rowY = floorBase + floorHeight * static_cast<float>(row);
        if (rowY <= yLow || rowY >= yHigh) continue;

        float t = (rowY - a.y) / (b.y - a.y);
        float x = a.x + t * (b.x - a.x);
        if (RowBlocks(floorRows[row], x)) return false;
    }
    return true;
}

}
