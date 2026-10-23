#include "Highscores.h"

#include <fstream>

// Created in the working directory of the game (next to the Assets folder when running from Visual Studio)
#define HIGHSCORES_FILE "highscores.txt"

Highscores::Highscores() : count(0)
{
	for (int i = 0; i < MAX_HIGHSCORES; ++i)
	{
		entries[i].score = 0;
		entries[i].seconds = 0;
	}
}

// More points is better. With the same points, less time is better
static bool IsBetter(const ScoreEntry& a, const ScoreEntry& b)
{
	return a.score > b.score || (a.score == b.score && a.seconds < b.seconds);
}

int Highscores::Insert(int score, int seconds)
{
	// A game with no points is not worth saving
	if (score <= 0 || seconds < 0)
		return -1;

	ScoreEntry e;
	e.score = score;
	e.seconds = seconds;

	// First position that the new result beats. Equal results stay in front of it
	int pos = 0;
	while (pos < count && !IsBetter(e, entries[pos]))
		++pos;

	if (pos >= MAX_HIGHSCORES)
		return -1;

	// Make room, the last one falls out of the list when it is full
	int last = (count < MAX_HIGHSCORES) ? count : MAX_HIGHSCORES - 1;
	for (int i = last; i > pos; --i)
		entries[i] = entries[i - 1];

	entries[pos] = e;

	if (count < MAX_HIGHSCORES)
		++count;

	return pos;
}

void Highscores::Load()
{
	count = 0;

	std::ifstream file(HIGHSCORES_FILE);

	// One result per line: "score seconds"
	int score, seconds;
	while (file >> score >> seconds)
		Insert(score, seconds);
}

void Highscores::Save() const
{
	std::ofstream file(HIGHSCORES_FILE, std::ios::trunc);

	for (int i = 0; i < count; ++i)
		file << entries[i].score << ' ' << entries[i].seconds << '\n';
}

int Highscores::Add(int score, int seconds)
{
	int pos = Insert(score, seconds);

	if (pos >= 0)
		Save();

	return pos;
}

int Highscores::Count() const
{
	return count;
}

const ScoreEntry& Highscores::Get(int index) const
{
	return entries[index];
}