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

// Reflect relative velocity about unit normal n: v' = v_body + (v - v_body) - (1+e)((v - v_body)·n) n.
static void reflectPellet(Pellet& p, Point2D n, Point2D vBody) {
    float rx = p.velocity.x - vBody.x, ry = p.velocity.y - vBody.y;
    float vn = rx * n.x + ry * n.y;
    if (vn < 0) {
        rx -= (1 + PELLET_BOUNCE) * vn * n.x;
        ry -= (1 + PELLET_BOUNCE) * vn * n.y;
    }
    p.velocity = {vBody.x + rx, vBody.y + ry};
}

// Pellet is inside a convex polygon: push it out through the nearest edge and reflect.
static void bounceOffConvex(Pellet& p, const Point2D* poly, int n, Point2D vBody) {
    float area2 = 0;
    for (int i = 0, j = n - 1; i < n; j = i++)
        area2 += poly[j].x * poly[i].y - poly[i].x * poly[j].y;
    float orient = area2 > 0 ? 1.f : -1.f;

    float best = -1e30f;
    Point2D bestN = {0, 1};
    for (int i = 0, j = n - 1; i < n; j = i++) {
        float ex = poly[i].x - poly[j].x, ey = poly[i].y - poly[j].y;
        float len = sqrtf(ex * ex + ey * ey);
        if (len == 0) continue;
        Point2D nrm = {orient * ey / len, -orient * ex / len};
        float d = (p.pos.x - poly[j].x) * nrm.x + (p.pos.y - poly[j].y) * nrm.y;
        if (d > best) { best = d; bestN = nrm; }
    }
    float push = PELLET_RADIUS - best;
    p.pos.x += bestN.x * push;
    p.pos.y += bestN.y * push;
    reflectPellet(p, bestN, vBody);
}

