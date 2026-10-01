//
//  ship.cpp
//  opengl-series
//
//  Created by Terrence McGuinness on 3/14/15.
//
// working in progress

#include "ship.h"
#include <iostream>
#include <sstream>

#include "stb_easy_font.h"

#define PI 3.14159

#include <time.h>

using namespace std;

#define INF 10000

GLuint cloudTexture = 0;

// (x, y) is a baseline position in the original 800x600 y-up virtual space.
void drawText( string text, int x, int y)
{
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    const float W = vp[2], H = vp[3];
    const float scale = fmaxf(1.0f, H / 960.0f);   // ~9x15 px glyphs at 1440p
    static char buf[64 * 1024];

    int quads = stb_easy_font_print(0, 0, (char*)text.c_str(), NULL, buf, sizeof(buf));

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, W, H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(x / 800.0f * W, H - y / 600.0f * H - 7.0f * scale, 0);
    glScalef(scale, scale, 1);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, buf);
    glDrawArrays(GL_QUADS, 0, quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}


bool onSegment(Point2D p, Point2D q, Point2D r)
{
    if (q.x <= max(p.x, r.x) && q.x >= min(p.x, r.x) &&
        q.y <= max(p.y, r.y) && q.y >= min(p.y, r.y))
        return true;
    return false;
}

int orientation(Point2D p, Point2D q, Point2D r)
{
    int val = (q.y - p.y) * (r.x - q.x) -
    (q.x - p.x) * (r.y - q.y);
    
    if (val == 0) return 0;  // colinear
    return (val > 0)? 1: 2; // clock or counterclock wise
}

bool doIntersect(Point2D p1, Point2D q1, Point2D p2, Point2D q2)
{

    int o1 = orientation(p1, q1, p2);
    int o2 = orientation(p1, q1, q2);
    int o3 = orientation(p2, q2, p1);
    int o4 = orientation(p2, q2, q1);
    
    if (o1 != o2 && o3 != o4)
        return true;

    if (o1 == 0 && onSegment(p1, p2, q1)) return true;
    if (o2 == 0 && onSegment(p1, q2, q1)) return true;
    if (o3 == 0 && onSegment(p2, p1, q2)) return true;

    if (o4 == 0 && onSegment(p2, q1, q2)) return true;
    
    return false;
}

bool isInside(Point2D polygon[], int n, Point2D p)
{

    if (n < 3)  return false;
    Point2D extreme = {INF, p.y};
    int count = 0, i = 0;
    do
    {
        int next = (i+1)%n;
        if (doIntersect(polygon[i], polygon[next], p, extreme))
        {
            if (orientation(polygon[i], p, polygon[next]) == 0)
                return onSegment(polygon[i], p, polygon[next]);
            
            count++;
        }
        i = next;
    } while (i != 0);
    return count&1;
}



float
randomRange(float min, float max) {
    return min + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(max-min)));
}

bool pointInPolygon(int nvert, Point2D* points, float testx, float testy)
{
    int i, j ;
    bool c = false;
    for (i = 0, j = nvert-1; i < nvert; j = i++) {
        if ( ((points[i].y >testy) != (points[j].y>testy)) &&
            (testx < (points[j].x-points[i].x) * (testy-points[i].y) / (points[j].y-points[i].y) + points[i].x) )
            c = !c;
    }
    return c;
}


bool
Ship::shipInPolygon(SpaceShip spaceShip, Point2D* polygon ) {
    for( int i=0;i<SHIP;++i)
        if( pointInPolygon(4, polygon, spaceShip.location[i].x, spaceShip.location[i].y)) {
            pointWasInPolygon = true;
            return true ;
        }
    return false;
}

Ship::Ship(ShipStatusConsts shipStatusConsts) {
    statTextPosX = shipStatusConsts.statTextPosX ;
    statTextPosY = shipStatusConsts.statTextPosY ;
    scoreLocation = shipStatusConsts.scoreLocation;
    Point2D resetPoint;
    resetPoint.x = 0.0 ; resetPoint.y = 0.0;
    reset(resetPoint);
}

