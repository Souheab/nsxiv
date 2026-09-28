/* Commands are identities here: the reference must never execute them. */
#include "help.h"
#include "commands.h"
#include <X11/keysym.h>
#include <stdlib.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>

static const opt_t test_options = { 0 };
const opt_t *options = &test_options;

#define STUB(name) bool name(arg_t arg) { (void)arg; abort(); return false; }
STUB(cg_help)
STUB(cg_change_gamma)
STUB(cg_change_brightness)
STUB(cg_change_contrast)
STUB(cg_first)
STUB(cg_mark_range)
STUB(cg_n_or_last)
STUB(cg_navigate_marked)
STUB(cg_prefix_external)
STUB(cg_quit)
STUB(cg_pick_quit)
STUB(cg_reload_image)
STUB(cg_remove_image)
STUB(cg_reverse_marks)
STUB(cg_scroll_screen)
STUB(cg_switch_mode)
STUB(cg_toggle_bar)
STUB(cg_toggle_fullscreen)
STUB(cg_toggle_image_mark)
STUB(cg_unmark_all)
STUB(cg_zoom)
STUB(ci_alternate)
STUB(ci_cursor_navigate)
STUB(ci_drag)
STUB(ci_fit_to_win)
STUB(ci_flip)
STUB(ci_navigate)
STUB(ci_navigate_frame)
STUB(ci_rotate)
STUB(ci_scroll)
STUB(ci_scroll_to_center)
STUB(ci_scroll_to_edge)
STUB(ci_set_zoom)
STUB(ci_slideshow)
STUB(ci_toggle_alpha)
STUB(ci_toggle_animation)
STUB(ci_toggle_antialias)
STUB(ct_move_sel)
STUB(ct_reload_all)
STUB(ct_scroll)
STUB(ct_drag_mark_image)
STUB(ct_select)

static bool custom(arg_t arg)
{
	(void)arg;
	abort();
	return false;
}

static void query(help_t *help, const char *text)
{
	snprintf(help->query, sizeof(help->query), "%s", text);
	help_filter(help);
}

