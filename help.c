/* Searchable reference for the compiled keyboard mappings. */
#include "help.h"
#include "commands.h"

#include <X11/keysym.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *direction(arg_t arg)
{
	switch (arg) {
		case DIR_LEFT: return "left";
		case DIR_RIGHT: return "right";
		case DIR_UP: return "up";
		case DIR_DOWN: return "down";
		case DIR_LEFT | DIR_UP: return "top left";
		case DIR_RIGHT | DIR_UP: return "top right";
		case DIR_LEFT | DIR_DOWN: return "bottom left";
		case DIR_RIGHT | DIR_DOWN: return "bottom right";
		default: return "configured direction";
	}
}

static void describe(const keymap_t *binding, char *buf, size_t size)
{
	static const struct {
		cmd_f func;
		const char *text;
	} names[] = {
		{ cg_help, "Search keyboard shortcuts" },
		{ cg_quit, "Quit nsxiv" },
		{ cg_pick_quit, "Quit and print current filename if no images are marked (-o)" },
		{ cg_switch_mode, "Switch image / thumbnail mode" },
		{ cg_toggle_fullscreen, "Toggle fullscreen" },
		{ cg_toggle_bar, "Toggle status bar" },
		{ cg_prefix_external, "Send next key to external key-handler" },
		{ cg_first, "Go to first image" },
		{ cg_n_or_last, "Go to last image, or image number given by count" },
		{ cg_reload_image, "Reload current image" },
		{ cg_remove_image, "Remove current image from file list" },
		{ cg_toggle_image_mark, "Toggle current image mark" },
		{ cg_mark_range, "Mark / unmark range from last mark" },
		{ cg_reverse_marks, "Reverse all image marks" },
		{ cg_unmark_all, "Unmark all images" },
		{ ct_reload_all, "Reload all thumbnails" },
		{ ci_alternate, "Go to alternate previously viewed image" },
		{ ci_toggle_animation, "Toggle animation playback" },
		{ ci_scroll_to_center, "Scroll to image center" },
		{ ci_toggle_antialias, "Toggle antialiasing" },
		{ ci_toggle_alpha, "Toggle alpha transparency background" },
		{ ci_slideshow, "Toggle slideshow, or set delay in seconds using count" },
		{ ci_cursor_navigate, "Navigate image according to pointer position" },
		{ ct_select, "Select thumbnail under pointer" },
		{ ct_drag_mark_image, "Drag to mark thumbnails" }
	};
	cmd_f func = binding->cmd.func;
	arg_t arg = binding->arg;
	unsigned int i;
	const char *text;

	for (i = 0; i < ARRLEN(names); i++) {
		if (names[i].func == func) {
			snprintf(buf, size, "%s", names[i].text);
			return;
		}
	}
	if (func == cg_zoom) {
		snprintf(buf, size, "Zoom %s", arg > 0 ? "in" : "out");
	} else if (func == ci_zoom_relative) {
		snprintf(buf, size, "Zoom %s by a factor of %.2f", arg > 0 ? "in" : "out",
		         1.0 + ABS((double)arg) / 100.0);
	} else if (func == ci_set_zoom) {
		snprintf(buf, size, "Set zoom to %d%%, or percentage given by count", arg);
	} else if (func == ci_navigate || func == ci_navigate_frame || func == cg_navigate_marked) {
		text = func == ci_navigate_frame ? "frame(s)" :
		       func == cg_navigate_marked ? "marked image(s)" : "image(s)";
		snprintf(buf, size, "Go %s %ld %s (multiplied by count)",
		         arg < 0 ? "back" : "forward", arg < 0 ? -(long)arg : (long)arg, text);
	} else if (func == cg_scroll_screen || func == ci_scroll || func == ct_scroll ||
	           func == ct_move_sel || func == ci_scroll_to_edge) {
		text = func == ct_move_sel ? "Move thumbnail selection" :
		       func == ci_scroll_to_edge ? "Scroll to edge" :
		       func == cg_scroll_screen ? "Scroll one screen" : "Scroll";
		snprintf(buf, size, "%s %s", text, direction(arg));
	} else if (func == cg_change_gamma || func == cg_change_brightness || func == cg_change_contrast) {
		text = func == cg_change_gamma ? "gamma" :
		       func == cg_change_brightness ? "brightness" : "contrast";
		snprintf(buf, size, "%s %s", arg == 0 ? "Reset" : arg > 0 ? "Increase" : "Decrease", text);
	} else if (func == ci_fit_to_win) {
		switch (arg) {
			case SCALE_DOWN: text = "Fit image to window (shrink only)"; break;
			case SCALE_FIT: text = "Fit image to window"; break;
			case SCALE_FILL: text = "Fill window with image"; break;
			case SCALE_WIDTH: text = "Fit image to window width"; break;
			case SCALE_HEIGHT: text = "Fit image to window height"; break;
			default: text = "Set image scale mode"; break;
		}
		snprintf(buf, size, "%s", text);
	} else if (func == ci_rotate) {
		text = arg == DEGREE_90 ? "90 degrees clockwise" :
		       arg == DEGREE_270 ? "90 degrees counter-clockwise" : "180 degrees";
		snprintf(buf, size, "Rotate image %s", text);
	} else if (func == ci_flip) {
		snprintf(buf, size, "Flip image %s", arg == FLIP_HORIZONTAL ? "horizontally" : "vertically");
	} else if (func == ci_drag) {
		snprintf(buf, size, "Drag image (%s)", arg == DRAG_ABSOLUTE ? "absolute" : "relative");
	} else {
		snprintf(buf, size, "Custom command (argument %d)", arg);
	}
}

