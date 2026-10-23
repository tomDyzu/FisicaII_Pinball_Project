#include "Globals.h"
#include "Application.h"
#include "ModuleWindow.h"

ModuleWindow::ModuleWindow(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	width = SCREEN_WIDTH;
	height = SCREEN_HEIGHT;

	for (int i = 0; i < WINDOW_EVENT_COUNT; ++i)
		windowEvents[i] = false;
}

// Destructor
ModuleWindow::~ModuleWindow()
{
}

// Called before render is available
bool ModuleWindow::Init()
{
	LOG("Init window");

	unsigned int flags = 0;

	width = SCREEN_WIDTH;
	height = SCREEN_HEIGHT;

	if (WIN_FULLSCREEN == true) flags |= FLAG_FULLSCREEN_MODE;
	if (WIN_BORDERLESS == true) flags |= FLAG_WINDOW_UNDECORATED;
	if (WIN_RESIZABLE == true) flags |= FLAG_WINDOW_RESIZABLE;
	if (VSYNC == true) flags |= FLAG_VSYNC_HINT;

	LOG("Init raylib window");

	SetConfigFlags(flags);
	InitWindow(width, height, TITLE);

	if (!IsWindowReady())
	{
		LOG("Cannot create the window");
		return false;
	}

	// By default ESC closes the window in raylib. Uncomment to use ESC for pause/menu:
	// SetExitKey(KEY_NULL);

	return true;
}

// Called each loop iteration
update_status ModuleWindow::PreUpdate()
{
	// Events only live for one frame
	for (int i = 0; i < WINDOW_EVENT_COUNT; ++i)
		windowEvents[i] = false;

	if (IsWindowMinimized()) windowEvents[WINDOW_EVENT_HIDE] = true;
	if (IsWindowResized()) windowEvents[WINDOW_EVENT_RESIZE] = true;

	// The window is the first module: stopping here happens BEFORE
	// the renderer calls BeginDrawing(), so no frame is left half done
	if (WindowShouldClose())
	{
		windowEvents[WINDOW_EVENT_QUIT] = true;
		return UPDATE_STOP;
	}

	return UPDATE_CONTINUE;
}

update_status ModuleWindow::Update()
{
	return UPDATE_CONTINUE;
}

update_status ModuleWindow::PostUpdate()
{
	return UPDATE_CONTINUE;
}

// Called before quitting
bool ModuleWindow::CleanUp()
{
	LOG("Close window");

	if (IsWindowReady())
		CloseWindow();

	return true;
}

void ModuleWindow::SetTitle(const char* title)
{
	SetWindowTitle(title);
}

uint ModuleWindow::GetWidth() const
{
	return width;
}

uint ModuleWindow::GetHeight() const
{
	return height;
}

bool ModuleWindow::GetWindowEvent(WindowEvent ev) const
{
	return windowEvents[ev];
}