static bool bounceOffCircle(Pellet& p, Point2D c, float r) {
    float dx = p.pos.x - c.x, dy = p.pos.y - c.y;
    float rr = r + PELLET_RADIUS;
    float d2 = dx * dx + dy * dy;
    if (d2 >= rr * rr || d2 == 0) return false;
    float d = sqrtf(d2);
    Point2D n = {dx / d, dy / d};
    p.pos = {c.x + n.x * rr, c.y + n.y * rr};
    reflectPellet(p, n, {0, 0});
    return true;
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
    explodeFrame = 0 ;
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

// Phase 1: Newtonian linear-momentum collision between two ships.
// Reuses convexMTV for the contact normal, separates the pair by mass, then
// applies an impulse along the normal so momentum is conserved (and kinetic
// energy too when e==1). Returns true on a genuine impact. Linear only; spin
// from off-center hits is Phase 2.
bool
Ship::resolveCollision(Ship& other) {
    // Triangular convex hulls: vertices 0,2,4 (same convention as bounceOffBase).
    Point2D hullA[3] = { spaceShip.location[0], spaceShip.location[2], spaceShip.location[4] };
    Point2D hullB[3] = { other.spaceShip.location[0], other.spaceShip.location[2], other.spaceShip.location[4] };

    Point2D mtv;
    if (!convexMTV(hullA, 3, hullB, 3, mtv))
        return false;                      // not touching

    float len = sqrtf(mtv.x * mtv.x + mtv.y * mtv.y);
    if (len == 0.0f) return false;
    float nx = mtv.x / len, ny = mtv.y / len;   // unit normal, pushes A off B

    // Positional correction: split the separation by mass (equal -> half each).
    float invA = 1.0f / mass, invB = 1.0f / other.mass;
    float invSum = invA + invB;
    float sepA = (invA / invSum), sepB = (invB / invSum);
    spaceShip.offset.x += mtv.x * sepA;  spaceShip.offset.y += mtv.y * sepA;
    other.spaceShip.offset.x -= mtv.x * sepB;  other.spaceShip.offset.y -= mtv.y * sepB;
    for (int i = 0; i < SHIP; ++i) {
        spaceShip.location[i].x += mtv.x * sepA;  spaceShip.location[i].y += mtv.y * sepA;
        other.spaceShip.location[i].x -= mtv.x * sepB;  other.spaceShip.location[i].y -= mtv.y * sepB;
    }

    // Relative velocity along the normal (A relative to B).
    float rvx = velocity.x - other.velocity.x;
    float rvy = velocity.y - other.velocity.y;
    float vn = rvx * nx + rvy * ny;
    if (vn >= 0.0f) return false;          // already separating: no impulse, no stick

    // Impulse magnitude: j = -(1+e) vn / (1/mA + 1/mB). Use the softer (more
    // damaged) ship's restitution so damage reads as a deader collision.
    float e = fminf(restitution, other.restitution);
    float j = -(1.0f + e) * vn / invSum;

    velocity.x += (j * invA) * nx;  velocity.y += (j * invA) * ny;
    other.velocity.x -= (j * invB) * nx;  other.velocity.y -= (j * invB) * ny;
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

namespace {

struct RGB { float r, g, b; };

RGB mix(RGB a, RGB b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// Radial glow: opaque-ish centre fading to nothing at radius r.
void glowDisc(float x, float y, float r, RGB c, float a) {
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(c.r, c.g, c.b, a);
    glVertex2f(x, y);
    glColor4f(c.r, c.g, c.b, 0);
    for (int i = 0; i <= 24; ++i) {
        float th = (i % 24) * 2 * PI / 24;
        glVertex2f(x + r * cosf(th), y + r * sinf(th));
    }
    glEnd();
}

// Soft-edged ring of radius r and half-width w.
void glowRing(float x, float y, float r, float w, RGB c, float a) {
    const int N = 72;
    for (int side = 0; side < 2; ++side) {
        float r0 = side ? r : fmaxf(0.f, r - w), r1 = side ? r + w : r;
        float a0 = side ? a : 0.f, a1 = side ? 0.f : a;
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= N; ++i) {
            float th = (i % N) * 2 * PI / N, cs = cosf(th), sn = sinf(th);
            glColor4f(c.r, c.g, c.b, a0); glVertex2f(x + r0 * cs, y + r0 * sn);
            glColor4f(c.r, c.g, c.b, a1); glVertex2f(x + r1 * cs, y + r1 * sn);
        }
        glEnd();
    }
}

void cloudLayer(float x, float y, float r, float angDeg, RGB c, float a) {
    if (!cloudTexture) { glowDisc(x, y, r * 0.85f, c, a); return; }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cloudTexture);
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(angDeg, 0, 0, 1);
    glScalef(r, r, 1);
    glColor4f(c.r, c.g, c.b, a);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1, -1);
    glTexCoord2f(1, 0); glVertex2f( 1, -1);
    glTexCoord2f(1, 1); glVertex2f( 1,  1);
    glTexCoord2f(0, 1); glVertex2f(-1,  1);
    glEnd();
    glPopMatrix();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

void launchSpark(Spark& s, Point2D c) {
    float th = randomRange(0, 2 * PI), sp = randomRange(0.002, 0.016);
    s.pos = c;
    s.velocity = {cosf(th) * sp, sinf(th) * sp};
    s.maxLife = s.life = 40 + rand() % 70;
    s.ember = false;
}

void launchEmber(Spark& s, Point2D c, float r) {
    float th = randomRange(0, 2 * PI), d = r * 0.55f * sqrtf(randomRange(0, 1));
    s.pos = {c.x + cosf(th) * d, c.y + sinf(th) * d};
    s.velocity = {randomRange(-0.0003, 0.0003), randomRange(0.0005, 0.0013)};
    s.maxLife = s.life = 120 + rand() % 140;
    s.ember = true;
}

} // namespace

void prepareCloudTexture(unsigned char* rgba, int w, int h) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            unsigned char* p = rgba + 4 * (y * w + x);
            for (int k = 0; k < 3; ++k)
                p[k] = (unsigned char)fmaxf(0.f, (p[k] - 16) * 255.f / 239.f);
            float dx = (x + 0.5f) / w - 0.5f, dy = (y + 0.5f) / h - 0.5f;
            float edge = clamp01((0.5f - sqrtf(dx * dx + dy * dy)) / 0.14f);
            p[3] = (unsigned char)(p[3] * edge * edge * (3 - 2 * edge));
        }
    }
}

