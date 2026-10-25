#pragma once

#include "Module.h"
#include "Globals.h"

#include "box2d/box2d.h"

#include <vector>

// Gravity in m/s^2. Screen Y grows DOWNWARDS and we convert meters <-> pixels without flipping Y,
// so a POSITIVE Y gravity makes bodies fall down on screen.
// Pinball tables are tilted, so this is lower than real gravity (9.8). Tweak it until it feels right.
#define GRAVITY_X 0.0f
#define GRAVITY_Y 12.0f

// Fixed timestep: the physics always advance in steps of 1/60 s, whatever the frame rate is
#define FIXED_DT (1.0f / 60.0f)
#define MAX_ACCUMULATED_TIME 0.25f // avoids the "spiral of death" after a long freeze

// Flippers. Tweak these until they feel right
#define FLIPPER_ANGLE_DEG 30.0f   // the flipper moves between -30 and +30 degrees (60 total)
#define FLIPPER_SPEED     15.0f   // motor speed in rad/s: higher = harder hit
#define FLIPPER_TORQUE    1000.0f // max motor torque: must be high enough to win against the ball

// Circles that move along a horizontal rail (the star bumpers)
#define RAIL_SPEED        1.8f    // normal speed in m/s (90 pixels/s)
#define RAIL_FORCE        10.0f   // max force of the motor: how fast it gets back to its normal speed after a push

// Units: Box2D works in meters (float), raylib draws in pixels (int)
#define PIXELS_PER_METER 50.0f // if touched change METER_PER_PIXEL too
#define METER_PER_PIXEL 0.02f  // this is 1 / PIXELS_PER_METER !

#define METERS_TO_PIXELS(m) ((int) floor(PIXELS_PER_METER * (m)))
#define PIXEL_TO_METERS(p)  ((float) METER_PER_PIXEL * (p))

// Small class to return to other modules to track position and rotation of physics bodies
class PhysBody
{
public:
	PhysBody() : body(NULL), listener(NULL), joint(NULL), active_dir(0.0f), rail_joint(NULL), rail_dir(1.0f)
	{
	}

	void GetPosition(int& x, int& y) const;
	float GetRotation() const;
	bool Contains(int x, int y) const;
	int RayCast(int x1, int y1, int x2, int y2, float& normal_x, float& normal_y) const;

	void SetLinearVelocity(float vx, float vy);   // pixels per second
	void SetDynamic(bool dynamic);                // false = kinematic (we move it), true = normal body
	void DrawFilled(Color color, float thickness = 8.0f) const;

public:
	b2Body* body;
	// Module that listens to the collisions of this body
	Module* listener;

	// Only used by flippers
	b2RevoluteJoint* joint;
	float active_dir; // sign of the motor speed that raises the flipper

	// Only used by circles on a rail
	b2PrismaticJoint* rail_joint;
	float rail_dir;   // +1 moving right, -1 moving left
};

// Module --------------------------------------
class ModulePhysics : public Module, public b2ContactListener
{
public:
	ModulePhysics(Application* app, bool start_enabled = true);
	~ModulePhysics();

	bool Start();
	update_status PreUpdate();
	update_status PostUpdate();
	bool CleanUp();

	// restitution: 0 = no bounce, 1 = perfect bounce. bullet: use continuous collision (fast bodies)
	PhysBody* CreateCircle(int x, int y, int radius, float restitution = 0.0f, float friction = 0.2f, bool bullet = false);
	PhysBody* CreateRectangle(int x, int y, int half_width, int half_height);
	PhysBody* CreateRectangleSensor(int x, int y, int half_width, int half_height);

	// Static chain. loop = true closes the shape, false leaves it open (walls, ramps)
	PhysBody* CreateChain(int x, int y, const int* points, int size, bool loop = true);

	// (x, y) is the pivot. A left flipper extends to the right of the pivot, a right one to the left
	PhysBody* CreateFlipper(int x, int y, int length, int half_thickness, bool is_left);
	// pressed = true raises the flipper, false lowers it back to its resting position
	void SetFlipperMotor(PhysBody* flipper, bool pressed);

	// Dynamic circle with no gravity that can only move along the horizontal line at height y,
	// between rail_min_x and rail_max_x (the position of its center). It keeps moving by itself,
	// reverses at the ends of the rail, and gets back to its normal speed after a push.
	// start_dir: +1 right, -1 left
	PhysBody* CreateRailCircle(int x, int y, int radius, int rail_min_x, int rail_max_x, int start_dir);

	void DestroyBody(PhysBody* pbody);

	void BeginContact(b2Contact* contact) override;

private:

	bool debug;
	b2World* world;
	float accumulator;

	std::vector<PhysBody*> rail_bodies; // updated every frame in PreUpdate

	// Static body with no fixtures, needed to create joints like the mouse joint
	b2Body* ground;
	b2MouseJoint* mouse_joint;
};