#pragma once

#include "Globals.h"
#include "Module.h"
#include "HighScores.h"

#include <vector>

class PhysBody;

#define STAR_COUNT 5

enum GameState
{
	STATE_WAITING,    
	STATE_PLAYING,    
	STATE_GAME_OVER   // bola dorada perdida
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


	void StartNewGame();

	// The game is over: saves the result in the highscores
	void EndGame();

	// Plunger of the launcher lane: hold S / DOWN to pull it back, release to shoot
	void UpdatePlunger(float dt);

	void CreateStars();
	void DestroyStars();
	bool IsStar(PhysBody* body) const;
	int StarIndex(PhysBody* body) const;        
	void DrawBall(PhysBody* body, float radius);

	void SpawnExtraBall();
	void DestroyExtraBalls();
	bool IsBall(PhysBody* body) const;          
	float* LaneCooldownOf(PhysBody* body);      // NULL si no es bolaa

private:

	GameState state;

	PhysBody* gold_ball;
	float launch_charge;    
	bool gold_lost;         

	float time_left;        
	int score;

	PhysBody* walls;        // outer walls + funnels down to the flippers
	PhysBody* half_moon;    // the wall of the half-moon lane
	PhysBody* left_flipper;
	PhysBody* right_flipper;
	PhysBody* sensor;       // below the board: detects when the ball is lost

	PhysBody* stars[STAR_COUNT];

	PhysBody* lane_sensor;  // the line in the middle of the half-moon

	// Launcher lane, to the right of the playfield
	PhysBody* lane_walls;   // floor, outer wall, curve at the top and ceiling
	PhysBody* lane_divider; // wall between the lane and the playfield (seen from the lane)
	PhysBody* plunger;      // kinematic box: the ball rests on it
	bool plunger_firing;    // going up after being released

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
	int last_rank;                             

	Texture2D background;
	Texture2D logo;
	Texture2D ball_texture;                   
	Texture2D extra_ball_texture;           
	Texture2D spawn_texture;

	float ball_restitution;              

	// Combo: every star is a letter of a word. Hit all the letters and you get a bonus
	bool star_lit[STAR_COUNT];                 // letters already hit
	bool combo_pending;                        // raised in OnCollision, handled in Update
	float combo_flash;                         // seconds left showing the combo message
	unsigned int combo_fx;                    
	unsigned int launch_fx;                 
	unsigned int roll_fx;                  
	unsigned int restart_fx;           
	unsigned int game_over_fx;                 
	bool roll_playing;                         
	bool roll_going_up;                      
	unsigned int bumper_fx[3];               
	unsigned int bounce_fx[2];                 
	float bounce_cooldown;                    
	unsigned int left_flipper_fx;             
	unsigned int right_flipper_fx;             
};