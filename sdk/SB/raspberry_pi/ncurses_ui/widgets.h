#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include <ncurses.h>

#define COLOR_PAIR_DEFAULT 1
#define COLOR_PAIR_HIGHLIGHT 2
#define COLOR_PAIR_ACTIVE 3
#define COLOR_PAIR_BORDER 4
#define COLOR_PAIR_TITLE 5
#define COLOR_PAIR_WARNING 6
#define COLOR_PAIR_ERROR 7
#define COLOR_PAIR_SUCCESS 8

typedef struct {
    int x, y, w, h;
    char *label;
    int selected;
    int (*draw)(void *widget, WINDOW *win);
    int (*handle_key)(void *widget, int key);
    void *data;
} widget_t;

typedef struct {
    widget_t base;
    int value;
    int min;
    int max;
    int step;
    int (*on_change)(int value);
} slider_t;

typedef struct {
    widget_t base;
    int value;
    int (*on_toggle)(int value);
} switch_t;

typedef struct {
    widget_t base;
    char *text;
    int (*on_click)(void);
} button_t;

typedef struct {
    widget_t base;
    char *text;
    int align;
} label_t;

typedef struct {
    widget_t base;
    int item_count;
    char **items;
    int selected_idx;
    int (*on_select)(int idx);
} menu_t;

void init_colors(void);

widget_t *widget_create(int x, int y, int w, int h, const char *label);
void widget_destroy(widget_t *widget);
int widget_draw(widget_t *widget, WINDOW *win);
int widget_handle_key(widget_t *widget, int key);

slider_t *slider_create(int x, int y, int w, const char *label, int min, int max, int value, int step);
void slider_set_value(slider_t *slider, int value);
int slider_get_value(slider_t *slider);

switch_t *switch_create(int x, int y, const char *label, int value);
void switch_toggle(switch_t *sw);
int switch_get_value(switch_t *sw);

button_t *button_create(int x, int y, int w, const char *text);
void button_set_text(button_t *btn, const char *text);

label_t *label_create(int x, int y, int w, const char *text, int align);
void label_set_text(label_t *label, const char *text);

menu_t *menu_create(int x, int y, int w, int h, int item_count, const char **items);
void menu_set_selected(menu_t *menu, int idx);
int menu_get_selected(menu_t *menu);

void draw_border(WINDOW *win, int x, int y, int w, int h);
void draw_title(WINDOW *win, int x, int y, int w, const char *title);
void draw_highlight(WINDOW *win, int x, int y, int w, int h, int selected);
int get_center_x(int parent_w, int child_w);
int get_center_y(int parent_h, int child_h);

#endif