#include "Globals.h"
#include "Application.h"
#include "ModuleRender.h"
#include "ModulePhysics.h"

#include <math.h>
#include <algorithm>

ModulePhysics::ModulePhysics(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	world = NULL;
	ground = NULL;
	mouse_joint = NULL;
	debug = true;
	accumulator = 0.0f;
}

// Destructor
ModulePhysics::~ModulePhysics()
{
}

bool ModulePhysics::Start()
{
	LOG("Creating Physics 2D environment");

	// Assign the member variable (do NOT declare a local "b2World* world" here, it would shadow the member)
	world = new b2World(b2Vec2(GRAVITY_X, GRAVITY_Y));

	world->SetContactListener(this);

	// Needed to create joints like the mouse joint
	b2BodyDef bd;
	ground = world->CreateBody(&bd);

	return true;
}

update_status ModulePhysics::PreUpdate()
{
	// Circles on a rail: choose the direction of their motor
	for (PhysBody* pb : rail_bodies)
	{
		b2PrismaticJoint* j = pb->rail_joint;

		// After a push the star follows the direction it is moving in
		float vx = pb->body->GetLinearVelocity().x;
		if (vx > 0.2f) pb->rail_dir = 1.0f;
		else if (vx < -0.2f) pb->rail_dir = -1.0f;

		// At the ends of the rail it turns around
		float t = j->GetJointTranslation();
		if (t <= j->GetLowerLimit() + 0.02f) pb->rail_dir = 1.0f;
		else if (t >= j->GetUpperLimit() - 0.02f) pb->rail_dir = -1.0f;

		j->SetMotorSpeed(pb->rail_dir * RAIL_SPEED);
	}

	// Fixed timestep: the physics advance in steps of FIXED_DT, whatever the frame rate is
	accumulator += GetFrameTime();
	if (accumulator > MAX_ACCUMULATED_TIME) accumulator = MAX_ACCUMULATED_TIME;

	while (accumulator >= FIXED_DT)
	{
		world->Step(FIXED_DT, 8, 3);
		accumulator -= FIXED_DT;
	}

	return UPDATE_CONTINUE;
}

