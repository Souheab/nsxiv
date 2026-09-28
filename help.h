#ifndef HELP_H
#define HELP_H

#include "nsxiv.h"

typedef struct {
	KeySym sym;
	unsigned int mask;
	char key[128];
	char aliases[256];
	char *description;
} help_row_t;

typedef struct {
	bool active;
	appmode_t mode;
	char query[256];
	help_row_t *rows;
	int *matches;
	int count, total, first, page;
} help_t;

void help_open(help_t*, Display*, const keymap_t*, int, unsigned int, appmode_t);
void help_free(help_t*);
void help_filter(help_t*);
void help_scroll(help_t*, int);
void help_keypress(help_t*, KeySym, unsigned int, const char*, int);

#endif