Ship::~Ship() {};

void
Ship::setShipColor(GLfloat* color) {
    shipColor[0] = color[0] ;
    shipColor[1] = color[1] ;
    shipColor[2] = color[2] ;
}

void
Ship::reset(Point2D resetPoint) {
    ang = 0;
    spaceShip.vertices[0].x =  .00 ; spaceShip.vertices[0].y =  .02 ;
    spaceShip.vertices[1].x =  .01 ; spaceShip.vertices[1].y =  .00 ;
    spaceShip.vertices[2].x =  .02 ; spaceShip.vertices[2].y = -.02 ;
    spaceShip.vertices[3].x =  .00 ; spaceShip.vertices[3].y = -.01 ;
    spaceShip.vertices[4].x = -.02 ; spaceShip.vertices[4].y = -.02 ;
    spaceShip.vertices[5].x = -.01 ; spaceShip.vertices[5].y =  .00 ;
    

//    spaceShip.vertices[0].x =  .00 ; spaceShip.vertices[0].y =  .02 ;
//    spaceShip.vertices[1].x =  .02 ; spaceShip.vertices[1].y = -.02 ;
//    spaceShip.vertices[2].x =  .00 ; spaceShip.vertices[2].y = -.01 ;
//    spaceShip.vertices[3].x = -.02 ; spaceShip.vertices[3].y = -.02 ;
 
    
    tail[0].x = 0.01; tail[0].y = -0.02;
    tail[1].x = 0.00; tail[1].y = -0.01;
    tail[2].x =-0.01; tail[2].y = -0.02;
    
    rightThrust[0].x = 0.0f   ; rightThrust[0].y = 0.02f;
    rightThrust[1].x = -0.02f ; rightThrust[1].y = 0.02f;
    
    leftThrust[0].x = 0.0f  ; leftThrust[0].y = 0.02f;
    leftThrust[1].x = 0.02f ; leftThrust[1].y = 0.02f;

    spaceShip.offset = resetPoint ;
    for (int i = 0; i < SHIP; ++i) {
        spaceShip.location[i].x = spaceShip.vertices[i].x + resetPoint.x;
        spaceShip.location[i].y = spaceShip.vertices[i].y + resetPoint.y;
    }
    velocity.x = 0.0 ; velocity.y = 0.0 ;
    
    landed = false ;
    spaceShip.ball.ballhit = false ;
    spaceShip.ball.ballhitonce = false ;
    
    isRefueling = false;
    increaseFuelInc = 0 ;
    landedOnPad = false ;
    onPad = false ;
    hitOtherBall = false ;
    pointWasInPolygon = false ;
    rotateCloud = 0.0 ;
    
    explodeBigger.x = 0.0 ;
    explodeBigger.y = 0.0 ;
    score = 0;

    for (int i = 0; i < PELLET_MAX; ++i) pellets[i].active = false;
    ammo = AMMO_CAPACITY;
    fireCooldown = 0;
    ammoRecharge = 0;
    
}

void Ship::rotateShip(void) {
    for(int i=0 ; i < SHIP ; i++) {
        spaceShip.vertices[i] = rotatePoints(spaceShip.vertices[i], ang);
    }
    for(int i=0 ; i< tailPoints ; i++) {
        tail[i] = rotatePoints(tail[i],ang);
    }
    for(int i=0 ; i<thrustPoints ; i++) {
        leftThrust[i] = rotatePoints(leftThrust[i], ang);
        rightThrust[i] = rotatePoints(rightThrust[i], ang);
    }
}