/* Match the implicit Shift handling in on_keypress(), using the live keymap. */
static unsigned int implicit_shift(Display *dpy, KeySym sym)
{
	XKeyEvent event = { 0 };
	KeySym base, shifted;
	char dummy;

	event.display = dpy;
	event.keycode = XKeysymToKeycode(dpy, sym);
	if (event.keycode == 0)
		return 0;
	XLookupString(&event, &dummy, 1, &base, NULL);
	event.state = ShiftMask;
	XLookupString(&event, &dummy, 1, &shifted, NULL);
	return sym == shifted && shifted != base ? ShiftMask : 0;
}

static bool is_prefix_key(Display *dpy, KeySym sym, unsigned int state)
{
	XKeyEvent event = { 0 };
	KeySym looked_up;
	char text;

	event.display = dpy;
	event.keycode = XKeysymToKeycode(dpy, sym);
	event.state = state;
	return event.keycode != 0 && XLookupString(&event, &text, 1, &looked_up, NULL) == 1 &&
	       text >= '0' && text <= '9';
}

static void format_key(help_row_t *row, unsigned int implicit)
{
	const char *name = XKeysymToString(row->sym);
	const char *label = name;
	unsigned int mask = row->mask & ~implicit;
	char literal[2] = { 0 };
	char unknown[32];

	if (name == NULL) {
		snprintf(unknown, sizeof(unknown), "0x%lx", row->sym);
		label = name = unknown;
	} else if (row->sym == XK_space) {
		label = "Space";
	} else if (row->sym == XK_Return) {
		label = "Enter";
	} else if (row->sym == XK_Prior) {
		label = "Page Up";
	} else if (row->sym == XK_Next) {
		label = "Page Down";
	} else if (row->sym > XK_space && row->sym <= XK_asciitilde) {
		literal[0] = (char)row->sym;
		label = literal;
	}
	snprintf(row->key, sizeof(row->key), "%s%s%s%s%s%s%s%s",
	         mask & ControlMask ? "Ctrl+" : "", mask & Mod1Mask ? "Alt+" : "",
	         mask & ShiftMask ? "Shift+" : "", mask & Mod2Mask ? "Mod2+" : "",
	         mask & Mod3Mask ? "Mod3+" : "", mask & Mod4Mask ? "Super+" : "",
	         mask & Mod5Mask ? "Mod5+" : "", label);
	snprintf(row->aliases, sizeof(row->aliases), "%s %s %s %s %s",
	         name, row->mask & ControlMask ? "control" : "",
	         row->mask & Mod1Mask ? "meta" : "",
	         row->mask & ShiftMask ? "shift" : "",
	         row->mask & Mod4Mask ? "mod4" : "");
}

void help_free(help_t *help)
{
	int i;

	for (i = 0; i < help->count; i++)
		free(help->rows[i].description);
	free(help->rows);
	free(help->matches);
	memset(help, 0, sizeof(*help));
}

