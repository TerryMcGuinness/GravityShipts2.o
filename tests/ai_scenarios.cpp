#include "ai.h"
#include "gameplay.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

static void error(int, const char* text) { std::cerr << "GLFW: " << text << '\n'; }

struct Scenario {
    Ship human{{0, 0, 0}}, cpu{{0, 0, 0}};
    ai::Controller controller;
    std::uint64_t frame = 0;
    bool integrateHuman = false;
    explicit Scenario(int seed) {
        srand(seed);
        human.id = 0; cpu.id = 1;
        GLfloat color[] = {1, 1, 1};
        human.setShipColor(color); cpu.setShipColor(color);
        resetBall(human); resetBall(cpu);
        initialShipPosition(human, cpu);
        human.reset({0, 5});
        human.setLandingLocation({0, 5}, 0.1f);
        human.setBallLocation({0, 6}, 0.05f);
        refresh();
    }
    void refresh() {
        cpu.storeOtherShip(human.spaceShip);
        human.storeOtherShip(cpu.spaceShip);
    }
    ai::ShipControls step() {
        glClear(GL_COLOR_BUFFER_BIT);
        auto controls = controller.update(ai::observe(cpu, human, frame++));
        cpu.rotDir = human.rotDir = 0;
        cpu.thrusting = human.thrusting = false;
        ai::apply(cpu, controls);
        if (integrateHuman) updateShip(human);
        updateShip(cpu);
        human.updatePellets(cpu);
        cpu.updatePellets(human);
        refresh();
        resolveShipCollision(human, cpu);
        return controls;
    }
    bool scoreWithin(int frames) {
        int score = cpu.score;
        for (int f = 0; f < frames; ++f) {
            // inBounds() judges touchdown on the feet built before this frame's
            // position rebuild, so verify those (they differ while spinning).
            Point2D foot2 = cpu.spaceShip.location[2], foot4 = cpu.spaceShip.location[4];
            step();
            if (cpu.score > score) {
                bool valid = cpu.onPad && std::fabs(cpu.velocity.x) < 0.001f &&
                             std::fabs(cpu.velocity.y) < 0.001f &&
                             std::fabs(foot2.y - cpu.spaceShip.landingPad[1].y) < 0.001f &&
                             std::fabs(foot4.y - cpu.spaceShip.landingPad[1].y) < 0.001f &&
                             foot4.x > cpu.spaceShip.landingPad[1].x &&
                             foot2.x < cpu.spaceShip.landingPad[2].x;
                if (!valid) std::cerr << "score without valid two-foot touchdown\n";
                return valid;
            }
        }
        std::cerr << "scenario stalled frame=" << frame << " score=" << cpu.score
                  << " goal=" << ai::goalName(controller.decision().goal)
                  << " fuel=" << cpu.fuel << " position=" << cpu.spaceShip.offset.x
                  << ',' << cpu.spaceShip.offset.y << " ball=" << cpu.spaceShip.ball.ballLocation.x
                  << ',' << cpu.spaceShip.ball.ballLocation.y << " pad="
                  << cpu.landingLocation.x << ',' << cpu.landingLocation.y << '\n';
        return false;
    }
};

