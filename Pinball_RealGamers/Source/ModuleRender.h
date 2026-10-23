#pragma once
#include "Module.h"
#include "Globals.h"

#include <limits.h>

class ModuleRender : public Module
{
public:
	ModuleRender(Application* app, bool start_enabled = true);
	~ModuleRender();

	bool Init();
	update_status PreUpdate();
	update_status Update();
	update_status PostUpdate();
	bool CleanUp();

	void SetBackgroundColor(Color color);
	bool Draw(Texture2D texture, int x, int y, const Rectangle* section = NULL, double angle = 0, int pivot_x = 0, int pivot_y = 0) const;
	bool DrawText(const char* text, int x, int y, Font font, int spacing, Color tint) const;

	// Mouse position in BOARD coordinates (the camera moves the board to the center of the window)
	Vector2 GetMouseWorld() const;

public:

	Color background;   // color of the board
	Camera2D camera;    // only moves the board to the center: offset = PANEL_WIDTH
};