update_status ModulePhysics::PostUpdate()
{
	if (IsKeyPressed(KEY_F1))
	{
		debug = !debug;
	}

	if (!debug)
	{
		// The mouse joint only lives in debug mode
		if (mouse_joint != NULL)
		{
			world->DestroyJoint(mouse_joint);
			mouse_joint = NULL;
		}
		return UPDATE_CONTINUE;
	}

	// The board is drawn with a camera: convert the mouse to board coordinates
	Vector2 mouse_world = App->renderer->GetMouseWorld();
	b2Vec2 mouse_position(PIXEL_TO_METERS(mouse_world.x), PIXEL_TO_METERS(mouse_world.y));
	b2Body* body_clicked = NULL;

	// Bonus code: this will iterate all objects in the world and draw the circles
	for (b2Body* b = world->GetBodyList(); b; b = b->GetNext())
	{
		for (b2Fixture* f = b->GetFixtureList(); f; f = f->GetNext())
		{
			switch (f->GetType())
			{
				// Draw circles ------------------------------------------------
			case b2Shape::e_circle:
			{
				b2CircleShape* shape = (b2CircleShape*)f->GetShape();
				b2Vec2 pos = f->GetBody()->GetPosition();

				DrawCircle(METERS_TO_PIXELS(pos.x), METERS_TO_PIXELS(pos.y), (float)METERS_TO_PIXELS(shape->m_radius), Color{ 0, 0, 0, 128 });

				// Radius line to see the rotation
				b2Vec2 edge = b->GetWorldPoint(b2Vec2(shape->m_radius, 0.0f));
				DrawLine(METERS_TO_PIXELS(pos.x), METERS_TO_PIXELS(pos.y), METERS_TO_PIXELS(edge.x), METERS_TO_PIXELS(edge.y), WHITE);
			}
			break;

			// Draw polygons ------------------------------------------------
			case b2Shape::e_polygon:
			{
				b2PolygonShape* polygonShape = (b2PolygonShape*)f->GetShape();
				int32 count = polygonShape->m_count;
				b2Vec2 prev, v;

				for (int32 i = 0; i < count; ++i)
				{
					v = b->GetWorldPoint(polygonShape->m_vertices[i]);
					if (i > 0)
						DrawLine(METERS_TO_PIXELS(prev.x), METERS_TO_PIXELS(prev.y), METERS_TO_PIXELS(v.x), METERS_TO_PIXELS(v.y), RED);

					prev = v;
				}

				v = b->GetWorldPoint(polygonShape->m_vertices[0]);
				DrawLine(METERS_TO_PIXELS(prev.x), METERS_TO_PIXELS(prev.y), METERS_TO_PIXELS(v.x), METERS_TO_PIXELS(v.y), RED);
			}
			break;

			// Draw chains contour -------------------------------------------
			case b2Shape::e_chain:
			{
				b2ChainShape* shape = (b2ChainShape*)f->GetShape();
				b2Vec2 prev, v;

				for (int32 i = 0; i < shape->m_count; ++i)
				{
					v = b->GetWorldPoint(shape->m_vertices[i]);
					if (i > 0)
						DrawLine(METERS_TO_PIXELS(prev.x), METERS_TO_PIXELS(prev.y), METERS_TO_PIXELS(v.x), METERS_TO_PIXELS(v.y), GREEN);
					prev = v;
				}

			}
			break;

			// Draw a single segment(edge) ----------------------------------
			case b2Shape::e_edge:
			{
				b2EdgeShape* shape = (b2EdgeShape*)f->GetShape();
				b2Vec2 v1, v2;

				v1 = b->GetWorldPoint(shape->m_vertex1);
				v2 = b->GetWorldPoint(shape->m_vertex2);
				DrawLine(METERS_TO_PIXELS(v1.x), METERS_TO_PIXELS(v1.y), METERS_TO_PIXELS(v2.x), METERS_TO_PIXELS(v2.y), BLUE);
			}
			break;
			}

			// TODO 1: If mouse button 1 is pressed, test if the current body contains mouse position
			// Only dynamic bodies can be dragged
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse_joint == NULL && body_clicked == NULL
				&& b->GetType() == b2_dynamicBody && f->TestPoint(mouse_position))
			{
				body_clicked = b;
			}
		}
	}

	// TODO 2: If a body was selected, create a mouse joint
	// using mouse_joint class property
	if (body_clicked != NULL)
	{
		b2MouseJointDef def;
		def.bodyA = ground;
		def.bodyB = body_clicked;
		def.target = mouse_position;
		def.maxForce = 100.0f * body_clicked->GetMass();
		b2LinearStiffness(def.stiffness, def.damping, 5.0f, 0.7f, def.bodyA, def.bodyB); // 5 Hz, damping ratio 0.7

		mouse_joint = (b2MouseJoint*)world->CreateJoint(&def);
		body_clicked->SetAwake(true);
	}

	// TODO 3: If the player keeps pressing the mouse button, update
	// target position and draw a red line between both anchor points
	if (mouse_joint != NULL && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
	{
		mouse_joint->SetTarget(mouse_position);

		b2Vec2 anchor_a = mouse_joint->GetAnchorA(); // the target
		b2Vec2 anchor_b = mouse_joint->GetAnchorB(); // the point of the body we clicked
		DrawLine(METERS_TO_PIXELS(anchor_a.x), METERS_TO_PIXELS(anchor_a.y), METERS_TO_PIXELS(anchor_b.x), METERS_TO_PIXELS(anchor_b.y), RED);
	}

	// TODO 4: If the player releases the mouse button, destroy the joint
	if (mouse_joint != NULL && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
	{
		world->DestroyJoint(mouse_joint);
		mouse_joint = NULL;
	}

	return UPDATE_CONTINUE;
}


// Called before quitting
bool ModulePhysics::CleanUp()
{
	LOG("Destroying physics world");

	// Delete the whole physics world! (this also frees every body, fixture and joint it owns)
	delete world;
	world = NULL;
	rail_bodies.clear();
	ground = NULL;
	mouse_joint = NULL;

	return true;
}

// PhysBody pointer as UserData: set it in the b2BodyDef BEFORE calling CreateBody()
PhysBody* ModulePhysics::CreateCircle(int x, int y, int radius, float restitution, float friction, bool bullet)
{
	PhysBody* pbody = new PhysBody();

	b2BodyDef body;
	body.type = b2_dynamicBody;
	body.bullet = bullet;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	b2CircleShape shape;
	shape.m_radius = PIXEL_TO_METERS(radius);

	b2FixtureDef fixture;
	fixture.shape = &shape;
	fixture.density = 1.0f;
	fixture.restitution = restitution;
	fixture.friction = friction;
	b->CreateFixture(&fixture);

	pbody->body = b;

	return pbody;
}

PhysBody* ModulePhysics::CreateRectangle(int x, int y, int half_width, int half_height)
{
	PhysBody* pbody = new PhysBody();

	b2BodyDef body;
	body.type = b2_dynamicBody;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	b2PolygonShape box;
	box.SetAsBox(PIXEL_TO_METERS(half_width), PIXEL_TO_METERS(half_height));

	b2FixtureDef fixture;
	fixture.shape = &box;
	fixture.density = 1.0f;
	b->CreateFixture(&fixture);

	pbody->body = b;

	return pbody;
}

// Static box that detects overlaps but does not collide (a trigger)
PhysBody* ModulePhysics::CreateRectangleSensor(int x, int y, int half_width, int half_height)
{
	PhysBody* pbody = new PhysBody();

	b2BodyDef body;
	body.type = b2_staticBody;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	b2PolygonShape box;
	box.SetAsBox(PIXEL_TO_METERS(half_width), PIXEL_TO_METERS(half_height));

	b2FixtureDef fixture;
	fixture.shape = &box;
	fixture.isSensor = true;
	b->CreateFixture(&fixture);

	pbody->body = b;

	return pbody;
}

// size is the number of ints, not vertices
PhysBody* ModulePhysics::CreateChain(int x, int y, const int* points, int size, bool loop)
{
	PhysBody* pbody = new PhysBody();

	b2BodyDef body;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	int count = size / 2;
	b2Vec2* vertices = new b2Vec2[count];
	for (int i = 0; i < count; ++i)
	{
		vertices[i].x = PIXEL_TO_METERS(points[i * 2 + 0]);
		vertices[i].y = PIXEL_TO_METERS(points[i * 2 + 1]);
	}

	b2ChainShape shape;
	if (loop)
		shape.CreateLoop(vertices, count);
	else
		// Box2D 2.4.1+: the ends need "ghost" vertices. Repeating the end points is fine
		shape.CreateChain(vertices, count, vertices[0], vertices[count - 1]);
	delete[] vertices; // both functions copy the vertices

	b2FixtureDef fixture;
	fixture.shape = &shape;
	fixture.friction = 0.1f;
	b->CreateFixture(&fixture);

	pbody->body = b;

	return pbody;
}

PhysBody* ModulePhysics::CreateFlipper(int x, int y, int length, int half_thickness, bool is_left)
{
	PhysBody* pbody = new PhysBody();

	// Screen Y points down, so a POSITIVE angle turns the flipper clockwise on screen:
	// - left flipper: rests at +angle (tip down) and is raised towards -angle
	// - right flipper: rests at -angle (tip down) and is raised towards +angle
	float angle = FLIPPER_ANGLE_DEG * DEGTORAD;
	float rest_angle = is_left ? angle : -angle;
	float side = is_left ? 1.0f : -1.0f; // which side of the pivot the flipper extends to

	b2BodyDef body;
	body.type = b2_dynamicBody;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.angle = rest_angle;
	body.allowSleep = false; // a sleeping flipper would ignore the motor
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	// Tapered shape, the pivot is the origin of the body
	float l = PIXEL_TO_METERS(length);
	float r1 = PIXEL_TO_METERS(half_thickness);
	float r2 = r1 * 0.5f;

	b2Vec2 v[4];
	v[0].Set(0.0f, -r1);
	v[1].Set(side * l, -r2);
	v[2].Set(side * l, r2);
	v[3].Set(0.0f, r1);

	b2PolygonShape shape;
	shape.Set(v, 4);

	b2FixtureDef fixture;
	fixture.shape = &shape;
	fixture.density = 1.0f;
	fixture.friction = 0.3f;
	fixture.restitution = 0.1f;
	b->CreateFixture(&fixture);

	// Revolute joint to the ground at the pivot (the origin of the body, NOT its center of mass)
	b2RevoluteJointDef jd;
	jd.Initialize(ground, b, b->GetPosition());
	jd.referenceAngle = 0.0f; // so the joint angle is the same as the body angle
	jd.enableLimit = true;
	jd.lowerAngle = -angle;
	jd.upperAngle = angle;
	jd.enableMotor = true;
	jd.maxMotorTorque = FLIPPER_TORQUE;

	pbody->active_dir = is_left ? -1.0f : 1.0f;
	jd.motorSpeed = -pbody->active_dir * FLIPPER_SPEED; // starts going to the rest position

	pbody->joint = (b2RevoluteJoint*)world->CreateJoint(&jd);
	pbody->body = b;

	return pbody;
}

void ModulePhysics::SetFlipperMotor(PhysBody* flipper, bool pressed)
{
	if (flipper == NULL || flipper->joint == NULL)
		return;

	float dir = pressed ? flipper->active_dir : -flipper->active_dir;
	flipper->joint->SetMotorSpeed(dir * FLIPPER_SPEED);
}

PhysBody* ModulePhysics::CreateRailCircle(int x, int y, int radius, int rail_min_x, int rail_max_x, int start_dir)
{
	PhysBody* pbody = new PhysBody();

	b2BodyDef body;
	body.type = b2_dynamicBody;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.gravityScale = 0.0f;  // it only moves along its rail
	body.allowSleep = false;   // a sleeping body would ignore the motor
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody);

	b2Body* b = world->CreateBody(&body);

	b2CircleShape shape;
	shape.m_radius = PIXEL_TO_METERS(radius);

	b2FixtureDef fixture;
	fixture.shape = &shape;
	fixture.density = 1.0f;
	fixture.friction = 0.1f;
	fixture.restitution = 0.6f; // the ball bounces off it
	b->CreateFixture(&fixture);

	// Prismatic joint to the ground: the body can only slide along X. The translation is
	// measured from the starting position, so the limits are the distance to both ends of the rail
	b2PrismaticJointDef jd;
	jd.Initialize(ground, b, b->GetWorldCenter(), b2Vec2(1.0f, 0.0f));
	jd.enableLimit = true;
	jd.lowerTranslation = PIXEL_TO_METERS(rail_min_x - x);
	jd.upperTranslation = PIXEL_TO_METERS(rail_max_x - x);

	// Weak motor: it keeps the normal speed, but a ball can push the body much faster than that.
	// Afterwards the motor brings the speed back little by little
	jd.enableMotor = true;
	jd.maxMotorForce = RAIL_FORCE;

	pbody->rail_dir = (start_dir >= 0) ? 1.0f : -1.0f;
	jd.motorSpeed = pbody->rail_dir * RAIL_SPEED;

	pbody->rail_joint = (b2PrismaticJoint*)world->CreateJoint(&jd);
	pbody->body = b;

	rail_bodies.push_back(pbody);

	return pbody;
}

