#ifndef GEOMETRY_H
#define GEOMETRY_H

// 2D vector type shared by the physics base and the game objects.
// Extracted from ship.h so PhysicsBody (the base layer) need not depend on Ship.

#include <GLFW/glfw3.h>   // GLfloat

struct Point2D {
    GLfloat x;
    GLfloat y;

    Point2D operator +(const Point2D &a)
    {
        return {a.x + x, a.y + y};
    }

    Point2D operator *(const Point2D &a)
    {
        return {a.x * x, a.y * y};
    }

    Point2D& operator =(const Point2D &a)
    {
        x = a.x;
        y = a.y;
        return *this;
    }
};

#endif // GEOMETRY_H
