#pragma once

#include "Globals.h"
#include "Module.h"
#include "HighScores.h"

#include <vector>

class PhysBody;

#define STAR_COUNT 5

enum GameState
{
	STATE_WAITING,    // the gold ball moves by itself, waiting for SPACE
	STATE_PLAYING,    // the time is running
	STATE_GAME_OVER   // the gold ball was lost or the time ran out
};

class ModuleGame : public Module
{
public:
	ModuleGame(Application* app, bool start_enabled = true);
	~ModuleGame();

	bool Start();
	update_status Update();
	bool CleanUp();

	void OnCollision(PhysBody* bodyA, PhysBody* bodyB) override;

private:

	// Puts the gold ball on the "sun" line and resets time and score
	void StartNewGame();

	// The game is over: saves the result in the highscores
	void EndGame();

	void CreateStars();
	void DestroyStars();
	bool IsStar(PhysBody* body) const;
	void DrawBall(PhysBody* body, float radius);

	void SpawnExtraBall();
	void DestroyExtraBalls();
	bool IsBall(PhysBody* body) const;          // the gold ball or an extra ball
	float* LaneCooldownOf(PhysBody* body);      // NULL if it is not a ball

private:

	GameState state;

	PhysBody* gold_ball;
	float gold_dir;         // +1 right, -1 left (only while waiting)
	bool gold_lost;         // raised in OnCollision, handled in Update

	float time_left;        // seconds
	int score;

	PhysBody* walls;        // outer walls + funnels down to the flippers
	PhysBody* half_moon;    // the wall of the half-moon lane
	PhysBody* left_flipper;
	PhysBody* right_flipper;
	PhysBody* sensor;       // below the board: detects when the ball is lost

	PhysBody* stars[STAR_COUNT];

	PhysBody* lane_sensor;  // the line in the middle of the half-moon

	// Balls that appear every EXTRA_BALL_INTERVAL seconds
	struct ExtraBall
	{
		PhysBody* body;
		float lane_cooldown;   // time until it can score on the line again
	};
	std::vector<ExtraBall> extra_balls;
	std::vector<PhysBody*> balls_to_remove;   // extra balls that touched the bottom sensor
	float gold_lane_cooldown;
	float next_ball_time;                      // seconds until the next extra ball

	Highscores highscores;
	int last_rank;                             // position of the last game in the list (-1 = it did not enter)

	Texture2D background;
	Texture2D logo;
	Texture2D ball_texture;
};