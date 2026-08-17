#pragma once

namespace vortch {

struct Point { int x = 0; int y = 0; };
struct Size  { int w = 64; int h = 64; };

inline bool operator==(const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; }
inline bool operator==(const Size&  a, const Size&  b) { return a.w == b.w && a.h == b.h; }

} // namespace vortch