void
Ship::rotateCounterClockWise(void) {
    
    if (fuel < 0.0) {
        sfx::play(sfx::Sputter, spaceShip.offset.x, 0.f, id);
        return ;
    }
    
    rotDir = -1;
    fuel -= FUEL_ROTATE ;
    ang += angFactor;
    
    glLineWidth(.5);
    glColor3f(1.0, 0.0, 0.0);
    glPushMatrix();
    glTranslatef(spaceShip.offset.x,spaceShip.offset.y,0);
    glBegin(GL_LINES);
      glVertex2d( leftThrust[0].x, leftThrust[0].y );
      glVertex2d( leftThrust[1].x, leftThrust[1].y );
    glEnd();
    glPopMatrix();
}

void
Ship::rotateClockWise(void) {
    
    if (fuel < 0.0) {
        sfx::play(sfx::Sputter, spaceShip.offset.x, 0.f, id);
        return ;
    }
    
    rotDir = 1;
    fuel -= FUEL_ROTATE ;
    ang -= angFactor;
    
    glLineWidth(.5);
    glColor3f(1.0, 0.0, 0.0);
    glPushMatrix();
    glTranslatef(spaceShip.offset.x,spaceShip.offset.y,0);
    glBegin(GL_LINES);
      glVertex2d( rightThrust[0].x, rightThrust[0].y );
      glVertex2d( rightThrust[1].x, rightThrust[1].y );
    glEnd();
    glPopMatrix();
}

void
Ship::setBallLocation(Point2D setBallLocation, float setBallSize) {
 
    spaceShip.ball.ballLocation = setBallLocation;
    spaceShip.ball.ballSize = setBallSize ;
}

void
Ship::drawCircle(float r) {
    
    int num_segments = 16 ;
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBegin(GL_POLYGON);
    for(int ii = 0; ii < num_segments; ii++)
    {
        float theta = 2.0f * pi * float(ii) / float(num_segments);
        float x = r * cosf(theta);
        float y = r * sinf(theta);
        glVertex2f(x, y);
    }
    glEnd();
}

void
Ship::drawBall() {
    
    glColor3f(shipColor[0],shipColor[1],shipColor[2]);
    glPushMatrix();
    glTranslatef(spaceShip.ball.ballLocation.x,spaceShip.ball.ballLocation.y,0);
    drawCircle(spaceShip.ball.ballSize);
    glPopMatrix();
}

void
Ship::drawShip()
{
    glLineWidth(.5f);
//   if( hitSide)
//        glColor3f(0.0,1.0,0.0);
//   else
    glColor3f(shipColor[0],shipColor[1],shipColor[2]);
    
    glPushMatrix();
    glTranslatef(spaceShip.offset.x,spaceShip.offset.y,0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_POLYGON);
    for(int i=0 ; i < SHIP ; ++i) {
        glVertex2f(spaceShip.vertices[i].x, spaceShip.vertices[i].y);
    }
    glEnd();
    glPopMatrix();
}

bool
Ship::ifShipsColide(void) {
    for( int i=0 ; i < SHIP ; i++) {
      if( pointInPolygon(4, otherShipsPosition, spaceShip.location[i].x, spaceShip.location[i].y)) {
         sfx::play(sfx::Clang, spaceShip.offset.x);
         return true;
       }
    }
    return false;
}