void help_open(help_t *help, Display *dpy, const keymap_t *keys, int count,
               unsigned int used_mask, appmode_t current_mode)
{
	int i, j;
	unsigned int implicit, mask;
	help_row_t *row;
	char description[256];
	size_t len, extra;

	help_free(help);
	help->rows = ecalloc(count + 1, sizeof(*help->rows));
	help->matches = ecalloc(count + 1, sizeof(*help->matches));
	help->mode = current_mode;
	help->active = true;
	help->page = 1;
	for (i = 0; i < count; i++) {
		if (keys[i].cmd.func == NULL ||
		    (keys[i].cmd.mode != MODE_ALL && keys[i].cmd.mode != current_mode))
			continue;
		implicit = implicit_shift(dpy, keys[i].ksym_or_button);
		mask = (keys[i].mask | implicit) & used_mask;
		/* Digits are consumed as prefixes before binding dispatch. */
		if (is_prefix_key(dpy, keys[i].ksym_or_button, mask))
			continue;
		for (j = 0; j < help->count; j++) {
			if (help->rows[j].sym == keys[i].ksym_or_button && help->rows[j].mask == mask)
				break;
		}
		row = &help->rows[j];
		describe(&keys[i], description, sizeof(description));
		if (j == help->count) {
			row->sym = keys[i].ksym_or_button;
			row->mask = mask;
			format_key(row, implicit);
			row->description = estrdup(description);
			help->count++;
		} else {
			len = strlen(row->description);
			extra = strlen(description) + 3;
			row->description = erealloc(row->description, len + extra);
			snprintf(row->description + len, extra, "; %s", description);
		}
	}
	row = &help->rows[help->count++];
	strcpy(row->key, "0-9");
	strcpy(row->aliases, "digits numeric prefix count");
	row->description = estrdup("Prefix the next command with a number (count)");
	help_filter(help);
}

static bool contains_word(const char *text, const char *word, size_t len)
{
	size_t i;

	for (; *text != '\0'; text++) {
		for (i = 0; i < len && text[i] != '\0'; i++) {
			if (tolower((unsigned char)text[i]) != tolower((unsigned char)word[i]))
				break;
		}
		if (i == len)
			return true;
	}
	return false;
}

void help_filter(help_t *help)
{
	int i;
	const char *word, *end;
	const help_row_t *row;
	bool match;

	help->total = help->first = 0;
	for (i = 0; i < help->count; i++) {
		row = &help->rows[i];
		match = true;
		for (word = help->query; *word != '\0'; word = end) {
			while (isspace((unsigned char)*word))
				word++;
			end = word;
			while (*end != '\0' && !isspace((unsigned char)*end))
				end++;
			if (!contains_word(row->key, word, end - word) &&
			    !contains_word(row->aliases, word, end - word) &&
			    !contains_word(row->description, word, end - word)) {
				match = false;
				break;
			}
		}
		if (match)
			help->matches[help->total++] = i;
	}
}

void help_scroll(help_t *help, int amount)
{
	int last = MAX(0, help->total - MAX(1, help->page));

	help->first = (int)MAX(0L, MIN((long)last, (long)help->first + amount));
}

void help_keypress(help_t *help, KeySym sym, unsigned int state, const char *text, int len)
{
	size_t n = strlen(help->query);
	size_t before = n;
	int i;

	if (sym == XK_Escape) {
		help->active = false;
		return;
	} else if (sym == XK_Up || sym == XK_Down) {
		help_scroll(help, sym == XK_Up ? -1 : 1);
		return;
	} else if (sym == XK_Prior || sym == XK_Next) {
		help_scroll(help, sym == XK_Prior ? -help->page : help->page);
		return;
	} else if ((state & ControlMask) && (sym == XK_u || sym == XK_U)) {
		help->query[0] = '\0';
	} else if (sym == XK_BackSpace) {
		if (n > 0)
			help->query[n - 1] = '\0';
	} else {
		if (state & (ControlMask | Mod1Mask | Mod4Mask))
			return;
		for (i = 0; i < len && n < sizeof(help->query) - 1; i++) {
			if (text[i] >= ' ' && text[i] <= '~')
				help->query[n++] = text[i];
		}
		help->query[n] = '\0';
		if (n == before)
			return;
	}
	help_filter(help);
}