void ModulePhysics::DestroyBody(PhysBody* pbody)
{
	if (pbody == NULL)
		return;

	// Box2D also destroys the joints of this body: forget the mouse joint
	if (mouse_joint != NULL && mouse_joint->GetBodyB() == pbody->body)
		mouse_joint = NULL;

	rail_bodies.erase(std::remove(rail_bodies.begin(), rail_bodies.end(), pbody), rail_bodies.end());

	world->DestroyBody(pbody->body); // Box2D frees the body and its fixtures
	delete pbody;                    // we free our own wrapper
}

// Call the listeners that are not NULL
void ModulePhysics::BeginContact(b2Contact* contact)
{
	PhysBody* a = (PhysBody*)contact->GetFixtureA()->GetBody()->GetUserData().pointer;
	PhysBody* b = (PhysBody*)contact->GetFixtureB()->GetBody()->GetUserData().pointer;

	if (a != NULL && a->listener != NULL)
		a->listener->OnCollision(a, b);

	if (b != NULL && b->listener != NULL)
		b->listener->OnCollision(b, a);
}

// PhysBody ------------------------------------
void PhysBody::GetPosition(int& x, int& y) const
{
	b2Vec2 pos = body->GetPosition();
	x = METERS_TO_PIXELS(pos.x);
	y = METERS_TO_PIXELS(pos.y);
}

