#include "Globals.h"
#include "Application.h"
#include "ModuleGame.h"
#include "ModulePhysics.h"
#include "ModuleWindow.h"
#include "ModuleAudio.h"

#include <vector>
#include <math.h>
#include <algorithm>
#include <time.h>

// Art ---------------------------------------------------------------
#define BACKGROUND_PATH   "Assets/Back1.png"           
#define LOGO_PATH         "Assets/logo.png"         
#define BALL_PATH         "Assets/S1.png"           // imagen de la bola dorada
#define EXTRA_BALL_PATH   "Assets/S2.png"           // image de las bolas extras
#define SPAWN_PATH        "Assets/black.png"   // imang donde aparecen las bolas
#define SPAWN_IMAGE_SIZE  140                  
#define COMBO_FX_PATH     "Assets/Audio/Fx/combo.wav"   // sound when all the letters are hit
#define LAUNCH_FX_PATH    "Assets/Audio/Fx/inicio.wav"   // sound when the plunger is released at full charge
#define ROLL_FX_PATH      "Assets/Audio/Fx/ball_release.wav"   // ball going up the lane
#define BOUNCE_FX_COUNT   2
static const char* bounce_fx_paths[BOUNCE_FX_COUNT] =
{
	"Assets/Audio/Fx/bounce1.wav",
	"Assets/Audio/Fx/bounce2.wav"
};
#define BOUNCE_MIN_SPEED  1.5f    // softer touches do not sound (rolling, resting)
#define BOUNCE_COOLDOWN   0.1f    // seconds between two bounce sounds
#define LEFT_FLIPPER_FX_PATH   "Assets/Audio/Fx/flipper1.wav"  // left flipper
#define RIGHT_FLIPPER_FX_PATH  "Assets/Audio/Fx/flipper2.wav"  // right flipper
#define BUMPER_FX_COUNT   3
static const char* bumper_fx_paths[BUMPER_FX_COUNT] =
{
	"Assets/Audio/Fx/bumper1.wav",
	"Assets/Audio/Fx/bumper2.wav",
	"Assets/Audio/Fx/bumper3.wav"
};
#define RESTART_FX_PATH   "Assets/Audio/Fx/reinicio.wav"   // sound when a game starts or restarts
#define GAME_OVER_FX_PATH "Assets/Audio/Fx/gameOver.wav"   // melody when the game is over
#define DRAW_WALLS      1
#define WALL_THICKNESS  6.0f
#define WALL_COLOR      Color{ 150, 60, 255, 255 }   // purple (same as the launch text)
#define WALL_OUTLINE_COLOR WHITE                      // border of the walls
#define WALL_OUTLINE    2.0f                          // thickness of the border on each side (pixels)
#define FLIPPER_COLOR   Color{ 230, 60, 60, 255 }
#define GOLD_BRIGHTNESS 1.3f  
#define LANE_COLOR      Color{ 45, 45, 60, 255 }   

// Ball ---------------------------------------------------------------
#define BALL_RADIUS       12
#define BALL_RESTITUTION  0.4f
#define BALL_FRICTION     0.1f

// Game ---------------------------------------------------------------
#define GAME_TIME_SECONDS 240.0f   

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
#define CORNER_RADIUS       70   
#define FUNNEL_RADIUS       120

// Half-moon lane
#define LANE_WIDTH          60   
#define LANE_WALL_THICKNESS 32
#define LANE_CORNER_RADIUS  30
#define LANE_LEG_END_Y      200   

// Launcher lane, to the right of the playfield art. The border of the art (x from WALL_RIGHT to BOARD_WIDTH)
// is the thickness of the wall between the playfield and the lane
#define LAUNCH_LANE_WIDTH     44                                   // free width of the lane (the ball is 24)
#define LAUNCH_DIVIDER_X      (BOARD_WIDTH)                         // wall between the lane and the border of the art
#define LAUNCH_LANE_OUT_X     (LAUNCH_DIVIDER_X + LAUNCH_LANE_WIDTH) // outer wall of the lane
#define LAUNCH_LANE_X         (LAUNCH_DIVIDER_X + LAUNCH_LANE_WIDTH / 2) // center of the lane
#define LAUNCH_LANE_FLOOR_Y   890
#define LAUNCH_LANE_ARC       LAUNCH_LANE_WIDTH                    // radius of the curve at the top of the lane
#define DIVIDER_TOP_Y         (WALL_TOP + CORNER_RADIUS)           // the divider ends where the corner arc starts

