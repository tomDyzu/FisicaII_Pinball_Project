#include "Globals.h"
#include "Application.h"
#include "ModuleGame.h"
#include "ModulePhysics.h"

#include <vector>
#include <math.h>
#include <algorithm>

// Art ---------------------------------------------------------------
#define BACKGROUND_PATH   "Assets/0.png"            // background of the board (2:3 proportion)
#define LOGO_PATH         "Assets/logo.png"         // 1402x1122
#define BALL_PATH         "Assets/S1.png"           // image of the gold ball
#define EXTRA_BALL_PATH   "Assets/S2.png"           // image of the extra balls
#define DRAW_WALLS      1      // 0 = do not draw the walls (if the background image already has them)
#define WALL_THICKNESS  6.0f
#define WALL_COLOR      Color{ 40, 60, 110, 255 }
#define FLIPPER_COLOR   Color{ 230, 60, 60, 255 }

// Ball ---------------------------------------------------------------
#define BALL_RADIUS       12
#define BALL_RESTITUTION  0.4f
#define BALL_FRICTION     0.1f

// Game ---------------------------------------------------------------
#define GAME_TIME_SECONDS 300.0f   // 5 minutes

// Flippers -----------------------------------------------------------
#define FLIPPER_LENGTH         90
#define FLIPPER_HALF_THICKNESS 10
#define LEFT_FLIPPER_X         200
#define RIGHT_FLIPPER_X        (BOARD_WIDTH - LEFT_FLIPPER_X)
#define FLIPPER_Y              830

// Board layout (board pixels, 0,0 is the top left corner of the board) -
#define BOARD_BORDER        30    // distance from the edge of the board to the walls
#define WALL_LEFT           (BOARD_BORDER)
#define WALL_RIGHT          (BOARD_WIDTH - BOARD_BORDER)
#define WALL_TOP            (BOARD_BORDER)
#define CORNER_RADIUS       70    // rounded top corners
#define FUNNEL_RADIUS       120   // curve between the side walls and the slopes to the flippers

// Half-moon lane
#define LANE_WIDTH          60    // free space between the outer wall and the half-moon (the ball is 24)
#define LANE_WALL_THICKNESS 32
#define LANE_CORNER_RADIUS  30
#define LANE_LEG_END_Y      200   // where the two legs of the half-moon end

// The "sun": the line where the gold ball waits, moving by itself, until SPACE
#define SUN_Y               680
#define SUN_MIN_X           (WALL_LEFT + 60)
#define SUN_MAX_X           (WALL_RIGHT - 60)
#define SUN_SPEED           220.0f  // pixels per second

// Points and extra balls
#define LANE_POINTS          300    // a ball goes through the line in the middle of the half-moon
#define LANE_COOLDOWN        0.5f   // seconds before the same ball can score on that line again
#define EXTRA_BALL_INTERVAL  30.0f  // seconds of play between two extra balls
#define SPAWN_X              (BOARD_WIDTH / 2)   // where the extra balls appear (only a mark, no collision)
#define SPAWN_Y              180
#define SPAWN_MARK_RADIUS    16
#define SPAWN_RANDOM_X       10     // small random horizontal offset, in pixels

// Star bumpers: the only way to score. They move by themselves along a rail
#define STAR_POINTS         100
#define STAR_WALL_GAP       10    // free space between a star at the end of its rail and the wall
#define STAR_ROW1_Y         285
#define STAR_ROW2_Y         385
#define STAR_ROW3_Y         485

struct StarDef
{
	int x, y;     // starting position (the rail is the horizontal line at y)
	int radius;
	int dir;      // +1 starts moving right, -1 left
};

// Stars of the same row share the rail: they bump into each other
static const StarDef star_defs[STAR_COUNT] = {
	{ 170, STAR_ROW1_Y, 30,  1 }, { 430, STAR_ROW1_Y, 30,  1 },
	{ 300, STAR_ROW2_Y, 36, -1 },
	{ 170, STAR_ROW3_Y, 30, -1 }, { 430, STAR_ROW3_Y, 30, -1 }
};

// ---------------------------------------------------------------------
// Helpers to build chains
// IMPORTANT: chains are one-sided. Walk each chain so that the ball is always on your LEFT
// (as you see it on screen), otherwise the ball goes through it:
// - walls around the playfield: counter-clockwise on screen
// - a solid island inside the playfield (the half-moon): clockwise on screen
// ---------------------------------------------------------------------

