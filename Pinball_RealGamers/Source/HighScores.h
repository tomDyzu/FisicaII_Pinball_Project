#pragma once

#define MAX_HIGHSCORES 6

struct ScoreEntry
{
	int score;
	int seconds;   // time played in that game
};

// Top list of scores, saved in a text file so it survives when the game is closed
class Highscores
{
public:
	Highscores();

	// Reads the file. If it does not exist yet the list is empty
	void Load();

	// Adds a result if it is good enough for the list and saves the file.
	// Returns its position (0 = first place) or -1 if it did not enter
	int Add(int score, int seconds);

	int Count() const;
	const ScoreEntry& Get(int index) const;

private:
	int Insert(int score, int seconds);   // without saving
	void Save() const;

	ScoreEntry entries[MAX_HIGHSCORES];
	int count;
};