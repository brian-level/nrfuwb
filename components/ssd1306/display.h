
#pragma once

int DisplayWidth(void);
int DisplayHeight(void);
int DisplaySetFont(int height);
int DisplayCharWidth(const char ch);
int DisplayTextWidth(const char *text);
int DisplayText(int x, int y, const char *text);
int DisplayInit(void);