// SAT on two convex polygons; mtv pushes a out of b.
static bool convexMTV(const Point2D* a, int na, const Point2D* b, int nb, Point2D& mtv)
{
    float best = INFINITY;
    Point2D bestN = {0, 0};
    for (int pass = 0; pass < 2; ++pass) {
        const Point2D* p = pass ? b : a;
        int n = pass ? nb : na;
        for (int i = 0; i < n; ++i) {
            const Point2D& p0 = p[i];
            const Point2D& p1 = p[(i + 1) % n];
            float nx = p1.y - p0.y, ny = p0.x - p1.x;
            float len = sqrtf(nx * nx + ny * ny);
            if (len == 0.0f) continue;
            nx /= len; ny /= len;
            float amin = INFINITY, amax = -INFINITY, bmin = INFINITY, bmax = -INFINITY;
            for (int k = 0; k < na; ++k) {
                float d = a[k].x * nx + a[k].y * ny;
                amin = fminf(amin, d); amax = fmaxf(amax, d);
            }
            for (int k = 0; k < nb; ++k) {
                float d = b[k].x * nx + b[k].y * ny;
                bmin = fminf(bmin, d); bmax = fmaxf(bmax, d);
            }
            float overlap = fminf(amax, bmax) - fmaxf(amin, bmin);
            if (overlap <= 0.0f) return false;
            if (overlap < best) { best = overlap; bestN = {nx, ny}; }
        }
    }
    if (!std::isfinite(best)) return false;
    Point2D ca = {0, 0}, cb = {0, 0};
    for (int k = 0; k < na; ++k) { ca.x += a[k].x / na; ca.y += a[k].y / na; }
    for (int k = 0; k < nb; ++k) { cb.x += b[k].x / nb; cb.y += b[k].y / nb; }
    if ((ca.x - cb.x) * bestN.x + (ca.y - cb.y) * bestN.y < 0.0f) {
        bestN.x = -bestN.x; bestN.y = -bestN.y;
    }
    mtv = {bestN.x * best, bestN.y * best};
    return true;
}

// Returns true on an actual impact (ship moving into the pad).
bool
Ship::bounceOffBase(const Point2D* base) {
    // Vertices 1, 3, 5 lie on or inside the triangle 0-2-4, so it is the convex hull.
    Point2D hull[3] = { spaceShip.location[0], spaceShip.location[2], spaceShip.location[4] };
    Point2D mtv;
    if (!convexMTV(hull, 3, base, BASE, mtv))
        return false;

    spaceShip.offset.x += mtv.x;
    spaceShip.offset.y += mtv.y;
    for (int i = 0; i < SHIP; ++i) {
        spaceShip.location[i].x += mtv.x;
        spaceShip.location[i].y += mtv.y;
    }

    float len = sqrtf(mtv.x * mtv.x + mtv.y * mtv.y);
    if (len == 0.0f) return false;
    float nx = mtv.x / len, ny = mtv.y / len;
    float vn = velocity.x * nx + velocity.y * ny;
    if (vn >= 0.0f) return false;
    velocity.x -= 2.0f * vn * nx;
    velocity.y -= 2.0f * vn * ny;
    return true;
}

