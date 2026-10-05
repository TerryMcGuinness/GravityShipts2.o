#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#include "gameplay.h"
#include "ai.h"
#include "vectorfont.h"

#include <vector>

static std::string assetPath(const char* name)
{
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0) return name;
    exe[n] = '\0';
    std::string dir(exe);
    return dir.substr(0, dir.find_last_of('/')) + "/assets/" + name;
}

float diff;
float second = 1000000;

static void error_callback(int error, const char* description)
{
    fputs(description, stderr);
}

struct MenuState {
    bool singlePlayer = true;
    bool start = false;
};

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    auto* menu = static_cast<MenuState*>(glfwGetWindowUserPointer(window));
    if (menu && action == GLFW_PRESS) {
        if (key == GLFW_KEY_UP || key == GLFW_KEY_DOWN)
            menu->singlePlayer = !menu->singlePlayer;
        if (key == GLFW_KEY_ENTER) menu->start = true;
    }
}

void
openGLSetup(GLFWwindow* window) {
    float ratio;
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    ratio = width / (float) height;
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void drawGround(void) {
    glLineWidth(1.5);
    glColor3f(1.0, 1.0, 1.0);
    glBegin(GL_LINES);
    glVertex2d(-1.75 , -1.0 );
    glVertex2d( 1.75 , -1.0 );
    glEnd();
}

static void glowLines(GLenum mode, const float* xy, int n, float r, float g, float b,
                      float glow, float px) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_LINE_SMOOTH);
    const float width[3] = {6.f * px, 3.f * px, 1.3f * px};
    const float alpha[3] = {0.10f, 0.25f, 1.f};
    for (int pass = 0; pass < 3; ++pass) {
        float w = pass == 2 ? 0.5f : 0.f;
        glLineWidth(fminf(10.f, fmaxf(1.f, width[pass])));
        glColor4f(r + (1 - r) * w, g + (1 - g) * w, b + (1 - b) * w, fminf(1.f, alpha[pass] * glow));
        glBegin(mode);
        for (int i = 0; i < n; ++i) glVertex2f(xy[2 * i], xy[2 * i + 1]);
        glEnd();
    }
    glDisable(GL_LINE_SMOOTH);
    glLineWidth(1.f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

// The in-game hull outline, nose along +y before rotation, with a flickering flame.
static void menuShip(float x, float y, float heading, float scale, const float* rgb,
                     float glow, float px, double t) {
    static const float hull[6][2] = {
        {0, .02f}, {.01f, 0}, {.02f, -.02f}, {0, -.01f}, {-.02f, -.02f}, {-.01f, 0}};
    float c = cosf(heading), s = sinf(heading), pts[12];
    for (int i = 0; i < 6; ++i) {
        pts[2 * i]     = x + (hull[i][0] * c - hull[i][1] * s) * scale;
        pts[2 * i + 1] = y + (hull[i][0] * s + hull[i][1] * c) * scale;
    }
    glowLines(GL_LINE_LOOP, pts, 6, rgb[0], rgb[1], rgb[2], glow, px);
    float len = 0.022f + 0.012f * (float)fabs(sin(t * 37.0) * sin(t * 23.0));
    const float flame[3][2] = {{-.007f, -.013f}, {0, -.013f - len}, {.007f, -.013f}};
    for (int i = 0; i < 3; ++i) {
        pts[2 * i]     = x + (flame[i][0] * c - flame[i][1] * s) * scale;
        pts[2 * i + 1] = y + (flame[i][0] * s + flame[i][1] * c) * scale;
    }
    glowLines(GL_LINE_STRIP, pts, 3, 1.f, 0.45f, 0.1f, glow, px);
}

static void centered(const std::string& text, float y, float size, float r, float g, float b,
                     float glow = 1.f) {
    vfont::draw(text, -vfont::width(text, size) * 0.5f, y, size, r, g, b, glow);
}

static bool selectMode(GLFWwindow* window) {
    MenuState menu;
    glfwSetWindowUserPointer(window, &menu);

    const std::string title = "GRAVITY SHIPS";
    const float titleSize = 0.17f, titleBase = 0.40f;
    const float titleLeft = -vfont::width(title, titleSize) * 0.5f;
    const float advance = titleSize;   // 6 grid units at size/6 per unit
    struct Letter { float y, vy, flash; bool landed; };
    std::vector<Letter> letters(title.size());
    for (size_t i = 0; i < letters.size(); ++i)
        letters[i] = {1.25f + 0.11f * ((i * 7) % 5) + 0.05f * i, 0, 0, false};

    struct Star { float x, y, speed, phase; };
    std::vector<Star> stars(110);
    unsigned seed = 0x5EED1234u;
    auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.f; };
    for (auto& s : stars) s = {rnd() * 4 - 2, rnd() * 2 - 1, 0.004f + rnd() * 0.02f, rnd() * 6.3f};

    const float yellow[3] = {1.f, 1.f, 0.f}, cyan[3] = {0.f, 1.f, 1.f};
    double start = glfwGetTime(), last = start, settledAt = -1;
    float marker = 0.05f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (menu.start || glfwWindowShouldClose(window)) break;
        openGLSetup(window);
        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        const float ratio = fbw / (float)fbh, px = fmaxf(1.f, fbh / 1080.f);
        const double now = glfwGetTime(), t = now - start;
        const float dt = (float)fmin(1.0 / 30.0, now - last);
        last = now;

        // Drifting, twinkling star field.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glPointSize(fmaxf(1.f, 1.6f * px));
        glBegin(GL_POINTS);
        for (auto& s : stars) {
            s.x -= s.speed * dt;
            if (s.x < -ratio - 0.05f) s.x += 2 * ratio + 0.1f;
            float a = 0.25f + 0.35f * s.speed / 0.024f + 0.25f * sinf((float)t * 2.3f + s.phase);
            glColor4f(0.75f, 0.85f, 1.f, a);
            glVertex2f(s.x, s.y);
        }
        glEnd();
        glPointSize(1.f);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);

        // Two ships orbit the title; the far half of the orbit is dimmer and smaller.
        const float rx = -titleLeft + 0.28f, ry = 0.31f, cy = titleBase + titleSize * 0.5f;
        auto orbit = [&](int k, bool far) {
            float a = (float)t * 0.55f + k * 3.14159f;
            if ((sinf(a) > 0) != far) return;
            float x = rx * cosf(a), y = cy + ry * sinf(a);
            float hx = -rx * sinf(a), hy = ry * cosf(a);
            menuShip(x, y, atan2f(-hx, hy), far ? 1.5f : 2.2f, k ? cyan : yellow,
                     far ? 0.4f : 1.f, px, t + k);
        };
        orbit(0, true); orbit(1, true);

        // Title letters fall under gravity, bounce, then ride a gentle gravity wave.
        bool settled = true;
        for (size_t i = 0; i < title.size(); ++i) {
            Letter& L = letters[i];
            if (title[i] == ' ') continue;
            float x = titleLeft + i * advance;
            if (!L.landed) {
                L.vy -= 3.2f * dt;
                L.y += L.vy * dt;
                if (L.y <= titleBase) {
                    float impact = -L.vy;
                    L.y = titleBase;
                    L.vy = impact * 0.42f;
                    L.flash = fminf(1.f, impact / 1.5f);
                    sfx::play(sfx::Thud, x + advance * 0.4f, fminf(1.f, impact / 2.5f), (int)(i % 2));
                    if (impact < 0.25f) L.landed = true;
                }
                settled = false;
            }
            L.flash *= expf(-dt * 4.f);
            float wave = L.landed ? 0.012f * sinf((float)t * 1.8f - i * 0.5f) : 0.f;
            float u = i / (float)(title.size() - 1);
            vfont::drawChar(title[i], x, L.y + wave, titleSize,
                            1.f - u, 1.f, u, 1.f + 1.5f * L.flash);
        }
        if (settled && settledAt < 0) settledAt = t;
        float fade = settledAt < 0 ? 0.f : fminf(1.f, (float)(t - settledAt) / 0.8f);

        orbit(0, false); orbit(1, false);

        centered("A GRAVITY-WELL DUEL FOR ONE OR TWO PILOTS", titleBase - 0.1f, 0.03f,
                 0.7f, 0.8f, 1.f, 0.8f * fade);

        // Options, with a hovering ship as the cursor.
        const float optY[2] = {0.03f, -0.13f}, optSize = 0.075f;
        const char* optText[2] = {"SINGLE PLAYER", "TWO PLAYER"};
        int sel = menu.singlePlayer ? 0 : 1;
        marker += (optY[sel] - marker) * fminf(1.f, dt * 14.f);
        for (int k = 0; k < 2; ++k) {
            float pulse = k == sel ? 1.1f + 0.35f * sinf((float)t * 5.f) : 0.3f;
            const float* c = k == 0 ? yellow : cyan;
            centered(optText[k], optY[k], optSize, c[0], c[1], c[2], pulse);
        }
        float optLeft = -vfont::width(optText[0], optSize) * 0.5f;
        menuShip(optLeft - 0.11f + 0.012f * sinf((float)t * 4.f), marker + optSize * 0.5f,
                 -1.5708f, 2.f, sel ? cyan : yellow, 1.f, px, t);

        centered("UP / DOWN: CHOOSE    ENTER: START    ESC: QUIT", -0.34f, 0.032f,
                 1.f, 1.f, 1.f, 0.55f + 0.25f * sinf((float)t * 2.f));
        centered("YELLOW: Q/W ROTATE, X THRUST, Z OR C FIRE", -0.48f, 0.028f, 1, 1, 0, 0.7f);
        centered("CYAN: CPU, OR [ AND ] ROTATE, / THRUST, . OR RIGHT SHIFT FIRE", -0.56f, 0.028f,
                 0, 1, 1, 0.7f);
        centered("CONSERVATION OF MOMENTUM IS NOT OPTIONAL", -0.64f, 0.028f,
                 1, 0.5f, 0.2f, 0.45f + 0.25f * sinf((float)t * 2.f));
        centered("FIRST TO FIVE WINS", -0.72f, 0.028f, 1, 1, 1, 0.5f);

        // Ground and the two home pads, as in the game.
        const float ground[4] = {-ratio, -0.92f, ratio, -0.92f};
        glowLines(GL_LINES, ground, 2, 1, 1, 1, 0.6f, px);
        for (int k = 0; k < 2; ++k) {
            float cx = k ? 0.9f : -0.9f;
            const float pad[8] = {cx - 0.12f, -0.92f, cx - 0.1f, -0.9f, cx + 0.1f, -0.9f, cx + 0.12f, -0.92f};
            const float* c = k ? cyan : yellow;
            glowLines(GL_LINE_STRIP, pad, 4, c[0], c[1], c[2], 0.8f, px);
        }

        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(1.0 / 60.0);
    }
    glfwSetWindowUserPointer(window, nullptr);
    return menu.singlePlayer;
}