static bool curated() {
    int failures = 0;
    auto check = [&failures](bool ok, const char* name) {
        std::cout << name << ": " << (ok ? "passed" : "FAILED") << '\n';
        if (!ok) ++failures;
    };
    for (int seed = 1; seed <= 5; ++seed) {
        Scenario s(seed);
        bool won = true;
        for (int stage = 0; stage < WIN_SCORE; ++stage)
            if (!s.scoreWithin(18000)) { won = false; break; }
        if (won) {
            for (int f = 0; f < 100; ++f) {
                auto controls = s.step();
                won = won && s.cpu.score == WIN_SCORE && !controls.thrust &&
                      !controls.fire && controls.rotation == ai::Rotation::None;
            }
        }
        std::cout << "full match seed=" << seed << " frames=" << s.frame << '\n';
        check(won, "full match and terminal score");
    }
    for (int seed = 1; seed <= 3; ++seed) {
        Scenario s(seed);
        initialShipPosition(s.human, s.cpu);
        resetBall(s.human); resetBall(s.cpu);
        s.integrateHuman = true;
        bool won = true;
        for (int stage = 0; stage < WIN_SCORE; ++stage)
            if (!s.scoreWithin(18000)) { won = false; break; }
        std::cout << "idle human seed=" << seed << " frames=" << s.frame << '\n';
        check(won, "match with live human physics and collisions");
    }
    {
        Scenario s(31);
        s.cpu.reset({1.65f, 0.5f});
        s.cpu.velocity = {0.001f, 0};
        s.cpu.setBallLocation({-1.6f, 0.4f}, 0.01f);
        s.refresh();
        check(s.scoreWithin(18000), "wrapped interception and landing");
    }
    {
        Scenario s(32);
        s.cpu.reset({-0.6f, 0.5f});
        s.cpu.score = STAGE5;
        s.cpu.setLandingLocation({0.5f, 0.5f}, 0.05f);
        s.cpu.setBallLocation({-0.6f, -0.4f}, 0.01f);
        s.cpu.spaceShip.ball.ballhit = s.cpu.spaceShip.ball.ballhitonce = true;
        s.cpu.velocity = {0.004f, -0.002f};
        s.cpu.ang = 0.05f;
        s.refresh();
        check(s.scoreWithin(18000), "spin recovery and narrow raised landing");
    }
    {
        Scenario s(33);
        s.cpu.reset({0, -0.1f});
        s.cpu.setLandingLocation({0, 0.4f}, 0.05f);
        s.cpu.setBallLocation({0.8f, -0.5f}, 0.02f);
        s.cpu.spaceShip.ball.ballhit = s.cpu.spaceShip.ball.ballhitonce = true;
        s.refresh();
        check(s.scoreWithin(18000), "escape from below raised pad");
    }
    {
        Scenario s(34);
        s.cpu.reset({0, 0.5f});
        s.cpu.setLandingLocation({0, 0.3f}, 0.05f);
        s.cpu.setBallLocation({0, -0.5f}, 0.02f);
        s.refresh();
        check(s.scoreWithin(18000), "ball below raised pad");
    }
    {
        Scenario s(35);
        s.cpu.fuel = 450;
        s.cpu.ammo = 0;
        bool refueled = false, reloaded = false;
        for (int f = 0; f < 12000; ++f) {
            s.step();
            reloaded = reloaded || s.cpu.ammo == AMMO_CAPACITY;
            if (s.cpu.fuel >= 1800) { refueled = true; break; }
        }
        check(refueled && reloaded, "low fuel return, refuel, and reload");
        check(s.scoreWithin(18000), "launch after refueling");
    }
    {
        Scenario s(36);
        s.cpu.reset({0, 0});
        s.cpu.setBallLocation({1, 0.5f}, 0.05f);
        s.cpu.fuel = -0.01f;
        bool valid = true;
        for (int f = 0; f < 200; ++f) {
            auto controls = s.step();
            valid = valid && !controls.thrust && !controls.fire &&
                    controls.rotation == ai::Rotation::None && std::isfinite(s.cpu.spaceShip.offset.y);
        }
        check(valid, "empty fuel remains finite without free controls");
    }
    {
        Scenario s(38);
        s.cpu.reset({-0.6f, 0.4f});
        s.cpu.setBallLocation({0.6f, 0.4f}, 0.01f);
        for (int f = 0; f < 180; ++f) s.step();
        s.cpu.setBallLocation({-0.8f, -0.4f}, 0.01f);
        s.refresh();
        check(s.scoreWithin(18000), "target relocation while moving");
    }
    {
        Scenario s(39);
        s.cpu.reset({0, 0});
        s.cpu.setBallLocation({0, 0}, 0.01f);
        s.cpu.velocity = {0.0051f, 0};
        s.cpu.inBounds();
        check(!s.cpu.spaceShip.ball.ballhit, "pickup rejects excessive horizontal speed");
        s.cpu.velocity = {0, -0.0051f};
        s.cpu.inBounds();
        check(!s.cpu.spaceShip.ball.ballhit, "pickup rejects excessive vertical speed");
        s.cpu.velocity = {0.0049f, -0.0049f};
        s.cpu.inBounds();
        check(s.cpu.spaceShip.ball.ballhit, "pickup accepts safe component speeds");
    }
    {
        Scenario s(40);
        s.cpu.reset({0, 0});
        s.cpu.setBallLocation({1, 0.5f}, 0.01f);
        s.human.pellets[0] = {{0.1f, 0.002f}, {-0.01f, 0}, 10, true, false};
        s.step();
        bool evaded = s.controller.decision().goal == ai::Goal::Evade;
        s.human.pellets[0].active = false;
        for (int f = 0; f < 100; ++f) s.step();
        check(evaded && s.controller.decision().goal == ai::Goal::Collect,
              "projectile evasion resumes productive play");
    }
    {
        Scenario s(41);
        s.cpu.reset({0, 0.4f});
        s.cpu.setBallLocation({1, 0.5f}, 0.01f);
        s.human.reset({0.5f, 0});
        s.human.setLandingLocation({0.5f, -0.3f}, 0.075f);
        s.human.spaceShip.ball.ballhit = true;
        s.refresh();
        s.step();
        bool blocked = s.controller.decision().goal == ai::Goal::Block;
        for (int f = 0; f < 280; ++f) s.step();
        check(blocked && s.controller.decision().goal == ai::Goal::Collect,
              "blocking commitment expires");
    }
    {
        Scenario s(37);
        s.cpu.reset({0, 0});
        s.cpu.setBallLocation({1, 0.5f}, 0.05f);
        s.human.reset({0, 0.3f});
        s.refresh();
        bool fired = false, recoil = false;
        for (int f = 0; f < 240; ++f) {
            auto o = ai::observe(s.cpu, s.human, s.frame++);
            auto controls = s.controller.update(o);
            auto velocity = s.cpu.velocity;
            int ammo = s.cpu.ammo;
            ai::apply(s.cpu, controls);
            if (s.cpu.ammo < ammo) {
                fired = true;
                recoil = s.cpu.velocity.y < velocity.y;
                int after = s.cpu.ammo;
                s.cpu.fire();
                check(s.cpu.ammo == after, "fire cooldown uses production method");
                break;
            }
            updateShip(s.cpu);
            s.cpu.updatePellets(s.human);
            s.refresh();
        }
        check(fired && recoil, "attack fires with real ammo and recoil");
    }
    return failures == 0;
}