void
Ship::inBounds(void) {
    
    if( spaceShip.offset.x < -1.75f ) spaceShip.offset.x  =  1.74f ;
    if( spaceShip.offset.x >  1.75f ) spaceShip.offset.x *= -1.0f  ;
    
    if( sqrt( pow( spaceShip.ball.ballLocation.x - spaceShip.offset.x, 2 ) +
             pow( spaceShip.ball.ballLocation.y - spaceShip.offset.y, 2 ) ) < spaceShip.ball.ballSize+0.02 &&
       fabs(velocity.x) < 0.005 && fabs(velocity.y) < 0.005 ) {
        if (!spaceShip.ball.ballhit)
            sfx::play(sfx::Collect, spaceShip.offset.x, 1.f, id);
        spaceShip.ball.ballhit = true ;
        landedOnPad = false ;
    }
    
    if( (landedOnPad == true) &&
       sqrt( pow( otherShipBall.ballLocation.x - spaceShip.offset.x, 2 ) +
             pow( otherShipBall.ballLocation.y - spaceShip.offset.y, 2 ) ) < otherShipBall.ballSize+0.02 ) {
        hitOtherBall = true ;
        sfx::play(sfx::Boing, spaceShip.offset.x, 1.f, id);
    }
    
    float minY = spaceShip.location[0].y;
    for( int i=1;i<SHIP;i++)
        minY = fminf(minY, spaceShip.location[i].y);
    if( minY <= GROUND ) {
        // Resolve penetration on offset; location[] is rebuilt from it in updatePosition().
        float push = GROUND - minY;
        spaceShip.offset.y += push;
        for( int i=0;i<SHIP;i++)
            spaceShip.location[i].y += push;
        // Reflect only when moving into the ground, else a spinning ship flip-flops and sinks.
        if( velocity.y < 0.0f ) {
            velocity.y = -velocity.y;
            if (velocity.y > 0.0004f)
                sfx::play(sfx::Thud, spaceShip.offset.x, fminf(1.f, velocity.y / 0.008f), id);
        }
        return;
    }
    
    bool wasOnPad = onPad;
    onPad = false ;
    float damp = 0.001f;
    if( fabs(spaceShip.location[4].y - spaceShip.landingPad[1].y) < damp &&
        fabs(spaceShip.location[2].y - spaceShip.landingPad[1].y) < damp &&
       
        (spaceShip.location[4].x > spaceShip.landingPad[1].x) &&
        (spaceShip.location[2].x < spaceShip.landingPad[2].x)   )
    {
        if( fabs(velocity.x) < damp && fabs(velocity.y) < damp ) {
            int x1 = 1000 * spaceShip.vertices[2].x ;
            int x3 = 1000 * spaceShip.vertices[4].x ;
            if( ( 18 < x1 && x1 < 21) && (-21 < x3 && x3 < -18) ) {
                landed = true ;
                landedOnPad = true ;
                onPad = true ;
                velocity.x = velocity.y = 0.0 ;
                ang = 0.0 ;
                if (!wasOnPad)
                    sfx::play(sfx::Chime, spaceShip.offset.x, 1.f, id);
                return;
          }
        } else if( velocity.y < 0.0f ) {
            sfx::play(sfx::Thud, spaceShip.offset.x, fminf(1.f, fabsf(velocity.y) / 0.008f), id);
            velocity.y *= -1.0f;
        }
    }
    
    if ( bounceOffBase(otherShipPad) )
        sfx::play(sfx::Clank, spaceShip.offset.x, 1.f, id);
    if ( bounceOffBase(spaceShip.landingPad) )
        sfx::play(sfx::Clank, spaceShip.offset.x, 1.f, id);
}

void
Ship::explode(void) {
    
    gravity = 0.0;
    velocity.x = 0.0 ;
    velocity.y = 0.0 ;
    
            glColor3f(1.0, 0.0, 0.0);
            glBindTexture(GL_TEXTURE_2D, cloudTexture);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_TEXTURE_2D);
            glPushMatrix();
            glTranslated(spaceShip.offset.x-explodeBigger.x*2.0, spaceShip.offset.y-explodeBigger.y*2.0,0.0);
            glScalef(explodeBigger.x*4,explodeBigger.y*4,explodeBigger.x*4);
    
    rotateCloud += 0.5 ;
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(0.5,0.5,0.0);
    glRotatef(rotateCloud,0.0,0.0,1.0);
    glTranslatef(-0.5,-0.5,0.0);
    glMatrixMode(GL_MODELVIEW);
    
            glBegin( GL_QUADS );
            glTexCoord2d(0.0,0.0); glVertex2d(0.0,0.0);
            glTexCoord2d(1.0,0.0); glVertex2d(1.0,0.0);
            glTexCoord2d(1.0,1.0); glVertex2d(1.0,1.0);
            glTexCoord2d(0.0,1.0); glVertex2d(0.0,1.0);
            glEnd();
            glPopMatrix();
            glFlush();
            glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_TEXTURE_2D);
    
    float randomAng ;
    glLineWidth(.5);
    

    glColor3f(shipColor[0],shipColor[1],shipColor[2]);
   
    for(int i=0;i<SHIP;i++) {
        
        for(int j=0;j<4;j++) {
            randomAng = randomRange(-0.1, 0.1);
            spaceShip.vertices[i] = rotatePoints(spaceShip.vertices[i], randomAng);
        }
       
        glColor3f(shipColor[0], shipColor[1], shipColor[2]);
        glPushMatrix();
        if (explodeBigger.x < 0.03)
            explodeBigger.x += 0.00005 ;
        explodeBigger.y = explodeBigger.x;
        int pick=rand()%4+1;
        if (pick==1) glTranslatef(spaceShip.offset.x+explodeBigger.x,spaceShip.offset.y+explodeBigger.y,0);
        if (pick==2) glTranslatef(spaceShip.offset.x-explodeBigger.x,spaceShip.offset.y-explodeBigger.y,0);
        if (pick==3) glTranslatef(spaceShip.offset.x+explodeBigger.x,spaceShip.offset.y-explodeBigger.y,0);
        if (pick==4) glTranslatef(spaceShip.offset.x-explodeBigger.x,spaceShip.offset.y+explodeBigger.y,0);
        //glBegin(GL_LINE_STRIP);
        // glVertex2d(spaceShip.vertices[i].x, spaceShip.vertices[i].y);
        // glVertex2d(spaceShip.vertices[i+1].x, spaceShip.vertices[i+1].y);
        //glEnd();
        glPopMatrix();
        
        //glPushMatrix();
        //glTranslatef(spaceShip.offset.x-explodeBigger.x,spaceShip.offset.y+explodeBigger.y,0);
        //glBegin(GL_LINE_STRIP);
        // glVertex2d(spaceShip.vertices[i].x, spaceShip.vertices[i].y);
        // glVertex2d(spaceShip.vertices[i+1].x, spaceShip.vertices[i+1].y);
        //glEnd();
        //glPopMatrix();
    }
}

