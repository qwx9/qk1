#pragma once

// the status bar is only redrawn if something has changed, but if anything
// does, the entire thing will be redrawn for the next vid.numpages frames.

enum {
	SBAR_HEIGHT = 24,
};

extern int sb_lines; // scan lines to draw

void Sbar_Init(void);

// called every frame by screen
void Sbar_Draw(void);

// called each frame after the level has been completed
void Sbar_IntermissionOverlay(void);

void Sbar_FinaleOverlay(void);