int main(void)
{
	static const keymap_t keys[] = {
		{ ControlMask, XK_equal, { cg_zoom, MODE_ALL }, +1 },
		{ 0, XK_n, { ci_navigate, MODE_IMAGE }, +1 },
		{ 0, XK_n, { ci_scroll_to_edge, MODE_IMAGE }, DIR_LEFT | DIR_UP },
		{ 0, XK_j, { ct_move_sel, MODE_THUMB }, DIR_DOWN },
		{ 0, XK_less, { ci_rotate, MODE_IMAGE }, DEGREE_270 },
		{ 0, XK_greater, { ci_rotate, MODE_IMAGE }, DEGREE_90 },
		{ 0, XK_question, { cg_help, MODE_ALL }, 0 },
		{ ShiftMask, XK_question, { custom, MODE_ALL }, 42 },
		{ 0, XK_space, { ci_navigate, MODE_IMAGE }, 10 },
		{ 0, XK_Q, { cg_pick_quit, MODE_ALL }, 0 },
		{ 0, XK_F2, { custom, MODE_ALL }, -7 },
		{ 0, XK_F3, { NULL, MODE_ALL }, 0 },
		{ 0, XK_2, { cg_zoom, MODE_ALL }, 1 },
		{ ControlMask, XK_6, { ci_alternate, MODE_IMAGE }, 0 }
	};
	Display *dpy = XOpenDisplay(NULL);
	help_t help = { 0 };
	const help_row_t *row;
	char input[300];
	int i, saved;

	assert(dpy != NULL);
	help_open(&help, dpy, keys, ARRLEN(keys), ShiftMask | ControlMask | Mod1Mask, MODE_IMAGE);
	assert(help.active && help.mode == MODE_IMAGE && help.query[0] == '\0');
	assert(help.count == 10); /* grouped keys, disabled/mode-filtered keys, plus count */
	assert(strcmp(help.rows[0].key, "Ctrl+=") == 0);
	query(&help, " ZOOM   CtRl ");
	assert(help.total == 1);
	assert(strstr(help.rows[help.matches[0]].description, "Zoom in") != NULL);
	query(&help, "rotate 90");
	assert(help.total == 2);
	assert(strcmp(help.rows[help.matches[0]].key, "<") == 0);
	assert(strstr(help.rows[help.matches[0]].description, "counter-clockwise") != NULL);
	query(&help, "top left forward");
	assert(help.total == 1);
	row = &help.rows[help.matches[0]];
	assert(strcmp(row->key, "n") == 0);
	assert(strcmp(row->description, "Go forward 1 image(s) (multiplied by count); Scroll to edge top left") == 0);
	query(&help, "question");
	assert(help.total == 1);
	row = &help.rows[help.matches[0]];
	assert(strcmp(row->key, "?") == 0);
	assert(strstr(row->description, "; Custom command (argument 42)") != NULL);
	query(&help, "space 10");
	assert(help.total == 1);
	assert(strcmp(help.rows[help.matches[0]].key, "Space") == 0);
	query(&help, "shift quit");
	assert(help.total == 1 && strcmp(help.rows[help.matches[0]].key, "Q") == 0);
	query(&help, "custom -7");
	assert(help.total == 1 && strcmp(help.rows[help.matches[0]].key, "F2") == 0);
	query(&help, "numeric");
	assert(help.total == 1 && strcmp(help.rows[help.matches[0]].key, "0-9") == 0);
	query(&help, "alternate");
	assert(help.total == 1 && strcmp(help.rows[help.matches[0]].key, "Ctrl+6") == 0);
	query(&help, "no such command");
	assert(help.total == 0 && help.first == 0);
	help_scroll(&help, 100);
	assert(help.first == 0);
	query(&help, "");
	help.page = 3;
	help_scroll(&help, 100);
	assert(help.first == help.total - 3);
	saved = help.first;
	help_keypress(&help, XK_Return, 0, "\r", 1);
	assert(help.first == saved); /* Enter is a true no-op */
	help_keypress(&help, XK_Prior, 0, "", 0);
	assert(help.first == saved - 3);
	help_scroll(&help, -100);
	assert(help.first == 0);
	help.page = 100;
	help_scroll(&help, 100);
	assert(help.first == 0);
	help_keypress(&help, XK_q, 0, "q?123", 5);
	assert(strcmp(help.query, "q?123") == 0 && help.active);
	help_keypress(&help, XK_BackSpace, 0, "", 0);
	assert(strcmp(help.query, "q?12") == 0);
	help_keypress(&help, XK_u, ControlMask, "", 0);
	assert(help.query[0] == '\0');
	memset(input, 'a', sizeof(input));
	help_keypress(&help, XK_a, 0, input, sizeof(input));
	assert(strlen(help.query) == 255);
	help_keypress(&help, XK_u, ControlMask, "", 0);
	help_keypress(&help, XK_a, Mod1Mask, "a", 1);
	help_keypress(&help, XK_Return, 0, "\r", 1);
	help_keypress(&help, XK_a, 0, "\xc3\xa9", 2);
	assert(help.query[0] == '\0');
	help_keypress(&help, XK_Escape, 0, "", 0);
	assert(!help.active);
	help_open(&help, dpy, keys, ARRLEN(keys), ShiftMask | ControlMask | Mod1Mask, MODE_THUMB);
	assert(help.query[0] == '\0' && help.first == 0);
	query(&help, "selection down");
	assert(help.total == 1 && strcmp(help.rows[help.matches[0]].key, "j") == 0);
	query(&help, "rotate");
	assert(help.total == 0);
	for (i = 0; i < 20; i++)
		help_open(&help, dpy, keys, ARRLEN(keys), ShiftMask | ControlMask, MODE_IMAGE);
	help_free(&help);
	assert(!help.active && help.rows == NULL);
	XCloseDisplay(dpy);
	puts("help tests passed");
	return 0;
}