void
Ship::thrust() {
    
    if( landed == true ) {
        landed = false ;
        velocity.y += 0.001;
        //spaceShip.offset.y  += 0.0005 ; // Extra Boost to get off ground
        updatePosition();
    }
    
    if (fuel < 0.0) {
        sfx::play(sfx::Sputter, spaceShip.offset.x, 1.f, id);
        return ;
    }
    
    fuel -= FUEL_THRUST ;
    thrusting = true;
    velocity.x -= spaceShip.vertices[3].x*velocityFactor;
    velocity.y -= spaceShip.vertices[3].y*velocityFactor;
    
    glLineWidth(.5);
    glColor3f(1.0, 0.0, 0.0);
    glPushMatrix();
    glTranslatef(spaceShip.offset.x,spaceShip.offset.y,0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBegin(GL_POLYGON);
    for(int i=0 ; i< tailPoints ; i++) {
        glVertex2d(tail[i].x, tail[i].y);
    }
    glEnd();
    glPopMatrix();
}


Point2D Ship::rotatePoints(Point2D p, float ang)
{
   if( ang > 2.0*pi || ang < -2.0*pi ) ang = 0.0 ;
    Point2D ptemp;
    ptemp.x = p.x * cos(ang) - p.y * sin(ang);
    ptemp.y = p.x * sin(ang) + p.y * cos(ang);
    return ptemp;
}

void Ship::updatePosition(void) {
    spaceShip.offset.x += velocity.x ;
    spaceShip.offset.y += velocity.y ;
   
    for(int i = 0 ; i < SHIP; ++i) {
        spaceShip.location[i].x = spaceShip.vertices[i].x + spaceShip.offset.x ;
        spaceShip.location[i].y = spaceShip.vertices[i].y + spaceShip.offset.y ;
    }
    
    if( !landed ) {
       velocity.y -= gravity ;
    }
}

void
Ship::fire(void) {
    if (fireCooldown > 0) return;
    fireCooldown = FIRE_COOLDOWN;
    if (ammo <= 0) {
        sfx::play(sfx::Dry, spaceShip.offset.x, 1.f, id);
        return;
    }

    Pellet* p = nullptr;
    for (int i = 0; i < PELLET_MAX && !p; ++i)
        if (!pellets[i].active) p = &pellets[i];
    if (!p) {   // recycle the oldest spent round
        p = &pellets[0];
        for (int i = 1; i < PELLET_MAX; ++i)
            if (pellets[i].resting && (!p->resting || pellets[i].life > p->life)) p = &pellets[i];
    }

    Point2D tip = spaceShip.vertices[0];
    float len = sqrtf(tip.x * tip.x + tip.y * tip.y);
    Point2D dir = {tip.x / len, tip.y / len};

    --ammo;
    p->active  = true;
    p->resting = false;
    p->life    = 0;
    p->pos = {spaceShip.offset.x + tip.x, spaceShip.offset.y + tip.y};
    p->vel = {velocity.x + dir.x * MUZZLE_SPEED, velocity.y + dir.y * MUZZLE_SPEED};

    velocity.x -= dir.x * RECOIL;
    velocity.y -= dir.y * RECOIL;
    landed = false;
    sfx::play(sfx::Fire, spaceShip.offset.x, 1.f, id);
}

void
Ship::bump(const Pellet& p) {
    Point2D dv = {p.vel.x - velocity.x, p.vel.y - velocity.y};
    Point2D r  = {p.pos.x - spaceShip.offset.x, p.pos.y - spaceShip.offset.y};
    velocity.x += dv.x * PELLET_KICK;
    velocity.y += dv.y * PELLET_KICK;
    ang += (r.x * dv.y - r.y * dv.x) * PELLET_SPIN_KICK;
    landed = false;
    onPad = false;
    sfx::play(sfx::Ping, spaceShip.offset.x, 1.f, id);
}

void
Ship::updatePellets(Ship& other) {
    if (fireCooldown > 0) --fireCooldown;

    if (onPad && ammo < AMMO_CAPACITY) {
        if (++ammoRecharge >= AMMO_RECHARGE) {
            ammoRecharge = 0;
            ++ammo;
            sfx::play(sfx::Reload, spaceShip.offset.x, (float)ammo / AMMO_CAPACITY, id);
        }
    } else {
        ammoRecharge = 0;
    }

    Ship* scavengers[2] = {this, &other};
    for (int i = 0; i < PELLET_MAX; ++i) {
        Pellet& p = pellets[i];
        if (!p.active) continue;
        ++p.life;

        if (p.resting) {
            if (p.life > PELLET_REST_FRAMES) { p.active = false; continue; }
            // Anyone can scoop up a spent round off the ground, including the enemy's.
            for (Ship* s : scavengers) {
                if (s->ammo >= AMMO_CAPACITY) continue;
                float dx = s->spaceShip.offset.x - p.pos.x, dy = s->spaceShip.offset.y - p.pos.y;
                if (dx * dx + dy * dy < PICKUP_RADIUS * PICKUP_RADIUS) {
                    ++s->ammo;

    string rounds = " Ammo:  " + string(ammo, 'o') + string(AMMO_CAPACITY - ammo, '.');
    drawText(rounds, statTextPosX, statTextPosY-20);
                    p.active = false;
                    sfx::play(sfx::Reload, p.pos.x, (float)s->ammo / AMMO_CAPACITY, s->id);
                    break;
                }
            }
            continue;
        }

        p.vel.y -= PELLET_GRAVITY;
        p.pos.x += p.vel.x;
        p.pos.y += p.vel.y;
        if (p.pos.x < -1.75f) p.pos.x += 3.5f;
        if (p.pos.x >  1.75f) p.pos.x -= 3.5f;

        if (p.pos.y <= GROUND + PELLET_RADIUS) {
            p.pos.y = GROUND + PELLET_RADIUS;
            p.vel = {0, 0};
            p.resting = true;
            p.life = 0;
            continue;
        }

        if (pointInPolygon(BASE, spaceShip.landingPad, p.pos.x, p.pos.y) ||
            pointInPolygon(BASE, other.spaceShip.landingPad, p.pos.x, p.pos.y)) {
            p.active = false;
            continue;
        }

        const Point2D* h = other.spaceShip.location;
        Point2D hull[3] = {h[0], h[2], h[4]};
        if (pointInPolygon(3, hull, p.pos.x, p.pos.y)) {
            other.bump(p);
            p.active = false;
        }
    }
}

void
Ship::drawPellets(void) {
    for (int i = 0; i < PELLET_MAX; ++i) {
        const Pellet& p = pellets[i];
        if (!p.active) continue;
        float k = p.resting ? 0.45f : 1.0f;
        glColor3f(shipColor[0] * k, shipColor[1] * k, shipColor[2] * k);
        glPushMatrix();
        glTranslatef(p.pos.x, p.pos.y, 0);
        drawCircle(PELLET_RADIUS);
        glPopMatrix();
    }
}

void
Ship::setLandingLocation(Point2D location, float size) {
    landingLocation = location;
    landingPadSize  = size ;
    spaceShip.landingPad[0].x = landingLocation.x - landingPadSize - 0.01 ;
    spaceShip.landingPad[0].y = landingLocation.y - 0.05 ;
    spaceShip.landingPad[1].x = landingLocation.x - landingPadSize ;
    spaceShip.landingPad[1].y = landingLocation.y - 0.02 ;
    spaceShip.landingPad[2].x = landingLocation.x + landingPadSize ;
    spaceShip.landingPad[2].y = landingLocation.y - 0.02 ;
    spaceShip.landingPad[3].x = landingLocation.x + landingPadSize + 0.01 ;
    spaceShip.landingPad[3].y = landingLocation.y - 0.05 ;
}

void
Ship::drawLandingPad(void) {
    
    glColor3f(shipColor[0], shipColor[1], shipColor[2]);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBegin(GL_POLYGON);
    for( int i = 0 ; i < 4 ; ++i ) {
        glVertex2d(spaceShip.landingPad[i].x, spaceShip.landingPad[i].y) ;
    }
    glEnd();
    
}

void
Ship::storeOtherShip( SpaceShip spaceShip ) {
    
    otherShipBall = spaceShip.ball;
    for(int i = 0 ; i < BASE ; ++i) {
      otherShipPad[i].x  = spaceShip.landingPad[i].x;
      otherShipPad[i].y  = spaceShip.landingPad[i].y;
    }
    
    for( int i = 0 ; i < SHIP ; ++i) {
        otherShipsPosition[i].x = spaceShip.location[i].x;
        otherShipsPosition[i].y = spaceShip.location[i].y;    }
}

void
Ship::displayShipScore(void){
    glColor3f(shipColor[0], shipColor[1], shipColor[2]);
    for( int i= 0; i < score ; ++i) {
        glPushMatrix();
        glTranslatef(scoreLocation+(float)i*0.05,0.976,0.0);
        drawCircle(0.01f);
        glPopMatrix();
    }
}

string Convert (float number){
    ostringstream buff;
    buff.precision(3);
    buff<<fixed<<number;
    return buff.str();
}

void
Ship::displayShipStatus(void) {
    glColor3f(shipColor[0], shipColor[1], shipColor[2]);
    int fuel_int = (int)fuel;
    string fuel = " Fuel:  " + std::to_string(fuel_int);
    drawText(fuel, statTextPosX, statTextPosY);

    float factor = 10000;
    string velocityX, velocityY;
    velocityX = "X Vel: "+Convert(fabs(velocity.x)*factor);
    velocityY = "Y Vel: "+Convert(velocity.y*factor);
    //velocityX = "X pos: "+Convert(spaceShip.location[0].x);
    //velocityY = "Y pos: "+Convert(spaceShip.location[0].y);
    
    drawText(velocityX, statTextPosX, statTextPosY-10);
    drawText(velocityY, statTextPosX, statTextPosY-15);
}