// Plunger: hold S / DOWN to pull it back, release to shoot the ball
#define PLUNGER_HALF_W        18
#define PLUNGER_HALF_H        6
#define PLUNGER_REST_Y        820
#define PLUNGER_PULL          40      
#define PLUNGER_CHARGE_TIME   1.0f     
#define PLUNGER_SPEED_MIN     250.0f   
#define PLUNGER_SPEED_MAX     900.0f   

// Points and extra balls
#define LANE_POINTS          300    
#define LANE_COOLDOWN        0.5f  
#define EXTRA_BALL_INTERVAL  20.0f  
#define SPAWN_X              (BOARD_WIDTH / 2)   
#define SPAWN_Y              180
#define SPAWN_MARK_RADIUS    16
#define SPAWN_RANDOM_X       10     

// Star bumpers: the only way to score. They move by themselves along a rail
#define STAR_POINTS         100
#define STAR_WALL_GAP       10   
#define STAR_ROW1_Y         285
#define STAR_ROW2_Y         385
#define STAR_ROW3_Y         485

// Combo: every star is a letter of this word (same order as star_defs). Hit them all to get a bonus
#define COMBO_WORD          "BONUS"  
#define COMBO_POINTS        1000
#define COMBO_SHOW_TIME     1.5f      

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