float PhysBody::GetRotation() const
{
	return body->GetAngle();
}

bool PhysBody::Contains(int x, int y) const
{
	b2Vec2 p(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));

	// True if the point is inside ANY of the shapes of this body
	for (b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
	{
		if (fixture->GetShape()->TestPoint(body->GetTransform(), p))
			return true;
	}

	return false;
}

int PhysBody::RayCast(int x1, int y1, int x2, int y2, float& normal_x, float& normal_y) const
{
	// No hit: return -1
	// Hit: fill normal_x and normal_y and return the distance between x1,y1 and the hit point
	int ret = -1;

	b2RayCastInput input;
	input.p1.Set(PIXEL_TO_METERS(x1), PIXEL_TO_METERS(y1));
	input.p2.Set(PIXEL_TO_METERS(x2), PIXEL_TO_METERS(y2));
	input.maxFraction = 1.0f;

	float closest = 1.0f;

	for (b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
	{
		// A chain has one child per edge
		for (int32 i = 0; i < fixture->GetShape()->GetChildCount(); ++i)
		{
			b2RayCastOutput output;
			if (fixture->GetShape()->RayCast(&output, input, body->GetTransform(), i) && output.fraction <= closest)
			{
				closest = output.fraction;

				float dx = (float)(x2 - x1);
				float dy = (float)(y2 - y1);
				ret = (int)(output.fraction * sqrtf(dx * dx + dy * dy));

				normal_x = output.normal.x;
				normal_y = output.normal.y;
			}
		}
	}

	return ret;
}

void PhysBody::SetLinearVelocity(float vx, float vy)
{
	body->SetLinearVelocity(b2Vec2(PIXEL_TO_METERS(vx), PIXEL_TO_METERS(vy)));
}

void PhysBody::SetDynamic(bool dynamic)
{
	body->SetType(dynamic ? b2_dynamicBody : b2_kinematicBody);
	body->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
	body->SetAngularVelocity(0.0f);
	body->SetAwake(true);
}