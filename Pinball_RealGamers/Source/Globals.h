#pragma once

#include "raylib.h"

#include <stdio.h>
#include <stdint.h>

// ##__VA_ARGS__ lets you call LOG("text") without extra arguments
#define LOG(format, ...) log(__FILE__, __LINE__, format, ##__VA_ARGS__)

void log(const char file[], int line, const char* format, ...);

#define CAP(n) ((n <= 0.0f) ? n=0.0f : (n >= 1.0f) ? n=1.0f : n=n)

#define DEGTORAD 0.0174532925199432957f
#define RADTODEG 57.295779513082320876f
#define IN_RANGE( value, min, max ) ( ((value) >= (min) && (value) <= (max)) ? 1 : 0 )
#define MIN( a, b ) ( ((a) < (b)) ? (a) : (b) )
#define MAX( a, b ) ( ((a) > (b)) ? (a) : (b) )
#define TO_BOOL( a )  ( (a != 0) ? true : false )

typedef unsigned int uint;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef unsigned char uchar;

enum update_status
{
	UPDATE_CONTINUE = 1,
	UPDATE_STOP,
	UPDATE_ERROR
};

// Configuration -----------

// The board (where the physics live) is always BOARD_WIDTH x BOARD_HEIGHT pixels.
// The window is wider: a side panel on each side (logo + score/time on the left, highscores on the right)
#define BOARD_WIDTH			  600
#define BOARD_HEIGHT		  900
#define PANEL_WIDTH			  250


#define LAUNCH_STRIP_WIDTH	  30
#define SCREEN_WIDTH		  (BOARD_WIDTH + LAUNCH_STRIP_WIDTH + 2 * PANEL_WIDTH)
#define SCREEN_HEIGHT		  BOARD_HEIGHT

// Panels in BOARD coordinates (the camera moves the board to the center of the window),
// so they are outside of the 0..BOARD_WIDTH range
#define LEFT_PANEL_X		  (-PANEL_WIDTH)
#define RIGHT_PANEL_X		  (BOARD_WIDTH + LAUNCH_STRIP_WIDTH)

#define SCREEN_SIZE				1
#define WIN_FULLSCREEN		false
#define WIN_RESIZABLE		false
#define WIN_BORDERLESS		false
#define WIN_FULLSCREEN_DESKTOP false
#define VSYNC				false
#define TITLE "Meteor Strike"