// "Red giant": a white flash and twin shockwaves, then a seed of fire that
// cools from white to deep red while it blooms into a slowly counter-rotating,
// breathing nebula. Hull edges tumble away and settle as wreckage, sparks
// spray out, and embers keep drifting up from the cloud for good.
void
Ship::explode(void) {

    gravity = 0.0;
    velocity.x = 0.0 ;
    velocity.y = 0.0 ;

    const Point2D c = spaceShip.offset;

    if (explodeFrame == 0) {
        rotateCloud = randomRange(0, 360);
        for (int i = 0; i < SHIP; ++i) {
            Point2D a = spaceShip.vertices[i], b = spaceShip.vertices[(i + 1) % SHIP];
            Point2D m = {(a.x + b.x) / 2, (a.y + b.y) / 2};
            Debris& d = debris[i];
            d.a = {a.x - m.x, a.y - m.y};
            d.b = {b.x - m.x, b.y - m.y};
            d.pos = {c.x + m.x, c.y + m.y};
            float len = hypotf(m.x, m.y) + 1e-4f, sp = randomRange(0.002, 0.006);
            d.velocity = {m.x / len * sp + randomRange(-0.001, 0.001),
                     m.y / len * sp + randomRange(0.0005, 0.002)};
            d.ang = 0;
            d.spin = randomRange(-6, 6);
        }
        for (int i = 0; i < EXPLOSION_SPARKS; ++i) launchSpark(sparks[i], c);
    }

    const float t = explodeFrame;
    if (explodeFrame < 1000000) ++explodeFrame;
    rotateCloud += EXPLOSION_CLOUD_SPIN;

    const float u = clamp01(t / EXPLOSION_GROW_FRAMES);
    const float grow = u * u * (3 - 2 * u);
    const float r = EXPLOSION_CLOUD_SIZE * (0.05f + 0.95f * grow);
    const float heat = expf(-t / 80.f);

    const RGB white   = {1.00f, 1.00f, 0.95f};
    const RGB hot     = {1.00f, 0.90f, 0.65f};
    const RGB orange  = {1.00f, 0.42f, 0.10f};
    const RGB red     = {0.95f, 0.10f, 0.04f};
    const RGB crimson = {0.55f, 0.02f, 0.06f};

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);   // additive: everything glows

    // Corona rays behind the cloud, turning against it and flickering.
    {
        const int RAYS = 20;
        RGB rc = mix(red, hot, heat);
        glBegin(GL_TRIANGLES);
        for (int i = 0; i < RAYS; ++i) {
            float th = (-rotateCloud * 0.6f + i * 360.f / RAYS) * PI / 180;
            float flick = 0.55f + 0.45f * sinf(t * 0.045f * (1 + (i % 4) * 0.35f) + i * 2.39f);
            float len = r * (1.05f + 0.6f * flick), hw = 0.045f;
            glColor4f(rc.r, rc.g, rc.b, 0.22f * grow * flick);
            glVertex2f(c.x, c.y);
            glColor4f(rc.r, rc.g, rc.b, 0);
            glVertex2f(c.x + len * cosf(th - hw), c.y + len * sinf(th - hw));
            glVertex2f(c.x + len * cosf(th + hw), c.y + len * sinf(th + hw));
        }
        glEnd();
    }

    // Three cloud layers: different sizes, spin rates and directions, each breathing.
    cloudLayer(c.x, c.y, r * 1.25f * (1 + 0.035f * sinf(t * 0.021f + 2.0f)),
               rotateCloud * 0.55f + 140, mix(crimson, hot, heat), 0.55f);
    cloudLayer(c.x, c.y, r * (1 + 0.035f * sinf(t * 0.03f)),
               rotateCloud, mix(red, hot, heat), 0.80f);
    cloudLayer(c.x, c.y, r * 0.62f * (1 + 0.05f * sinf(t * 0.043f + 1.0f)),
               -rotateCloud * 1.6f + 60, mix(orange, white, heat), 0.50f);
    glowDisc(c.x, c.y, r * 0.5f, mix(orange, white, heat), 0.30f + 0.12f * sinf(t * 0.05f));

    // Twin shockwaves, cooling from blue-white to orange as they expand.
    for (int k = 0; k < 2; ++k) {
        float age = t - k * 10;
        if (age < 0 || age >= EXPLOSION_RING_FRAMES) continue;
        float p = age / EXPLOSION_RING_FRAMES, q = 1 - p;
        glowRing(c.x, c.y, 0.55f * (1 - q * q * q), 0.03f * q + 0.004f,
                 mix(orange, RGB{0.8f, 0.9f, 1.0f}, q), q * q * (k ? 0.6f : 1.f));
    }

    // Initial white-hot flash.
    if (t < 14) {
        float p = t / 14;
        glowDisc(c.x, c.y, 0.03f + 0.15f * p, white, (1 - p) * (1 - p));
    }

    // Hull edges tumble out, fall, bounce and settle as glowing wreckage.
    for (int i = 0; i < SHIP; ++i) {
        Debris& d = debris[i];
        d.integrate(GRAVITY * 6);          // drag 0.99 baked into the body
        d.ang += d.spin;
        float ca = cosf(d.ang * PI / 180), sa = sinf(d.ang * PI / 180);
        Point2D a = {d.a.x * ca - d.a.y * sa, d.a.x * sa + d.a.y * ca};
        Point2D b = {d.b.x * ca - d.b.y * sa, d.b.x * sa + d.b.y * ca};
        float low = d.pos.y + fminf(a.y, b.y);
        if (low < GROUND) {
            d.pos.y += GROUND - low;
            d.bounceOffStatic({0.0f, 1.0f}, d.restitution); // e=0.3 off the ground
            d.velocity.x *= 0.7f;              // extra ground friction on the tangent
            d.spin *= 0.6f;
        }
        RGB hull = {shipColor[0], shipColor[1], shipColor[2]};
        RGB col = mix(white, hull, clamp01(t / 25));
        float glow = 0.5f + 0.5f * expf(-t / 300.f);
        for (int pass = 0; pass < 2; ++pass) {
            glLineWidth(pass ? 1.5f : 5.f);
            glColor4f(col.r * glow, col.g * glow, col.b * glow, pass ? 1.f : 0.25f);
            glBegin(GL_LINES);
            glVertex2f(d.pos.x + a.x, d.pos.y + a.y);
            glVertex2f(d.pos.x + b.x, d.pos.y + b.y);
            glEnd();
        }
    }

    // Embers keep rising out of the cloud once the blast has settled.
    if (t > 45 && explodeFrame % 2 == 0) {
        for (int i = 0; i < EXPLOSION_SPARKS; ++i) {
            if (sparks[i].life <= 0) { launchEmber(sparks[i], c, r); break; }
        }
    }

    glLineWidth(1.5f);
    for (int i = 0; i < EXPLOSION_SPARKS; ++i) {
        Spark& s = sparks[i];
        if (s.life <= 0) continue;
        float f = (float)s.life / s.maxLife;
        --s.life;
        if (s.ember) {
            s.velocity.x += sinf(t * 0.05f + s.pos.y * 40) * 0.00002f;
            s.pos.x += s.velocity.x; s.pos.y += s.velocity.y;
            RGB ec = mix(crimson, orange, 0.5f + 0.5f * sinf(t * 0.3f + i));
            float a = sinf(PI * f) * 0.9f;
            glowDisc(s.pos.x, s.pos.y, 0.008f, ec, a);
            glowDisc(s.pos.x, s.pos.y, 0.0025f, hot, a);
            continue;
        }
        s.integrate(0.00004f);             // drag 0.965 baked into the body
        if (s.pos.y < GROUND) {
            s.pos.y = GROUND;
            s.bounceOffStatic({0.0f, 1.0f}, s.restitution); // e=0.4 off the ground
            s.velocity.x *= 0.7f;              // extra ground friction on the tangent
        }
        RGB sc = f > 0.5f ? mix(orange, hot, (f - 0.5f) * 2) : mix(red, orange, f * 2);
        glBegin(GL_LINES);
        glColor4f(sc.r, sc.g, sc.b, fminf(1.f, f * 1.5f));
        glVertex2f(s.pos.x, s.pos.y);
        glColor4f(sc.r, sc.g, sc.b, 0);
        glVertex2f(s.pos.x - s.velocity.x * 4, s.pos.y - s.velocity.y * 4);
        glEnd();
    }

    glLineWidth(1.f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glColor4f(1, 1, 1, 1);
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
    p->velocity = {velocity.x + dir.x * MUZZLE_SPEED, velocity.y + dir.y * MUZZLE_SPEED};

    velocity.x -= dir.x * RECOIL;
    velocity.y -= dir.y * RECOIL;
    landed = false;
    sfx::play(sfx::Fire, spaceShip.offset.x, 1.f, id);
}