int main(void)
{
    GLFWwindow* window;
    
    glfwSetErrorCallback(error_callback);
    
    if (!glfwInit())
        exit(EXIT_FAILURE);
    
    GLFWmonitor * 	monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    
    window = glfwCreateWindow(mode->width, mode->height, "Gravity Ships", monitor, NULL);

    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, key_callback);
    // Pin the pointer so focus-follows-mouse can't steal keyboard focus to another monitor.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    sfx::init();
    bool singlePlayer = selectMode(window);
    if (glfwWindowShouldClose(window)) {
        sfx::shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_SUCCESS;
    }
    
    ShipStatusConsts ship1Stats ;
    ShipStatusConsts ship2Stats ;
    
    ship1Stats.statTextPosX = 20 ;
    ship1Stats.statTextPosY = 590 ;
    
    ship2Stats.statTextPosX = 720 ;
    ship2Stats.statTextPosY = 590  ;
    
    ship1Stats.scoreLocation = -1.45;
    ship2Stats.scoreLocation =  1.165;
    
    Ship ship2(ship2Stats); Ship ship1(ship1Stats) ;
    
    GLfloat yellowShip[] = {1.0,1.0,0.0};
    GLfloat blueShip[]   = {0.0,1.0,1.0};
    
    ship1.id = 0;
    ship2.id = 1;
    ship1.setShipColor(yellowShip);
    ship2.setShipColor(blueShip);
    
    
    srand (static_cast <unsigned> (time(NULL)));
    resetBall(ship1);
    srand (static_cast <unsigned> (time(NULL)+1.5));
    resetBall(ship2);
    
    initialShipPosition(ship1, ship2);
    
    int img_width, img_height, img_channels;
    std::string cloudPath = assetPath("cloud.jpg");
    stbi_set_flip_vertically_on_load(1);
    unsigned char* img = stbi_load(cloudPath.c_str(), &img_width, &img_height, &img_channels, 4);
    if (!img) {
        std::cerr << "stb_image: " << cloudPath << ": " << stbi_failure_reason() << std::endl;
    } else {
        prepareCloudTexture(img, img_width, img_height);
        glGenTextures(1, &cloudTexture);
        glBindTexture(GL_TEXTURE_2D, cloudTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_width, img_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(img);
    }

    
//    ship1.score = 1;
//    resetLandingBase(ship1);
//    ship2.score = 1;
//    resetLandingBase(ship2);
    
    ship1.hitSide = false ;
    ship2.hitSide = false ;

    sfx::setShipVolume(1, singlePlayer ? 0.5f : 1.f);   // the CPU flies ship 2
    sfx::play(sfx::Start);
    bool lowFuel[2] = {false, false};
    bool gameOver = false;
    ai::Controller computer;
    std::uint64_t frame = 0;
    double nextFrame = glfwGetTime();
    
    while (!glfwWindowShouldClose(window) )
    {
       
        openGLSetup(window);
        
        ship1.displayShipScore();
        ship2.displayShipScore();
        
        ship1.rotDir = ship2.rotDir = 0;
        ship1.thrusting = ship2.thrusting = false;

        if( ship1.score != WIN_SCORE && ship2.score != WIN_SCORE )
        {
            ai::ShipControls controls;
            if (singlePlayer) controls = computer.update(ai::observe(ship2, ship1, frame));
            if(glfwGetKey(window, 'Q')) {
                ship1.rotateCounterClockWise();
            } else if(glfwGetKey(window, 'W')) {
                ship1.rotateClockWise();
            }
            if(glfwGetKey(window,'X')) {
                ship1.thrust();
            }
            if(glfwGetKey(window,'Z') || glfwGetKey(window,'C')) {
                ship1.fire();
            }

            if (singlePlayer) {
                ai::apply(ship2, controls);
            } else {
                if(glfwGetKey(window, '[')) {
                    ship2.rotateCounterClockWise();
                } else if(glfwGetKey(window, ']')) {
                    ship2.rotateClockWise();
                }
                if(glfwGetKey(window,'/')) {
                    ship2.thrust();
                }
                if(glfwGetKey(window,'.') || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT)) {
                    ship2.fire();
                }
            }
        }
        
        drawGround();

        sfx::engine(0, ship1.thrusting, ship1.rotDir, ship1.spaceShip.offset.x);
        sfx::engine(1, ship2.thrusting, ship2.rotDir, ship2.spaceShip.offset.x);
        Ship* ships[2] = {&ship1, &ship2};
        for (int i = 0; i < 2; ++i) {
            bool low = ships[i]->fuel < 0.15f * FUEL_CAPACITY;
            if (low && !lowFuel[i])
                sfx::play(sfx::Alarm, ships[i]->spaceShip.offset.x, 1.f, i);
            lowFuel[i] = low;
        }
        if (!gameOver && (ship1.score == WIN_SCORE || ship2.score == WIN_SCORE)) {
            gameOver = true;
            Ship& loser = ship1.score == WIN_SCORE ? ship2 : ship1;
            Ship& winner = ship1.score == WIN_SCORE ? ship1 : ship2;
            sfx::play(sfx::Explosion, loser.spaceShip.offset.x);
            sfx::play(sfx::Win, winner.spaceShip.offset.x);
        }
        
        if( ship1.score == WIN_SCORE ) ship2.explode();
        if( ship2.score == WIN_SCORE ) ship1.explode();
        
        if( ship2.score != WIN_SCORE ) ship1.drawShip() ;
        if( ship1.score != WIN_SCORE ) ship2.drawShip() ;
        
 
        updateShip(ship1);
        updateShip(ship2);

        ship1.updatePellets(ship2);
        ship2.updatePellets(ship1);
        ship1.drawPellets();
        ship2.drawPellets();
        
        ship1.displayShipStatus();
        ship2.displayShipStatus();
        if (singlePlayer) {
            glColor3f(0, 1, 1);
            std::string label = std::string("CPU: ") + computer.difficultyName();
            if (fabs(computer.difficulty() - computer.targetDifficulty()) > 0.01f)
                label += computer.difficulty() < computer.targetDifficulty() ? " +" : " -";
            drawText(label, 700, 548);
        }
        
        ship1.storeOtherShip(ship2.spaceShip);
        ship2.storeOtherShip(ship1.spaceShip);
        
        resolveShipCollision(ship1, ship2);
    
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Physics is tuned per-frame at 60 Hz; hold that rate on faster displays.
        ++frame;
        nextFrame += 1.0 / 60.0;
        double now = glfwGetTime();
        if (nextFrame > now)
            usleep(static_cast<useconds_t>((nextFrame - now) * 1e6));
        else if (now - nextFrame > 0.1)
            nextFrame = now;
        
    }
    sfx::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}
