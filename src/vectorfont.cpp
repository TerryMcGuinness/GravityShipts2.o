#include "vectorfont.h"

#include <GLFW/glfw3.h>
#include <ctype.h>
#include <math.h>

namespace vfont {

namespace {

// Glyphs on a 4-wide, 6-tall grid. Each point is two digits "xy"; strokes are
// separated by '|'.
const char* glyph(char c) {
    switch (toupper((unsigned char)c)) {
    case 'A': return "00 04 26 44 40|03 43";
    case 'B': return "00 06 36 45 44 33 03|33 42 41 30 00";
    case 'C': return "45 36 16 05 01 10 30 41";
    case 'D': return "00 06 26 44 42 20 00";
    case 'E': return "46 06 00 40|03 33";
    case 'F': return "46 06 00|03 33";
    case 'G': return "45 36 16 05 01 10 30 41 43 23";
    case 'H': return "00 06|40 46|03 43";
    case 'I': return "06 46|26 20|00 40";
    case 'J': return "46 41 30 10 01 02";
    case 'K': return "00 06|46 03 40";
    case 'L': return "06 00 40";
    case 'M': return "00 06 23 46 40";
    case 'N': return "00 06 40 46";
    case 'O': return "10 01 05 16 36 45 41 30 10";
    case 'P': return "00 06 46 43 03";
    case 'Q': return "10 01 05 16 36 45 42 20 10|22 40";
    case 'R': return "00 06 46 43 03|13 40";
    case 'S': return "46 06 03 43 40 00";
    case 'T': return "06 46|26 20";
    case 'U': return "06 00 40 46";
    case 'V': return "06 20 46";
    case 'W': return "06 00 23 40 46";
    case 'X': return "00 46|06 40";
    case 'Y': return "06 24 46|24 20";
    case 'Z': return "06 46 00 40";
    case '0': return "00 06 46 40 00|00 46";
    case '1': return "15 26 20|00 40";
    case '2': return "06 46 43 03 00 40";
    case '3': return "06 46 40 00|03 43";
    case '4': return "06 03 43|46 40";
    case '5': return "46 06 03 43 40 00";
    case '6': return "46 06 00 40 43 03";
    case '7': return "06 46 40";
    case '8': return "00 06 46 40 00|03 43";
    case '9': return "40 46 06 03 43";
    case '-': return "13 33";
    case '+': return "13 33|22 24";
    case '/': return "00 46";
    case ':': return "21 22|24 25";
    case '.': return "20 21";
    case ',': return "21 10";
    case '!': return "26 22|20 21";
    case '>': return "15 33 11";
    case '<': return "35 13 31";
    case '[': return "36 16 10 30";
    case ']': return "16 36 30 10";
    case '(': return "36 15 11 30";
    case ')': return "16 35 31 10";
    default:  return "";
    }
}

const float ADVANCE = 6.f;   // grid units per character, including spacing

void strokes(char c, float x, float y, float unit) {
    const char* s = glyph(c);
    bool open = false;
    for (; *s; ++s) {
        if (*s == '|') { if (open) glEnd(); open = false; continue; }
        if (*s == ' ' || !s[1]) continue;
        if (!open) { glBegin(GL_LINE_STRIP); open = true; }
        glVertex2f(x + (s[0] - '0') * unit, y + (s[1] - '0') * unit);
        ++s;
    }
    if (open) glEnd();
}

float pixelScale() {
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    return fmaxf(1.f, vp[3] / 1080.f);
}

} // namespace

float width(const std::string& text, float size) {
    if (text.empty()) return 0;
    float unit = size / 6.f;
    return (text.size() * ADVANCE - 2.f) * unit;
}

static void drawWith(void (*body)(const void*, float), const void* ctx, float size,
                     float r, float g, float b, float glow) {
    float px = pixelScale() * fmaxf(0.6f, size / 0.06f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_LINE_SMOOTH);
    glColor4f(r, g, b, 0.08f * glow);
    glLineWidth(fminf(10.f, 7.f * px));
    body(ctx, size);
    glColor4f(r, g, b, 0.22f * glow);
    glLineWidth(fminf(10.f, 3.5f * px));
    body(ctx, size);
    glColor4f(r + (1 - r) * 0.55f, g + (1 - g) * 0.55f, b + (1 - b) * 0.55f, fminf(1.f, glow));
    glLineWidth(fmaxf(1.f, 1.4f * px));
    body(ctx, size);
    glDisable(GL_LINE_SMOOTH);
    glLineWidth(1.f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glColor4f(1, 1, 1, 1);
}

namespace {
struct TextArgs { const std::string* text; float x, y; };
struct CharArgs { char c; float x, y; };

void textBody(const void* p, float size) {
    const TextArgs& a = *static_cast<const TextArgs*>(p);
    float unit = size / 6.f;
    for (size_t i = 0; i < a.text->size(); ++i)
        strokes((*a.text)[i], a.x + i * ADVANCE * unit, a.y, unit);
}

void charBody(const void* p, float size) {
    const CharArgs& a = *static_cast<const CharArgs*>(p);
    strokes(a.c, a.x, a.y, size / 6.f);
}
}

void draw(const std::string& text, float x, float y, float size,
          float r, float g, float b, float glow) {
    TextArgs args{&text, x, y};
    drawWith(textBody, &args, size, r, g, b, glow);
}

void drawChar(char c, float x, float y, float size, float r, float g, float b, float glow) {
    CharArgs args{c, x, y};
    drawWith(charBody, &args, size, r, g, b, glow);
}

}