void
Ship::bump(const Pellet& p) {
    // TODO (unify): fold this shove AND the pellet's ricochet into ONE
    // PhysicsBody::resolveImpulse(pellet, normal) call, so a single collision
    // law handles both bodies. Blocked now only because the pellet arrives
    // const here and bounceOffConvex does the ricochet a line later; take the
    // pellet by non-const ref, compute the contact normal once, and let
    // resolveImpulse write both velocities. Then bounceOffConvex keeps only the
    // positional push-out, not the reflect. Same physics, one law.
    // Mass-driven momentum transfer: the shove on this ship is the pellet's
    // momentum relative to us, scaled by the mass ratio. Heavier/faster pellet
    // -> bigger bodoink, automatically. (Direction is the approach vector; the
    // pellet's own ricochet off the hull is handled by bounceOffConvex.)
    Point2D dv = {p.velocity.x - velocity.x, p.velocity.y - velocity.y};
    Point2D r  = {p.pos.x - spaceShip.offset.x, p.pos.y - spaceShip.offset.y};
    float massRatio = p.mass / (p.mass + mass);   // 0..1; pellet's share of the pair
    float transfer = (1.0f + fminf(p.restitution, restitution)) * massRatio;
    velocity.x += dv.x * transfer;
    velocity.y += dv.y * transfer;
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
                    p.active = false;
                    sfx::play(sfx::Reload, p.pos.x, (float)s->ammo / AMMO_CAPACITY, s->id);
                    break;
                }
            }
            continue;
        }

        p.integrate(PELLET_GRAVITY);       // drag 1.0: pellets don't self-damp
        if (p.pos.x < -1.75f) p.pos.x += 3.5f;
        if (p.pos.x >  1.75f) p.pos.x -= 3.5f;

        if (p.pos.y <= GROUND + PELLET_RADIUS) {
            p.pos.y = GROUND + PELLET_RADIUS;
            p.bounceOffStatic({0.0f, 1.0f}, p.restitution); // e=0.6: ~3 bounces from top, ~2 from mid
            p.velocity.x *= 0.85f;                           // a little ground friction on the roll
            // Settle to resting only once the hop is too small to see.
            if (p.velocity.y < PELLET_REST_SPEED) {
                p.velocity = {0, 0};
                p.resting = true;
                p.life = 0;
            }
            continue;
        }

        Ship* ships[2] = {this, &other};
        bool hit = false;
        for (Ship* s : ships) {
            if (s == this && p.life < PELLET_ARM_FRAMES) continue;
            const Point2D* h = s->spaceShip.location;
            Point2D hull[3] = {h[0], h[2], h[4]};
            if (pointInPolygon(3, hull, p.pos.x, p.pos.y)) {
                s->bump(p);
                bounceOffConvex(p, hull, 3, s->velocity);
                hit = true;
                break;
            }
        }
        if (hit) continue;

        for (Ship* s : ships) {
            if (pointInPolygon(BASE, s->spaceShip.landingPad, p.pos.x, p.pos.y)) {
                bounceOffConvex(p, s->spaceShip.landingPad, BASE, {0, 0});
                sfx::play(sfx::Thud, p.pos.x, 0.15f);
                break;
            }
            const ShipBall& b = s->spaceShip.ball;
            if (!b.ballhit && bounceOffCircle(p, b.ballLocation, b.ballSize)) {
                sfx::play(sfx::Boing, p.pos.x, 0.5f);
                break;
            }
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
    for( int i= 0; i < WIN_SCORE - score ; ++i) {
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

    string rounds = " Ammo:  " + string(ammo, 'o') + string(AMMO_CAPACITY - ammo, '.');
    drawText(rounds, statTextPosX, statTextPosY-20);
}