static void AddPoint(std::vector<int>& pts, float x, float y)
{
	int ix = (int)floorf(x + 0.5f);
	int iy = (int)floorf(y + 0.5f);

	// Box2D does not accept two equal consecutive points
	size_t n = pts.size();
	if (n >= 2 && pts[n - 2] == ix && pts[n - 1] == iy)
		return;

	pts.push_back(ix);
	pts.push_back(iy);
}

// Screen angles in degrees (Y points down): 0 = right, 90 = down, 180 = left, 270 = up
static void AddArc(std::vector<int>& pts, float cx, float cy, float r, float from_deg, float to_deg, int steps)
{
	for (int i = 0; i <= steps; ++i)
	{
		float a = (from_deg + (to_deg - from_deg) * i / steps) * DEGTORAD;
		AddPoint(pts, cx + r * cosf(a), cy + r * sinf(a));
	}
}

// Left side: from the left wall, curve, and a straight slope that ends exactly on the top surface
// of the left flipper at rest (so the ball does not hit a step). Works with any FLIPPER_ANGLE_DEG
static void BuildLeftFunnel(std::vector<int>& pts)
{
	const float alpha = FLIPPER_ANGLE_DEG * DEGTORAD;
	const float r = FUNNEL_RADIUS;

	// End of the slope: the top surface of the flipper at rest
	const float tx = LEFT_FLIPPER_X + FLIPPER_HALF_THICKNESS * sinf(alpha);
	const float ty = FLIPPER_Y - FLIPPER_HALF_THICKNESS * cosf(alpha);

	// The curve turns from "going down" (90 degrees) to the direction of the slope (alpha).
	// Its center is on the playfield side. We ask the slope to reach (tx, ty) and get the height
	// where the straight wall has to end
	const float cx = WALL_LEFT + r;
	const float ex = cx - r * sinf(alpha);              // end of the curve
	const float ey = ty - tanf(alpha) * (tx - ex);
	const float cy = ey - r * cosf(alpha);

	const int steps = 10;
	for (int i = 0; i <= steps; ++i)
	{
		float h = (90.0f + (FLIPPER_ANGLE_DEG - 90.0f) * i / steps) * DEGTORAD;
		AddPoint(pts, cx - r * sinf(h), cy + r * cosf(h));
	}

	AddPoint(pts, tx, ty);
}

// Outer walls: starts at the right flipper, goes up the right side, across the top
// (rounded corners) and down the left side to the left flipper. Counter-clockwise on screen
static void BuildOuterWalls(std::vector<int>& pts)
{
	std::vector<int> left;
	BuildLeftFunnel(left);

	// Right funnel = left funnel mirrored and walked backwards (from the flipper to the wall)
	for (int i = (int)left.size() / 2 - 1; i >= 0; --i)
		AddPoint(pts, (float)(BOARD_WIDTH - left[i * 2]), (float)left[i * 2 + 1]);

	// Rounded top corners. The straight parts between arcs are added by the next point
	AddArc(pts, WALL_RIGHT - CORNER_RADIUS, WALL_TOP + CORNER_RADIUS, CORNER_RADIUS, 0.0f, -90.0f, 10);
	AddArc(pts, WALL_LEFT + CORNER_RADIUS, WALL_TOP + CORNER_RADIUS, CORNER_RADIUS, -90.0f, -180.0f, 10);

	for (size_t i = 0; i < left.size(); i += 2)
		AddPoint(pts, (float)left[i], (float)left[i + 1]);
}

// The half-moon is a solid band: both of its sides touch the ball (the lane above, the
// playfield below), so it is a closed loop. Clockwise on screen
static void BuildHalfMoon(std::vector<int>& pts)
{
	const float xl = WALL_LEFT + LANE_WIDTH;
	const float xr = WALL_RIGHT - LANE_WIDTH;
	const float yt = WALL_TOP + LANE_WIDTH;
	const float t = LANE_WALL_THICKNESS;
	const float ro = LANE_CORNER_RADIUS;       // outer radius of the corners
	const float ri = MAX(ro - t, 0.0f);        // inner radius: 0 = sharp corner (never negative)
	const float y_end = LANE_LEG_END_Y;

	// Outer side (the lane), from left to right
	AddPoint(pts, xl, y_end);
	AddArc(pts, xl + ro, yt + ro, ro, 180.0f, 270.0f, 8);
	AddArc(pts, xr - ro, yt + ro, ro, 270.0f, 360.0f, 8);
	AddPoint(pts, xr, y_end);

	// Inner side (the playfield), from right to left. The loop closes by itself.
	// The inner arcs are tangent to the inner faces (x = xr - t, x = xl + t, y = yt + t)
	AddPoint(pts, xr - t, y_end);
	AddArc(pts, xr - t - ri, yt + t + ri, ri, 360.0f, 270.0f, 8);
	AddArc(pts, xl + t + ri, yt + t + ri, ri, 270.0f, 180.0f, 8);
	AddPoint(pts, xl + t, y_end);
}