static bool delivery(int seed, int stage, int difficulty, std::vector<double>& times) {
    srand(seed);
    Ship human({0, 0, 0}), cpu({0, 0, 0});
    human.id = 0; cpu.id = 1;
    GLfloat color[] = {1, 1, 1};
    human.setShipColor(color); cpu.setShipColor(color);
    resetBall(human); resetBall(cpu);
    initialShipPosition(human, cpu);
    cpu.score = stage;
    resetBall(cpu);
    // Park the non-interfering opponent outside play, including its pad.
    human.reset({0, 5});
    human.setLandingLocation({0, 5}, 0.1f);
    human.setBallLocation({0, 6}, 0.05f);
    human.score = stage;
    cpu.storeOtherShip(human.spaceShip);
    ai::Controller controller;
    // Exercise the same smoothed levels even at stages where a two-point lead
    // would already be a victory. Scenario flight then uses this fixed level.
    auto calibration = ai::observe(cpu, human, 0);
    calibration.self.score = 0;
    calibration.opponent.score = difficulty;
    for (int f = 0; f < 180; ++f) {
        calibration.frame = f;
        controller.update(calibration);
    }
    for (int f = 0; f < 18000; ++f) {
        glClear(GL_COLOR_BUFFER_BIT);
        auto o = ai::observe(cpu, human, f + 180);
        ai::Decision decision{cpu.spaceShip.ball.ballhit ? ai::Goal::Land : ai::Goal::Collect};
        auto before = std::chrono::steady_clock::now();
        controller.chooseGoal(o);
        auto controls = controller.fly(o, decision);
        auto after = std::chrono::steady_clock::now();
        if (f % 16 == 0)
            times.push_back(std::chrono::duration<double, std::micro>(after - before).count());
        ai::apply(cpu, controls);
        updateShip(cpu);
        cpu.updatePellets(human);
        cpu.storeOtherShip(human.spaceShip);
        if (cpu.score > stage) {
            if (!cpu.onPad || std::fabs(cpu.velocity.x) >= 0.001f ||
                std::fabs(cpu.velocity.y) >= 0.001f) {
                std::cerr << "invalid landing transition seed=" << seed << '\n';
                return false;
            }
            return true;
        }
    }
    std::cerr << "delivery failed seed=" << seed << " stage=" << stage << " level=" << difficulty
              << " frame=18000 carrying=" << cpu.spaceShip.ball.ballhit
              << " phase=" << int(controller.phase()) << " fuel=" << cpu.fuel
              << " pos=" << cpu.spaceShip.offset.x << ',' << cpu.spaceShip.offset.y
              << " vel=" << cpu.velocity.x << ',' << cpu.velocity.y
              << " heading=" << ai::heading(cpu.spaceShip.vertices[0]) << " spin=" << cpu.ang
              << " ball=" << cpu.spaceShip.ball.ballLocation.x << ',' << cpu.spaceShip.ball.ballLocation.y
              << " pad=" << cpu.landingLocation.x << ',' << cpu.landingLocation.y << '\n';
    return false;
}

int main(int argc, char** argv) {
    int seeds = 20;
    if (argc > 1) {
        std::string_view argument(argv[1]);
        auto parsed = std::from_chars(argument.data(), argument.data() + argument.size(), seeds);
        if (argc != 2 || parsed.ec != std::errc{} ||
            parsed.ptr != argument.data() + argument.size() || seeds < 1 || seeds > 20) {
            std::cerr << "Usage: ai_scenarios [seed-count: 1..20]\n";
            return 1;
        }
    }
    glfwSetErrorCallback(error);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(64, 64, "AI scenarios", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glOrtho(-1.75, 1.75, -1, 1, -1, 1);
    std::vector<double> times;
    int failures = 0;
    if (!curated()) ++failures;
    for (int level = 0; level < 3; ++level) {
        for (int stage = 0; stage < 5; ++stage) {
            int passed = 0;
            for (int seed = 1; seed <= seeds; ++seed)
                if (delivery(seed, stage, level, times)) ++passed;
            std::cout << "level=" << level << " stage=" << stage << " deliveries=" << passed << '/' << seeds << '\n';
            if (passed < (seeds * 9 + 9) / 10) ++failures;
        }
    }
    std::sort(times.begin(), times.end());
    double p99 = times.empty() ? 0 : times[times.size() * 99 / 100];
    std::cout << "AI p99: " << p99 << " us\n";
    if (p99 >= 1000) ++failures;
    glfwDestroyWindow(window);
    glfwTerminate();
    return failures ? 1 : 0;
}
