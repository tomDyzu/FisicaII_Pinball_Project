#include "Globals.h"
#include "Application.h"
#include "ModuleWindow.h"
#include "ModuleRender.h"
#include <math.h>

// Color of the area around the board (side panels)
static const Color panel_color = { 30, 30, 40, 255 };

ModuleRender::ModuleRender(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	background = RAYWHITE;
	camera.offset = { (float)PANEL_WIDTH, 0.0f };
	camera.target = { 0.0f, 0.0f };
	camera.rotation = 0.0f;
	camera.zoom = 1.0f;
}

// Destructor
ModuleRender::~ModuleRender()
{
}

// Called before render is available
bool ModuleRender::Init()
{
	LOG("Creating Renderer context");
	return true;
}

// PreUpdate: the frame starts here, BEFORE any module draws
update_status ModuleRender::PreUpdate()
{
	BeginDrawing();

	// Screen space: the panels and the background of the board
	ClearBackground(panel_color);
	DrawRectangle(PANEL_WIDTH, 0, BOARD_WIDTH, BOARD_HEIGHT, background);

	// From here until PostUpdate everything is drawn in BOARD coordinates
	// (0,0 is the top left corner of the board, the panels have negative X or X > BOARD_WIDTH)
	BeginMode2D(camera);

	return UPDATE_CONTINUE;
}

// Nothing to do here: the other modules draw in their own Update()
update_status ModuleRender::Update()
{
	return UPDATE_CONTINUE;
}

// PostUpdate: the frame ends here, AFTER every module has drawn
update_status ModuleRender::PostUpdate()
{
	EndMode2D();

	// Screen space again
	DrawFPS(10, 10);

	EndDrawing();

	return UPDATE_CONTINUE;
}

// Called before quitting
bool ModuleRender::CleanUp()
{
	return true;
}

void ModuleRender::SetBackgroundColor(Color color)
{
	background = color;
}

// Draw to screen
bool ModuleRender::Draw(Texture2D texture, int x, int y, const Rectangle* section, double angle, int pivot_x, int pivot_y) const
{
	float scale = 1.0f;
	Vector2 position = { (float)x, (float)y };
	Rectangle rect = { 0.f, 0.f, (float)texture.width, (float)texture.height };

	if (section != NULL) rect = *section;

	// The camera is applied by BeginMode2D, no need to add it here
	position.x = (float)(x - pivot_x) * scale;
	position.y = (float)(y - pivot_y) * scale;

	rect.width *= scale;
	rect.height *= scale;

	// TODO: 'angle' is still ignored. Use DrawTexturePro if you need rotation
	DrawTextureRec(texture, rect, position, WHITE);

	return true;
}

bool ModuleRender::DrawText(const char* text, int x, int y, Font font, int spacing, Color tint) const
{
	Vector2 position = { (float)x, (float)y };

	DrawTextEx(font, text, position, (float)font.baseSize, (float)spacing, tint);

	return true;
}

Vector2 ModuleRender::GetMouseWorld() const
{
	return GetScreenToWorld2D(GetMousePosition(), camera);
}