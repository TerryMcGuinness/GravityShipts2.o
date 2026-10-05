#pragma once

#include <string>

// Stroke font in the style of vector arcade displays. Coordinates are world
// units in the current modelview; size is the cap height. Lowercase is drawn
// as uppercase and unsupported characters advance as spaces.
namespace vfont {

float width(const std::string& text, float size);

// Neon tube look: wide faint halo, mid glow, bright near-white core. Uses
// additive blending and restores the default blend state afterwards.
void draw(const std::string& text, float x, float y, float size,
          float r, float g, float b, float glow = 1.f);

// One glyph, useful for animating letters individually.
void drawChar(char c, float x, float y, float size,
              float r, float g, float b, float glow = 1.f);

}