// The lane of the launcher, to the right of the playfield. Counter-clockwise on screen (the ball is inside):
// along the floor, up the outer wall, around the curve at the top and left along the ceiling until it
// meets the top wall of the playfield.
// The corner arc of the playfield is one-sided: a ball that comes from the lane goes through it (it is
// behind the arc) and a ball that comes from the playfield cannot enter the lane. A one-way gate for free
static void BuildLaunchLane(std::vector<int>& pts)
{
	const float r = LAUNCH_LANE_ARC;

	AddPoint(pts, LAUNCH_DIVIDER_X, LAUNCH_LANE_FLOOR_Y);
	AddPoint(pts, LAUNCH_LANE_OUT_X, LAUNCH_LANE_FLOOR_Y);
	AddArc(pts, LAUNCH_LANE_OUT_X - r, WALL_TOP + r, r, 0.0f, -90.0f, 10);
	AddPoint(pts, WALL_RIGHT - CORNER_RADIUS, WALL_TOP);
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

// centered text with an outline. The outline color is drawn all around, the text on top
static void DrawCenteredTextOutline(const char* text, int y, int size, Color color, Color outline, int thickness)
{
	int w = MeasureText(text, size);
	int x = BOARD_WIDTH / 2 - w / 2;

	for (int dx = -thickness; dx <= thickness; ++dx)
	{
		for (int dy = -thickness; dy <= thickness; ++dy)
		{
			if (dx != 0 || dy != 0)
				DrawText(text, x + dx, y + dy, size, outline);
		}
	}

	DrawText(text, x, y, size, color);
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

// The plunger: a head, a spring down to the floor of the lane and a power bar (placeholder drawing)
static void DrawPlunger(int x, int y, float charge)
{
	// Spring: a zigzag between the head and the floor
	const int coils = 8;
	float top = (float)(y + PLUNGER_HALF_H);
	float bottom = (float)LAUNCH_LANE_FLOOR_Y;
	Vector2 prev = Vector2{ (float)x, top };
	for (int i = 1; i <= coils; ++i)
	{
		float t = (float)i / coils;
		Vector2 p = Vector2{ x + ((i % 2) ? 10.0f : -10.0f), top + (bottom - top) * t };
		if (i == coils) p.x = (float)x;
		DrawLineEx(prev, p, 3.0f, DARKGRAY);
		prev = p;
	}

	DrawRectangle(x - PLUNGER_HALF_W, y - PLUNGER_HALF_H, PLUNGER_HALF_W * 2, PLUNGER_HALF_H * 2, GRAY);

	// Power bar (in the empty corner next to the lane): green when empty, red when full
	int bar_x = WALL_RIGHT - 32;
	int bar_y = 770;
	int bar_w = 14;
	int bar_h = 110;
	int fill = (int)(bar_h * charge);
	Color c = Color{ (unsigned char)(255 * charge), (unsigned char)(255 * (1.0f - charge)), 0, 255 };

	DrawRectangle(bar_x, bar_y, bar_w, bar_h, Fade(BLACK, 0.5f));
	DrawRectangle(bar_x, bar_y + bar_h - fill, bar_w, fill, c);
	DrawRectangleLines(bar_x, bar_y, bar_w, bar_h, WHITE);
}

// Letters of the combo word in the left panel: they light up when their star is hit
static void DrawCombo(const bool* lit, float flash)
{
	int left = LEFT_PANEL_X + 20;
	int y = BOARD_HEIGHT - 75;

	DrawText("COMBO", left, y - 22, 16, GRAY);

	for (int i = 0; i < STAR_COUNT; ++i)
	{
		const char letter[2] = { COMBO_WORD[i], '\0' };
		DrawText(letter, left + i * 38, y, 40, lit[i] ? GOLD : DARKGRAY);
	}

	// The combo was completed: message over the board
	if (flash > 0.0f)
	{
		DrawRectangle(WALL_LEFT, 545, BOARD_WIDTH - 2 * WALL_LEFT, 100, Fade(BLACK, 0.6f));
		DrawCenteredText("BONUS! EXTRA BALL", 558, 34, GOLD);
		DrawCenteredText(TextFormat("+%d", COMBO_POINTS), 600, 30, WHITE);
	}
}

// What the player can change, in the right panel under the highscores
static void DrawSettings(int fps_limit, float gravity, float bounce)
{
	int right = RIGHT_PANEL_X + 20;
	int y = 640;
	Color text = LIGHTGRAY;

	DrawText("SETTINGS", right, y, 20, WHITE);
	DrawText("F1  DEBUG", right, y + 32, 16, text);
	DrawText(TextFormat("F2  FPS LIMIT: %d", fps_limit), right, y + 56, 16, text);
	DrawText(TextFormat("F3/F4  GRAVITY: %.0f", gravity), right, y + 80, 16, text);
	DrawText(TextFormat("F5/F6  BOUNCE: %.1f", bounce), right, y + 104, 16, text);
	DrawText("S / DOWN  LAUNCH", right, y + 128, 16, text);
}

// Loads an image and cuts it into a circle, so the square corners are not seen.
// gold = true: the image is turned into gray tones (it keeps its light and shadows), so the
// gold tint used when drawing it gives gold whatever the colors of the original are
static Texture2D LoadBallTexture(const char* path, bool gold)
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
				Color& p = px[y * img.width + x];

				float dx = x + 0.5f - cx;
				float dy = y + 0.5f - cy;
				if (dx * dx + dy * dy > r * r)
					p.a = 0;

				if (gold)
				{
					// Gray level, a bit brighter: the gold tint multiplies it and would make it too dark
					float gray = (0.299f * p.r + 0.587f * p.g + 0.114f * p.b) * GOLD_BRIGHTNESS;
					p.r = p.g = p.b = (unsigned char)MIN(gray, 255.0f);
				}
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
	launch_charge = 0.0f;
	plunger_firing = false;
	gold_lost = false;

	plunger = NULL;
	lane_walls = NULL;
	lane_divider = NULL;

	time_left = GAME_TIME_SECONDS;
	score = 0;

	for (int i = 0; i < STAR_COUNT; ++i)
		stars[i] = NULL;

	lane_sensor = NULL;
	gold_lane_cooldown = 0.0f;
	next_ball_time = EXTRA_BALL_INTERVAL;
	last_rank = -1;

	ball_restitution = BALL_RESTITUTION;
	combo_pending = false;
	combo_flash = 0.0f;
	combo_fx = 0;
	launch_fx = 0;
	roll_fx = 0;
	roll_playing = false;
	roll_going_up = false;
	for (int i = 0; i < BOUNCE_FX_COUNT; ++i)
		bounce_fx[i] = 0;
	bounce_cooldown = 0.0f;
	left_flipper_fx = 0;
	right_flipper_fx = 0;
	for (int i = 0; i < BUMPER_FX_COUNT; ++i)
		bumper_fx[i] = 0;
	for (int i = 0; i < STAR_COUNT; ++i)
		star_lit[i] = false;

	background = Texture2D{ 0 };
	logo = Texture2D{ 0 };
	spawn_texture = Texture2D{ 0 };
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

	SetRandomSeed((unsigned int)time(NULL));   // a different random order every run

	highscores.Load();

	combo_fx = App->audio->LoadFx(COMBO_FX_PATH);
	if (combo_fx == 0)
		LOG("Cannot load %s", COMBO_FX_PATH);

	launch_fx = App->audio->LoadFx(LAUNCH_FX_PATH);
	if (launch_fx == 0)
		LOG("Cannot load %s", LAUNCH_FX_PATH);

	roll_fx = App->audio->LoadFx(ROLL_FX_PATH);
	if (roll_fx == 0)
		LOG("Cannot load %s", ROLL_FX_PATH);

	for (int i = 0; i < BOUNCE_FX_COUNT; ++i)
	{
		bounce_fx[i] = App->audio->LoadFx(bounce_fx_paths[i]);
		if (bounce_fx[i] == 0)
			LOG("Cannot load %s", bounce_fx_paths[i]);
	}

	left_flipper_fx = App->audio->LoadFx(LEFT_FLIPPER_FX_PATH);
	if (left_flipper_fx == 0)
		LOG("Cannot load %s", LEFT_FLIPPER_FX_PATH);

	right_flipper_fx = App->audio->LoadFx(RIGHT_FLIPPER_FX_PATH);
	if (right_flipper_fx == 0)
		LOG("Cannot load %s", RIGHT_FLIPPER_FX_PATH);

	for (int i = 0; i < BUMPER_FX_COUNT; ++i)
	{
		bumper_fx[i] = App->audio->LoadFx(bumper_fx_paths[i]);
		if (bumper_fx[i] == 0)
			LOG("Cannot load %s", bumper_fx_paths[i]);
	}

	restart_fx = App->audio->LoadFx(RESTART_FX_PATH);
	if (restart_fx == 0)
		LOG("Cannot load %s", RESTART_FX_PATH);

	game_over_fx = App->audio->LoadFx(GAME_OVER_FX_PATH);
	if (game_over_fx == 0)
		LOG("Cannot load %s", GAME_OVER_FX_PATH);

	background = LoadTexture(BACKGROUND_PATH);
	logo = LoadTexture(LOGO_PATH);
	if (background.id == 0) LOG("Cannot load %s", BACKGROUND_PATH);
	if (logo.id == 0) LOG("Cannot load %s", LOGO_PATH);
	spawn_texture = LoadTexture(SPAWN_PATH);
	if (spawn_texture.id == 0)
		LOG("Cannot load %s", SPAWN_PATH);
	else
		SetTextureFilter(spawn_texture, TEXTURE_FILTER_POINT);   // pixel art: no blur

	// The ball images
	ball_texture = LoadBallTexture(BALL_PATH, true);               // gold
	extra_ball_texture = LoadBallTexture(EXTRA_BALL_PATH, false);  // as it is

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

	// Launcher lane: its walls and the divider. The divider is seen from the lane (the chain goes down)
	points.clear();
	BuildLaunchLane(points);
	lane_walls = App->physics->CreateChain(0, 0, points.data(), (int)points.size(), false);

	points.clear();
	AddPoint(points, WALL_RIGHT, DIVIDER_TOP_Y);              // cap: closes the gap between both walls
	AddPoint(points, LAUNCH_DIVIDER_X, DIVIDER_TOP_Y);
	AddPoint(points, LAUNCH_DIVIDER_X, LAUNCH_LANE_FLOOR_Y);  // the divider, seen from the lane
	lane_divider = App->physics->CreateChain(0, 0, points.data(), (int)points.size(), false);

	// The plunger: a box that we move (kinematic). The ball rests on it
	plunger = App->physics->CreateRectangle(LAUNCH_LANE_X, PLUNGER_REST_Y, PLUNGER_HALF_W, PLUNGER_HALF_H);
	plunger->SetDynamic(false);

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

	// The gold ball waits on the plunger, in the launcher lane
	gold_ball = App->physics->CreateCircle(LAUNCH_LANE_X, PLUNGER_REST_Y - PLUNGER_HALF_H - BALL_RADIUS, BALL_RADIUS, ball_restitution, BALL_FRICTION, true);
	gold_ball->listener = this;     // so OnCollision knows when it bounces

	launch_charge = 0.0f;
	plunger_firing = false;
	gold_lost = false;
	last_rank = -1;

	// The combo starts again
	combo_pending = false;
	combo_flash = 0.0f;
	for (int i = 0; i < STAR_COUNT; ++i)
		star_lit[i] = false;

	gold_lane_cooldown = 0.0f;
	next_ball_time = EXTRA_BALL_INTERVAL;
	time_left = GAME_TIME_SECONDS;
	score = 0;
	state = STATE_WAITING;

	// Cut the game over melody if it is still playing
	App->audio->StopFx(game_over_fx);

	// Sound of a new game (it is cut when the player shoots the ball)
	App->audio->PlayFx(restart_fx);
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

int ModuleGame::StarIndex(PhysBody* body) const
{
	if (body == NULL)
		return -1;

	for (int i = 0; i < STAR_COUNT; ++i)
	{
		if (stars[i] == body)
			return i;
	}

	return -1;
}

bool ModuleGame::IsStar(PhysBody* body) const
{
	return StarIndex(body) >= 0;
}

void ModuleGame::EndGame()
{
	state = STATE_GAME_OVER;

	// Time played: how long the game lasted (not the time left)
	int seconds = (int)(GAME_TIME_SECONDS - time_left);
	if (seconds < 0) seconds = 0;
	if (seconds > (int)GAME_TIME_SECONDS) seconds = (int)GAME_TIME_SECONDS;

	last_rank = highscores.Add(score, seconds);

	// Game over melody (it is cut when a new game starts)
	App->audio->PlayFx(game_over_fx);
}

void ModuleGame::SpawnExtraBall()
{
	// It appears at the mark, with no speed and a small random horizontal offset
	int x = SPAWN_X + GetRandomValue(-SPAWN_RANDOM_X, SPAWN_RANDOM_X);

	ExtraBall extra;
	extra.body = App->physics->CreateCircle(x, SPAWN_Y, BALL_RADIUS, ball_restitution, BALL_FRICTION, true);
	extra.body->listener = this;
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

void ModuleGame::UpdatePlunger(float dt)
{
	bool pulling = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);

	int x, y;
	plunger->GetPosition(x, y);

	// Going up fast: it stops when it gets back to its resting position
	if (plunger_firing)
	{
		if (y <= PLUNGER_REST_Y)
		{
			plunger_firing = false;
			plunger->SetLinearVelocity(0.0f, 0.0f);
		}
		return;
	}

	if (pulling)
	{
		launch_charge = MIN(launch_charge + dt / PLUNGER_CHARGE_TIME, 1.0f);
	}
	else if (launch_charge > 0.0f)
	{
		// Released: the plunger shoots up and pushes the ball. The more it was pulled, the faster it goes
		float speed = PLUNGER_SPEED_MIN + (PLUNGER_SPEED_MAX - PLUNGER_SPEED_MIN) * launch_charge;
		plunger->SetLinearVelocity(0.0f, -speed);
		plunger_firing = true;
		launch_charge = 0.0f;

		// The restart sound is cut when the ball is shot
		App->audio->StopFx(restart_fx);

		// Sound of the shot: always, every time the plunger is released
		App->audio->PlayFx(launch_fx);

		// Sound of the ball going up: only if the ball is resting on the plunger
		bool ball_on_plunger = false;
		if (gold_ball != NULL)
		{
			int bx, by;
			gold_ball->GetPosition(bx, by);
			float rest_y = (float)y - PLUNGER_HALF_H - BALL_RADIUS;   // where the ball is when it rests on the plunger
			ball_on_plunger = (bx >= WALL_RIGHT && fabsf((float)by - rest_y) <= 10.0f);
		}

		if (ball_on_plunger)
		{
			App->audio->PlayFx(roll_fx);
			roll_playing = true;
			roll_going_up = false;
		}

		// The first shot starts the game and the time
		if (state == STATE_WAITING)
			state = STATE_PLAYING;

		return;
	}

	// The plunger follows the pull smoothly
	float target_y = PLUNGER_REST_Y + PLUNGER_PULL * launch_charge;
	plunger->SetLinearVelocity(0.0f, (target_y - y) * 10.0f);
}

update_status ModuleGame::Update()
{
	// Art: the background of the board and the logo (if a file is missing, nothing is drawn)
	// The lane color first: it is only seen where the background image does not reach
	DrawRectangle(BOARD_WIDTH, 0, LAUNCH_STRIP_WIDTH, BOARD_HEIGHT, LANE_COLOR);

	// The image is scaled to the height of the board and placed at the left, so it is never stretched.
	// If it is wider than the playfield (it includes the lane) it also covers the lane
	if (background.id != 0)
	{
		float scale = (float)BOARD_HEIGHT / background.height;
		Rectangle source = { 0.0f, 0.0f, (float)background.width, (float)background.height };
		Rectangle dest = { 0.0f, 0.0f, background.width * scale, (float)BOARD_HEIGHT };
		DrawTexturePro(background, source, dest, Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
	}

	if (logo.id != 0)
		DrawTextureFit(logo, Rectangle{ (float)(LEFT_PANEL_X + 10), 10.0f, (float)(PANEL_WIDTH - 20), (float)(BOARD_HEIGHT / 2 - 20) });

	// Walls and flippers filled with a color
#if DRAW_WALLS
	// first the white border of all the walls (thicker), then the purple on top of all of them
	walls->DrawFilled(WALL_OUTLINE_COLOR, WALL_THICKNESS + 2.0f * WALL_OUTLINE);
	half_moon->DrawFilled(WALL_OUTLINE_COLOR, WALL_THICKNESS + 2.0f * WALL_OUTLINE);
	lane_walls->DrawFilled(WALL_OUTLINE_COLOR, WALL_THICKNESS + 2.0f * WALL_OUTLINE);
	lane_divider->DrawFilled(WALL_OUTLINE_COLOR, WALL_THICKNESS + 2.0f * WALL_OUTLINE);

	walls->DrawFilled(WALL_COLOR, WALL_THICKNESS);
	half_moon->DrawFilled(WALL_COLOR, WALL_THICKNESS);
	lane_walls->DrawFilled(WALL_COLOR, WALL_THICKNESS);
	lane_divider->DrawFilled(WALL_COLOR, WALL_THICKNESS);
#endif
	left_flipper->DrawFilled(FLIPPER_COLOR);
	right_flipper->DrawFilled(FLIPPER_COLOR);

	// The plunger
	{
		int px, py;
		plunger->GetPosition(px, py);
		DrawPlunger(px, py, launch_charge);
	}

	DrawPanels(score, time_left, next_ball_time, highscores, last_rank);
	DrawSettings(App->window->GetFPSLimit(), App->physics->GetGravity(), ball_restitution);

	// Flippers: LEFT / A and RIGHT / D (disabled during game over)
	bool flippers_on = (state != STATE_GAME_OVER);
	App->physics->SetFlipperMotor(left_flipper, flippers_on && (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)));
	App->physics->SetFlipperMotor(right_flipper, flippers_on && (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)));

	// Sound of the flippers: only at the moment the key is pressed (not while it is held)
	if (flippers_on)
	{
		if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
			App->audio->PlayFx(left_flipper_fx);

		if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
			App->audio->PlayFx(right_flipper_fx);
	}

	// R = restart (debug)
	if (IsKeyPressed(KEY_R))
		StartNewGame();

	// Things the player can change
	// F2: target FPS (the physics always run in fixed steps, so the game speed does not change)
	if (IsKeyPressed(KEY_F2))
		App->window->CycleFPSLimit();

	// F3 / F4: gravity
	float g = App->physics->GetGravity();
	if (IsKeyPressed(KEY_F3)) g = MAX(g - 1.0f, 1.0f);
	if (IsKeyPressed(KEY_F4)) g = MIN(g + 1.0f, 30.0f);
	if (g != App->physics->GetGravity())
		App->physics->SetGravity(g);

	// F5 / F6: bounce of the balls (restitution, from 0 to 1)
	float bounce = ball_restitution;
	if (IsKeyPressed(KEY_F5)) bounce = MAX(bounce - 0.1f, 0.0f);
	if (IsKeyPressed(KEY_F6)) bounce = MIN(bounce + 0.1f, 1.0f);
	bounce = roundf(bounce * 10.0f) / 10.0f;
	if (bounce != ball_restitution)
	{
		ball_restitution = bounce;

		// It also changes the balls that are already in the board
		if (gold_ball != NULL)
			gold_ball->SetRestitution(ball_restitution);
		for (ExtraBall& e : extra_balls)
			e.body->SetRestitution(ball_restitution);
	}

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
	bounce_cooldown = MAX(bounce_cooldown - dt, 0.0f);
	for (ExtraBall& e : extra_balls)
		e.lane_cooldown = MAX(e.lane_cooldown - dt, 0.0f);

	// Marks (placeholders): where the extra balls appear and the scoring line of the half-moon
	if (spawn_texture.id != 0)
	{
		DrawTextureFit(spawn_texture, Rectangle{
			SPAWN_X - SPAWN_IMAGE_SIZE / 2.0f,
			SPAWN_Y - SPAWN_IMAGE_SIZE / 2.0f,
			(float)SPAWN_IMAGE_SIZE,
			(float)SPAWN_IMAGE_SIZE });
	}
	else
	{
		// The image was not found: the old placeholder mark
		DrawCircle(SPAWN_X, SPAWN_Y, (float)SPAWN_MARK_RADIUS, Fade(SKYBLUE, 0.3f));
		DrawCircleLines(SPAWN_X, SPAWN_Y, (float)SPAWN_MARK_RADIUS, SKYBLUE);
	}
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
			DrawCircle(x, y, (float)star_defs[i].radius, star_lit[i] ? YELLOW : ORANGE);

			// The letter of the combo word
			const char letter[2] = { COMBO_WORD[i], '\0' };
			int size = (int)(star_defs[i].radius * 1.2f);
			DrawText(letter, x - MeasureText(letter, size) / 2, y - size / 2, size, WHITE);
		}
	}

	DrawCombo(star_lit, combo_flash);

	// The plunger works while the game is on (also to shoot again a ball that fell back to the lane)
	if (state != STATE_GAME_OVER)
		UpdatePlunger(dt);

	// Sound of the ball going up: it is cut if the ball falls back to the lane (weak shot)
	if (roll_playing)
	{
		if (gold_ball == NULL || state == STATE_GAME_OVER)
		{
			App->audio->StopFx(roll_fx);
			roll_playing = false;
		}
		else
		{
			int bx, by;
			gold_ball->GetPosition(bx, by);
			float vy = gold_ball->body->GetLinearVelocity().y;   // m/s: negative = going up

			if (bx < WALL_RIGHT)
			{
				// The ball reached the field: the sound ends by itself
				roll_playing = false;
			}
			else if (vy < -0.5f)
			{
				roll_going_up = true;
			}
			else if (roll_going_up && vy > 0.0f && by > WALL_TOP + LAUNCH_LANE_ARC)
			{
				// It stopped going up and falls back: cut the sound
				App->audio->StopFx(roll_fx);
				roll_playing = false;
			}
		}
	}

	switch (state)
	{
	case STATE_WAITING:
	{
		// The first shot is done with the plunger (UpdatePlunger starts the game)
		DrawCenteredTextOutline("HOLD S / DOWN, RELEASE TO LAUNCH", 610, 20, Color{ 150, 60, 255, 255 }, WHITE, 2);
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

		// Combo: all the letters were hit (the body is created here, never inside the collision callback)
		if (combo_pending)
		{
			combo_pending = false;
			score += COMBO_POINTS;
			App->audio->PlayFx(combo_fx);
			SpawnExtraBall();
			combo_flash = COMBO_SHOW_TIME;
		}

		// After the message the letters go out and the combo can be done again
		if (combo_flash > 0.0f)
		{
			combo_flash -= GetFrameTime();
			if (combo_flash <= 0.0f)
			{
				for (int i = 0; i < STAR_COUNT; ++i)
					star_lit[i] = false;
			}
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
	if (spawn_texture.id != 0) UnloadTexture(spawn_texture);
	background = Texture2D{ 0 };
	logo = Texture2D{ 0 };
	ball_texture = Texture2D{ 0 };
	extra_ball_texture = Texture2D{ 0 };
	spawn_texture = Texture2D{ 0 };

	DestroyStars();
	DestroyExtraBalls();
	App->physics->DestroyBody(lane_sensor);
	App->physics->DestroyBody(plunger);
	App->physics->DestroyBody(lane_walls);
	App->physics->DestroyBody(lane_divider);
	App->physics->DestroyBody(gold_ball);
	App->physics->DestroyBody(walls);
	App->physics->DestroyBody(half_moon);
	App->physics->DestroyBody(left_flipper);
	App->physics->DestroyBody(right_flipper);
	App->physics->DestroyBody(sensor);

	gold_ball = walls = half_moon = left_flipper = right_flipper = sensor = lane_sensor = plunger = lane_walls = lane_divider = NULL;

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
	int star = StarIndex(bodyA);
	if (state == STATE_PLAYING && star >= 0 && IsBall(bodyB))
	{
		score += STAR_POINTS;

		// Sound of the letter: one of the 3 at random. It does not cut the ones already playing
		App->audio->PlayFxMix(bumper_fx[GetRandomValue(0, BUMPER_FX_COUNT - 1)]);

		// Combo: a new letter of the word. During the message nothing is added
		if (combo_flash <= 0.0f && !star_lit[star])
		{
			star_lit[star] = true;

			bool all_lit = true;
			for (int i = 0; i < STAR_COUNT; ++i)
			{
				if (!star_lit[i])
					all_lit = false;
			}

			if (all_lit)
				combo_pending = true;
		}
	}

	// a ball bounces on something solid: one of the 2 bounce sounds at random
	// the sensors do not count (the ball goes through them) and the letters have their own sound
	if (state == STATE_PLAYING && IsBall(bodyA) && bounce_cooldown <= 0.0f
		&& bodyB != sensor && bodyB != lane_sensor && StarIndex(bodyB) < 0)
	{
		float speed = bodyA->body->GetLinearVelocity().Length();   // m/s

		if (speed >= BOUNCE_MIN_SPEED)
		{
			App->audio->PlayFxMix(bounce_fx[GetRandomValue(0, BOUNCE_FX_COUNT - 1)]);
			bounce_cooldown = BOUNCE_COOLDOWN;
		}
	}
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

	DrawTexturePro(texture, src, dst, origin, 0.0f, is_gold ? GOLD : WHITE);
}