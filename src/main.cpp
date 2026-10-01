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

#include "ship.h"

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

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
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

bool start = false ;

void resetBall(Ship& ship) {
    
    Point2D setRandomBall;
    bool distance = false ;
    while(!distance) {
        setRandomBall.x = randomRange(-1.3,1.3);
        rand();rand();rand();
        setRandomBall.y = randomRange(-0.9, 0.9);
        if( sqrt( pow(setRandomBall.x,2)+pow(setRandomBall.y, 2)) > 0.03 )
            distance = true ;
    }
    
    float ballSize ;
    if( ship.score == STAGE1 ) ballSize = 0.05 ;
    if( ship.score == STAGE2 ) ballSize = 0.04 ;
    if( ship.score == STAGE3 ) ballSize = 0.03 ;
    if( ship.score == STAGE4 ) ballSize = 0.02 ;
    if( ship.score == STAGE5 ) ballSize = 0.01 ;
    
    ship.setBallLocation(setRandomBall, ballSize);
    
    
}

void initialShipPosition(Ship& ship1, Ship& ship2) {
    
    Point2D initalPosition;
    Point2D baseLocation;
    
    float diff = fabs(ship1.spaceShip.vertices[1].y-ship1.spaceShip.vertices[2].y)+0.005;
    
    initalPosition.x = randomRange(-1.3, 1.3);
    initalPosition.y = GROUND + diff ;
    ship1.reset(initalPosition);
    baseLocation.x = 0.0;
    baseLocation.y = GROUND+0.02 ;
    ship1.setLandingLocation(baseLocation, 2*1.75);
    
    float ship1InitalPositionx = initalPosition.x;
    
    bool distance = false ;
    while(!distance) {
      initalPosition.x = randomRange(-1.3, 1.3);
        if( fabs(initalPosition.x - ship1InitalPositionx) > 0.05 )
            distance = true ;
    }
    ship2.reset(initalPosition);
    ship2.setLandingLocation(baseLocation, 2*1.75);
    ship1.storeOtherShip(ship2.spaceShip);
    ship2.storeOtherShip(ship1.spaceShip);
}

void
resetLandingBase(Ship& ship) {
    
    Point2D resetPoint;
    float x1,x2,y1,y2;
   
    bool distance = false;
    while( !distance ) {
        
        resetPoint.x = randomRange(-1.3,1.3);
        resetPoint.y = randomRange(-0.3, 0.75);
        
        x1 = resetPoint.x;
        x2 = ship.otherShipsPosition[0].x;
        y1 = resetPoint.y;
        y2 = ship.otherShipsPosition[0].y;
        
        if( sqrt(pow(x2-x1,2)+pow(y2-y1,2)) > 0.05 ) {
            distance = true;
        }
    }

    if( ship.score == STAGE2) {
        resetPoint.y = GROUND + BASE_THICKNESS*5;
        ship.setLandingLocation(resetPoint, 0.085);
    }
    if( ship.score == STAGE3) ship.setLandingLocation(resetPoint, 0.075);
    if( ship.score == STAGE4) ship.setLandingLocation(resetPoint, 0.065);
    if( ship.score == STAGE5) ship.setLandingLocation(resetPoint, 0.050);
}

void
refuelShip(Ship& ship) {
    if ( ship.isRefueling ) ship.increaseFuelInc += 1;
    if ( ship.increaseFuelInc > 30 ) {
         ship.increaseFuelInc = 0;
         sfx::play(sfx::Refuel, ship.spaceShip.offset.x, ship.fuel / FUEL_CAPACITY, ship.id);
         ship.fuel += REFUEL_RATE;
    }
}

bool updateShip(Ship& ship) {
    ship.inBounds();
    ship.updatePosition();
    ship.rotateShip();
    if( ship.spaceShip.ball.ballhit && !ship.spaceShip.ball.ballhitonce) {
        ship.spaceShip.ball.ballhitonce = true ;
        resetLandingBase(ship);
    }
    ship.drawLandingPad();
    if( ship.landedOnPad && ship.spaceShip.ball.ballhit && ship.spaceShip.ball.ballhitonce) {
        ship.score += 1 ;
        sfx::play(sfx::Score, ship.spaceShip.offset.x, 1.f, ship.id);
        ship.spaceShip.ball.ballhit = false ;
        ship.spaceShip.ball.ballhitonce = false;
        resetBall(ship);
    }
    if(ship.spaceShip.ball.ballhit == 0) {
        ship.drawBall();
    }
    if( ship.onPad) {
        if ( ship.fuel < FUEL_CAPACITY ) {
           ship.isRefueling = true;
           refuelShip(ship);
        }
        else {
            ship.isRefueling = false;
            ship.fuel = FUEL_CAPACITY;
        }
    }
    
    if( ship.hitOtherBall ) {
          ship.velocity.x *= -1 ;
          ship.velocity.y *= -1 ;

        if( ship.spaceShip.location[0].x < ship.spaceShip.ball.ballLocation.x )
            ship.spaceShip.offset.x -= 0.005;
        else
            ship.spaceShip.offset.x += 0.005;
        
        if( ship.spaceShip.location[0].y > ship.spaceShip.ball.ballLocation.y )
            ship.spaceShip.offset.y += 0.005;
        else
            ship.spaceShip.offset.y -= 0.005;
                
      ship.hitOtherBall = false;
    }
    return true;
}

void drawGround(void) {
    glLineWidth(1.5);
    glColor3f(1.0, 1.0, 1.0);
    glBegin(GL_LINES);
    glVertex2d(-1.75 , -1.0 );
    glVertex2d( 1.75 , -1.0 );
    glEnd();
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

    sfx::init();
    sfx::play(sfx::Start);
    bool lowFuel[2] = {false, false};
    bool gameOver = false;
    
    while (!glfwWindowShouldClose(window) )
    {
       
        openGLSetup(window);
        
        ship1.displayShipScore();
        ship2.displayShipScore();
        
        ship1.rotDir = ship2.rotDir = 0;
        ship1.thrusting = ship2.thrusting = false;

        if( ship1.score != WIN_SCORE && ship2.score != WIN_SCORE )
        {
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
        
        if( ship1.score == WIN_SCORE ) {
            for (int j=0;j<100;j++) {
                ship2.explode();
            }
        }
        if( ship2.score == WIN_SCORE ) {
            for (int j=0;j<100;j++) {
                ship1.explode();
            }
        }
        
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
        
        ship1.storeOtherShip(ship2.spaceShip);
        ship2.storeOtherShip(ship1.spaceShip);
        
        if( ship1.ifShipsColide() || ship2.ifShipsColide()) {
            ship2.velocity.x *= -1 ;
            ship2.velocity.y *= -1 ;
            ship2.ang += 0.01;
            ship1.velocity.x *= -1 ;
            ship1.velocity.y *= -1 ;
            ship1.ang += 0.01;
        };
    
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Physics is tuned per-frame at 60 Hz; hold that rate on faster displays.
        static double nextFrame = glfwGetTime();
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




