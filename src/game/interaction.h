// src/game/interaction.h
#pragma once
#include <stdbool.h>

typedef struct Game Game;

// What’s in front of the player?
typedef struct InteractProbe
{
    int tx;
    int ty;
    int id;     // 0 none, otherwise your interact id
    bool valid;
} InteractProbe;

InteractProbe Interaction_ProbeFront(const Game* g);

// Called when E is pressed.
// Returns true if something was interacted with.
bool Interaction_Try(Game* g);