// ---------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------

// Draws a texture inside 'area' keeping its proportions, centered
static void DrawTextureFit(Texture2D texture, Rectangle area)
{
	float scale = MIN(area.width / texture.width, area.height / texture.height);

	Rectangle source = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
	Rectangle dest = {
		area.x + (area.width - texture.width * scale) / 2.0f,
		area.y + (area.height - texture.height * scale) / 2.0f,
		texture.width * scale,
		texture.height * scale
	};

	DrawTexturePro(texture, source, dest, Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
}

static void DrawCenteredText(const char* text, int y, int size, Color color)
{
	int w = MeasureText(text, size);
	DrawText(text, BOARD_WIDTH / 2 - w / 2, y, size, color);
}

// Placeholder for the star art: a five-pointed star drawn with lines
static void DrawStarShape(int cx, int cy, float r, Color color)
{
	Vector2 p[5];
	for (int i = 0; i < 5; ++i)
	{
		float a = (-90.0f + 72.0f * i) * DEGTORAD;
		p[i] = Vector2{ cx + r * cosf(a), cy + r * sinf(a) };
	}
	for (int i = 0; i < 5; ++i)
		DrawLineEx(p[i], p[(i + 2) % 5], 2.0f, color);
}

// TODO: placeholder panels. Replace with the logo and the highscores
static void DrawPanels(int score, float time_left, float next_ball, const Highscores& hs, int highlight)
{
	Color text = LIGHTGRAY;
	int left = LEFT_PANEL_X + 20;
	int right = RIGHT_PANEL_X + 20;

	int seconds = (int)ceilf(time_left);

	// Left panel: logo on the top half, score and time on the bottom half
	DrawLine(LEFT_PANEL_X + 10, BOARD_HEIGHT / 2, LEFT_PANEL_X + PANEL_WIDTH - 10, BOARD_HEIGHT / 2, GRAY);
	DrawText("SCORE", left, BOARD_HEIGHT / 2 + 40, 24, text);
	DrawText(TextFormat("%d", score), left, BOARD_HEIGHT / 2 + 75, 40, WHITE);
	DrawText("TIME", left, BOARD_HEIGHT / 2 + 160, 24, text);
	DrawText(TextFormat("%d:%02d", seconds / 60, seconds % 60), left, BOARD_HEIGHT / 2 + 195, 40, WHITE);

	DrawText("NEXT BALL", left, BOARD_HEIGHT / 2 + 270, 24, text);
	DrawText(TextFormat("%d", (int)ceilf(next_ball)), left, BOARD_HEIGHT / 2 + 305, 40, WHITE);

	// Right panel: highscores (top 6)
	const int list_top = 200;      // y of the first row
	const int row_height = 60;     // distance between rows

	DrawText("HIGHSCORES", right, 60, 28, text);
	DrawText("SCORE", right + 40, list_top - 30, 16, GRAY);
	DrawText("TIME", right + 150, list_top - 30, 16, GRAY);

	for (int i = 0; i < MAX_HIGHSCORES; ++i)
	{
		int y = list_top + i * row_height;
		Color c = (i == highlight) ? GOLD : WHITE;   // the result of the game that just ended

		DrawText(TextFormat("%d.", i + 1), right, y, 24, c);

		if (i < hs.Count())
		{
			const ScoreEntry& e = hs.Get(i);
			DrawText(TextFormat("%d", e.score), right + 40, y, 24, c);
			DrawText(TextFormat("%d:%02d", e.seconds / 60, e.seconds % 60), right + 150, y, 24, c);
		}
		else
		{
			DrawText("---", right + 40, y, 24, GRAY);
		}
	}
}

// Loads an image and cuts it into a circle, so the square corners are not seen
static Texture2D LoadBallTexture(const char* path)
{
	Texture2D texture = Texture2D{ 0 };

	Image img = LoadImage(path);
	if (img.data != NULL)
	{
		ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
		Color* px = (Color*)img.data;
		float r = MIN(img.width, img.height) / 2.0f;
		float cx = img.width / 2.0f;
		float cy = img.height / 2.0f;

		for (int y = 0; y < img.height; ++y)
		{
			for (int x = 0; x < img.width; ++x)
			{
				float dx = x + 0.5f - cx;
				float dy = y + 0.5f - cy;
				if (dx * dx + dy * dy > r * r)
					px[y * img.width + x].a = 0;
			}
		}

		texture = LoadTextureFromImage(img);
		UnloadImage(img);
	}

	if (texture.id == 0) LOG("Cannot load %s", path);
	return texture;
}

// ---------------------------------------------------------------------

ModuleGame::ModuleGame(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	state = STATE_WAITING;

	gold_ball = NULL;
	gold_dir = 1.0f;
	gold_lost = false;

	time_left = GAME_TIME_SECONDS;
	score = 0;

	for (int i = 0; i < STAR_COUNT; ++i)
		stars[i] = NULL;

	lane_sensor = NULL;
	gold_lane_cooldown = 0.0f;
	next_ball_time = EXTRA_BALL_INTERVAL;
	last_rank = -1;

	background = Texture2D{ 0 };
	logo = Texture2D{ 0 };
	ball_texture = Texture2D{ 0 };
	extra_ball_texture = Texture2D{ 0 };

	walls = NULL;
	half_moon = NULL;
	left_flipper = NULL;
	right_flipper = NULL;
	sensor = NULL;
}

ModuleGame::~ModuleGame()
{
}

bool ModuleGame::Start()
{
	LOG("Loading game");

	highscores.Load();

	background = LoadTexture(BACKGROUND_PATH);
	logo = LoadTexture(LOGO_PATH);
	if (background.id == 0) LOG("Cannot load %s", BACKGROUND_PATH);
	if (logo.id == 0) LOG("Cannot load %s", LOGO_PATH);

	// The ball images
	ball_texture = LoadBallTexture(BALL_PATH);
	extra_ball_texture = LoadBallTexture(EXTRA_BALL_PATH);

	// The images are much bigger than on screen: mipmaps and trilinear filter make them look better
	if (background.id != 0)
	{
		GenTextureMipmaps(&background);
		SetTextureFilter(background, TEXTURE_FILTER_TRILINEAR);
	}
	if (logo.id != 0)
	{
		GenTextureMipmaps(&logo);
		SetTextureFilter(logo, TEXTURE_FILTER_TRILINEAR);
	}

	std::vector<int> points;

	BuildOuterWalls(points);
	walls = App->physics->CreateChain(0, 0, points.data(), (int)points.size(), false);

	points.clear();
	BuildHalfMoon(points);
	half_moon = App->physics->CreateChain(0, 0, points.data(), (int)points.size(), true);

	left_flipper = App->physics->CreateFlipper(LEFT_FLIPPER_X, FLIPPER_Y, FLIPPER_LENGTH, FLIPPER_HALF_THICKNESS, true);
	right_flipper = App->physics->CreateFlipper(RIGHT_FLIPPER_X, FLIPPER_Y, FLIPPER_LENGTH, FLIPPER_HALF_THICKNESS, false);

	// Sensor just below the board
	sensor = App->physics->CreateRectangleSensor(BOARD_WIDTH / 2, BOARD_HEIGHT + 40, BOARD_WIDTH, 25);
	sensor->listener = this;

	// The line in the middle of the half-moon: a thin sensor across the whole lane
	lane_sensor = App->physics->CreateRectangleSensor(BOARD_WIDTH / 2, WALL_TOP + LANE_WIDTH / 2, 4, LANE_WIDTH / 2);
	lane_sensor->listener = this;

	StartNewGame();

	return true;
}

void ModuleGame::StartNewGame()
{
	// The whole board starts again
	DestroyStars();
	CreateStars();
	DestroyExtraBalls();

	App->physics->DestroyBody(gold_ball);

	// Kinematic while waiting: we move it ourselves and nothing pushes it
	gold_ball = App->physics->CreateCircle(SUN_MIN_X, SUN_Y, BALL_RADIUS, BALL_RESTITUTION, BALL_FRICTION, true);
	gold_ball->SetDynamic(false);

	gold_dir = 1.0f;
	gold_lost = false;
	last_rank = -1;
	gold_lane_cooldown = 0.0f;
	next_ball_time = EXTRA_BALL_INTERVAL;
	time_left = GAME_TIME_SECONDS;
	score = 0;
	state = STATE_WAITING;
}

void ModuleGame::CreateStars()
{
	for (int i = 0; i < STAR_COUNT; ++i)
	{
		const StarDef& d = star_defs[i];

		// The rail is limited by the wall (the star must not touch it)
		int min_x = WALL_LEFT + STAR_WALL_GAP + d.radius;
		int max_x = WALL_RIGHT - STAR_WALL_GAP - d.radius;

		stars[i] = App->physics->CreateRailCircle(d.x, d.y, d.radius, min_x, max_x, d.dir);
		stars[i]->listener = this;
	}
}

void ModuleGame::DestroyStars()
{
	for (int i = 0; i < STAR_COUNT; ++i)
	{
		App->physics->DestroyBody(stars[i]);
		stars[i] = NULL;
	}
}

bool ModuleGame::IsStar(PhysBody* body) const
{
	if (body == NULL)
		return false;

	for (int i = 0; i < STAR_COUNT; ++i)
	{
		if (stars[i] == body)
			return true;
	}

	return false;
}

void ModuleGame::EndGame()
{
	state = STATE_GAME_OVER;

	// Time played: how long the game lasted (not the time left)
	int seconds = (int)(GAME_TIME_SECONDS - time_left);
	if (seconds < 0) seconds = 0;
	if (seconds > (int)GAME_TIME_SECONDS) seconds = (int)GAME_TIME_SECONDS;

	last_rank = highscores.Add(score, seconds);
}

void ModuleGame::SpawnExtraBall()
{
	// It appears at the mark, with no speed and a small random horizontal offset
	int x = SPAWN_X + GetRandomValue(-SPAWN_RANDOM_X, SPAWN_RANDOM_X);

	ExtraBall extra;
	extra.body = App->physics->CreateCircle(x, SPAWN_Y, BALL_RADIUS, BALL_RESTITUTION, BALL_FRICTION, true);
	extra.lane_cooldown = 0.0f;

	extra_balls.push_back(extra);
}

void ModuleGame::DestroyExtraBalls()
{
	for (ExtraBall& e : extra_balls)
		App->physics->DestroyBody(e.body);

	extra_balls.clear();
	balls_to_remove.clear();
}

bool ModuleGame::IsBall(PhysBody* body) const
{
	if (body == NULL)
		return false;

	if (body == gold_ball)
		return true;

	for (const ExtraBall& e : extra_balls)
	{
		if (e.body == body)
			return true;
	}

	return false;
}

float* ModuleGame::LaneCooldownOf(PhysBody* body)
{
	if (body == NULL)
		return NULL;

	if (body == gold_ball)
		return &gold_lane_cooldown;

	for (ExtraBall& e : extra_balls)
	{
		if (e.body == body)
			return &e.lane_cooldown;
	}

	return NULL;
}

update_status ModuleGame::Update()
{
	// Art: the background of the board and the logo (if a file is missing, nothing is drawn)
	if (background.id != 0)
		DrawTextureFit(background, Rectangle{ 0.0f, 0.0f, (float)BOARD_WIDTH, (float)BOARD_HEIGHT });

	if (logo.id != 0)
		DrawTextureFit(logo, Rectangle{ (float)(LEFT_PANEL_X + 10), 10.0f, (float)(PANEL_WIDTH - 20), (float)(BOARD_HEIGHT / 2 - 20) });

	// Walls and flippers filled with a color
#if DRAW_WALLS
	walls->DrawFilled(WALL_COLOR, WALL_THICKNESS);
	half_moon->DrawFilled(WALL_COLOR, WALL_THICKNESS);
#endif
	left_flipper->DrawFilled(FLIPPER_COLOR);
	right_flipper->DrawFilled(FLIPPER_COLOR);

	DrawPanels(score, time_left, next_ball_time, highscores, last_rank);

	// Flippers: LEFT / A and RIGHT / D (disabled during game over)
	bool flippers_on = (state != STATE_GAME_OVER);
	App->physics->SetFlipperMotor(left_flipper, flippers_on && (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)));
	App->physics->SetFlipperMotor(right_flipper, flippers_on && (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)));

	// R = restart (debug)
	if (IsKeyPressed(KEY_R))
		StartNewGame();

	// Extra balls that fell out of the board (bodies are never destroyed inside a collision callback)
	for (PhysBody* pb : balls_to_remove)
	{
		for (size_t i = 0; i < extra_balls.size(); ++i)
		{
			if (extra_balls[i].body == pb)
			{
				App->physics->DestroyBody(pb);
				extra_balls.erase(extra_balls.begin() + i);
				break;
			}
		}
	}
	balls_to_remove.clear();

	// Cooldown of the line in the middle of the half-moon
	float dt = GetFrameTime();
	gold_lane_cooldown = MAX(gold_lane_cooldown - dt, 0.0f);
	for (ExtraBall& e : extra_balls)
		e.lane_cooldown = MAX(e.lane_cooldown - dt, 0.0f);

	// Marks (placeholders): where the extra balls appear and the scoring line of the half-moon
	DrawCircle(SPAWN_X, SPAWN_Y, (float)SPAWN_MARK_RADIUS, Fade(SKYBLUE, 0.3f));
	DrawCircleLines(SPAWN_X, SPAWN_Y, (float)SPAWN_MARK_RADIUS, SKYBLUE);
	DrawLineEx(Vector2{ (float)(BOARD_WIDTH / 2), (float)WALL_TOP },
		Vector2{ (float)(BOARD_WIDTH / 2), (float)(WALL_TOP + LANE_WIDTH) }, 2.0f, RED);

	// Extra balls
	for (const ExtraBall& e : extra_balls)
		DrawBall(e.body, BALL_RADIUS);

	// Rails and stars (placeholder drawings until there are textures)
	for (int i = 0; i < STAR_COUNT; ++i)
	{
		const StarDef& d = star_defs[i];
		DrawLineEx(Vector2{ (float)(WALL_LEFT + STAR_WALL_GAP + d.radius), (float)d.y },
			Vector2{ (float)(WALL_RIGHT - STAR_WALL_GAP - d.radius), (float)d.y }, 3.0f, GRAY);
	}

	for (int i = 0; i < STAR_COUNT; ++i)
	{
		if (stars[i] != NULL)
		{
			int x, y;
			stars[i]->GetPosition(x, y);
			DrawCircle(x, y, (float)star_defs[i].radius, ORANGE);
			DrawStarShape(x, y, star_defs[i].radius * 0.7f, WHITE);
		}
	}

	switch (state)
	{
	case STATE_WAITING:
	{
		// The gold ball goes from side to side by itself
		int x, y;
		gold_ball->GetPosition(x, y);

		if (x >= SUN_MAX_X) gold_dir = -1.0f;
		else if (x <= SUN_MIN_X) gold_dir = 1.0f;

		gold_ball->SetLinearVelocity(gold_dir * SUN_SPEED, 0.0f);

		// Where the ball moves, and a hint
		DrawLine(SUN_MIN_X, SUN_Y, SUN_MAX_X, SUN_Y, GRAY);
		DrawCenteredText("PRESS SPACE TO DROP THE BALL", SUN_Y + 30, 20, DARKGRAY);

		// SPACE: it falls straight down, the game and the time start
		if (IsKeyPressed(KEY_SPACE))
		{
			gold_ball->SetDynamic(true);
			state = STATE_PLAYING;
		}
	}
	break;

	case STATE_PLAYING:
	{
		time_left -= GetFrameTime();

		// A new ball every EXTRA_BALL_INTERVAL seconds of play
		next_ball_time -= GetFrameTime();
		if (next_ball_time <= 0.0f)
		{
			SpawnExtraBall();
			next_ball_time += EXTRA_BALL_INTERVAL;
		}

		if (time_left <= 0.0f)
		{
			time_left = 0.0f;
			EndGame();
		}

		// The game is lost when the gold ball leaves the board, even if there are other balls
		if (gold_lost && state == STATE_PLAYING)
			EndGame();
	}
	break;

	case STATE_GAME_OVER:
	{
		DrawRectangle(WALL_LEFT, 300, BOARD_WIDTH - 2 * WALL_LEFT, 230, Fade(BLACK, 0.75f));
		DrawCenteredText("GAME OVER", 325, 50, WHITE);
		DrawCenteredText(TextFormat("SCORE: %d", score), 400, 30, GOLD);
		if (last_rank >= 0)
			DrawCenteredText(TextFormat("NEW HIGHSCORE: #%d", last_rank + 1), 438, 20, GOLD);
		DrawCenteredText("PRESS SPACE TO PLAY AGAIN", 470, 20, LIGHTGRAY);

		if (IsKeyPressed(KEY_SPACE))
			StartNewGame();
	}
	break;
	}

	// The gold ball (drawn last so nothing covers it)
	if (gold_ball != NULL)
		DrawBall(gold_ball, BALL_RADIUS);

	// The image was not found: show where the game is looking for it
	if (ball_texture.id == 0)
		DrawText(TextFormat("NO IMAGE: %s/%s", GetWorkingDirectory(), BALL_PATH), 10, 10, 12, RED);
	if (extra_ball_texture.id == 0)
		DrawText(TextFormat("NO IMAGE: %s/%s", GetWorkingDirectory(), EXTRA_BALL_PATH), 10, 26, 12, RED);

	return UPDATE_CONTINUE;
}

