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

/* TODO fix bounce off base by rethining points in poloygons */

void
Ship::bounceOffBase(Point2D* base) {
    
    for( int i=0;i<BASE;i++) {
        if(shipInPolygon(spaceShip , otherShipPad)) {
            base[i].x = otherShipPad[i].x ;
            base[i].y = otherShipPad[i].y ;
        }
        else {
            base[i].x = spaceShip.landingPad[i].x;
            base[i].y = spaceShip.landingPad[i].y;
        }
    }
    
    //float xMid = (base[3].x - base[0].x)/2.0 ;
    float yMid = (base[2].y - base[3].y)/2.0 ;
    
    bool hitSide = false ;
    for( int i = 0 ; i < SHIP ; i++ )
        if( spaceShip.location[i].x > base[0].x &&
            spaceShip.location[i].x < base[1].x &&
            spaceShip.location[i].y > base[0].y &&
            spaceShip.location[i].y < base[1].y )
        {
            hitSide = true;
            spaceShip.offset.x += 0.0002 ;
            spaceShip.offset.y += 0.0001 ;
            velocity.x *= -1;
            break;
        }
    for( int i = 0 ; i < SHIP ; i++ )
        if( spaceShip.location[i].x > base[2].x &&
            spaceShip.location[i].x < base[3].x &&
            spaceShip.location[i].y > base[3].y &&
            spaceShip.location[i].y < base[2].y )
        {
            hitSide = true;
            spaceShip.offset.x -= 0.0002 ;
            spaceShip.offset.y += 0.0004 ;
            velocity.x *= -1;
            break;
        }
    
    bool belowLineY = false;
    for( int i=0 ; i< SHIP ; i++)
        if( spaceShip.location[i].y < base[1].y - yMid )
            belowLineY = true;
    
    if( belowLineY )
        spaceShip.offset.y -= 0.001;
    else
        spaceShip.offset.y += 0.001;
    velocity.y *= -1 ;
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
    
    float extrabit = 0.027 ;
    for( int i=0;i<SHIP;i++) {
        if( spaceShip.location[i].y <= GROUND ) {
            spaceShip.location[i].y  = GROUND-extrabit;
            velocity.y *= -1.0f;
            if (fabsf(velocity.y) > 0.0004f)
                sfx::play(sfx::Thud, spaceShip.offset.x, fminf(1.f, fabsf(velocity.y) / 0.008f), id);
            return;
        }
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
        } else {
            sfx::play(sfx::Thud, spaceShip.offset.x, fminf(1.f, fabsf(velocity.y) / 0.008f), id);
            velocity.y *= -1.0f;
            //velocity.y -= damp;
        }
    }
    
    if ( shipInPolygon(spaceShip , otherShipPad) ) {
        sfx::play(sfx::Clank, spaceShip.offset.x, 1.f, id);
        bounceOffBase(otherShipPad);
    }
    if ( shipInPolygon(spaceShip , spaceShip.landingPad) ) {
        sfx::play(sfx::Clank, spaceShip.offset.x, 1.f, id);
        bounceOffBase(spaceShip.landingPad);
    }
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