bool ModuleGame::CleanUp()
{
	LOG("Unloading game");

	if (background.id != 0) UnloadTexture(background);
	if (logo.id != 0) UnloadTexture(logo);
	if (ball_texture.id != 0) UnloadTexture(ball_texture);
	if (extra_ball_texture.id != 0) UnloadTexture(extra_ball_texture);
	background = Texture2D{ 0 };
	logo = Texture2D{ 0 };
	ball_texture = Texture2D{ 0 };
	extra_ball_texture = Texture2D{ 0 };

	DestroyStars();
	DestroyExtraBalls();
	App->physics->DestroyBody(lane_sensor);
	App->physics->DestroyBody(gold_ball);
	App->physics->DestroyBody(walls);
	App->physics->DestroyBody(half_moon);
	App->physics->DestroyBody(left_flipper);
	App->physics->DestroyBody(right_flipper);
	App->physics->DestroyBody(sensor);

	gold_ball = walls = half_moon = left_flipper = right_flipper = sensor = lane_sensor = NULL;

	return true;
}

void ModuleGame::OnCollision(PhysBody* bodyA, PhysBody* bodyB)
{
	// bodyA is the body of this module's listener (the sensor), bodyB the one that touched it.
	// Never destroy bodies here (the world is locked): only raise the flag, Update() handles it
	// Bottom sensor: losing the gold ball ends the game, an extra ball just disappears
	if (bodyA == sensor)
	{
		if (state == STATE_PLAYING && bodyB == gold_ball)
			gold_lost = true;
		else if (bodyB != gold_ball && IsBall(bodyB)
			&& std::find(balls_to_remove.begin(), balls_to_remove.end(), bodyB) == balls_to_remove.end())
			balls_to_remove.push_back(bodyB);
	}

	// The line in the middle of the half-moon: points for every ball that goes through it
	if (state == STATE_PLAYING && bodyA == lane_sensor)
	{
		float* cooldown = LaneCooldownOf(bodyB);
		if (cooldown != NULL && *cooldown <= 0.0f)
		{
			score += LANE_POINTS;
			*cooldown = LANE_COOLDOWN;
		}
	}

	// The ball touches a star: points. Walls and other stars do not score
	if (state == STATE_PLAYING && IsStar(bodyA) && IsBall(bodyB))
		score += STAR_POINTS;

}

// Draws the ball image centered on the body: S1 for the gold ball, S2 for the extra balls
void ModuleGame::DrawBall(PhysBody* body, float radius)
{
	if (body == NULL)
		return;

	int x, y;
	body->GetPosition(x, y);

	bool is_gold = (body == gold_ball);
	Texture2D texture = is_gold ? ball_texture : extra_ball_texture;

	// The image did not load: draw a colored circle so the ball is always visible
	if (texture.id == 0)
	{
		DrawCircle(x, y, radius, is_gold ? GOLD : SKYBLUE);
		return;
	}

	Rectangle src = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
	Rectangle dst = { (float)x, (float)y, radius * 2.0f, radius * 2.0f };
	Vector2 origin = { radius, radius };

	DrawTexturePro(texture, src, dst, origin, 0.0f, WHITE);
}