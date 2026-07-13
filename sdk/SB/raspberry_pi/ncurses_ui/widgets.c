#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

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

widget_t *widget_create(int x, int y, int w, int h, const char *label);
void widget_destroy(widget_t *widget);
int widget_draw(widget_t *widget, WINDOW *win);
int widget_handle_key(widget_t *widget, int key);

slider_t *slider_create(int x, int y, int w, const char *label, int min, int max, int value, int step);
void slider_set_value(slider_t *slider, int value);

switch_t *switch_create(int x, int y, const char *label, int value);
void switch_toggle(switch_t *sw);

button_t *button_create(int x, int y, int w, const char *text);
void button_set_text(button_t *btn, const char *text);

label_t *label_create(int x, int y, int w, const char *text, int align);
void label_set_text(label_t *label, const char *text);

menu_t *menu_create(int x, int y, int w, int h, int item_count, const char **items);
void menu_set_selected(menu_t *menu, int idx);

void draw_border(WINDOW *win, int x, int y, int w, int h);
void draw_title(WINDOW *win, int x, int y, int w, const char *title);
void draw_highlight(WINDOW *win, int x, int y, int w, int h, int selected);
int get_center_x(int parent_w, int child_w);
int get_center_y(int parent_h, int child_h);

void init_colors(void) {
    start_color();
    
    init_pair(COLOR_PAIR_DEFAULT, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_PAIR_HIGHLIGHT, COLOR_BLACK, COLOR_CYAN);
    init_pair(COLOR_PAIR_ACTIVE, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_PAIR_BORDER, COLOR_CYAN, COLOR_BLACK);
    init_pair(COLOR_PAIR_TITLE, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_PAIR_WARNING, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_PAIR_ERROR, COLOR_RED, COLOR_BLACK);
    init_pair(COLOR_PAIR_SUCCESS, COLOR_GREEN, COLOR_BLACK);
}

widget_t *widget_create(int x, int y, int w, int h, const char *label) {
    widget_t *wgt = malloc(sizeof(widget_t));
    if (!wgt) return NULL;
    
    memset(wgt, 0, sizeof(widget_t));
    wgt->x = x;
    wgt->y = y;
    wgt->w = w;
    wgt->h = h;
    if (label) wgt->label = strdup(label);
    wgt->selected = 0;
    
    return wgt;
}

void widget_destroy(widget_t *widget) {
    if (!widget) return;
    free(widget->label);
    free(widget);
}

int widget_draw(widget_t *widget, WINDOW *win) {
    if (!widget || !widget->draw) return -1;
    return widget->draw(widget, win);
}

int widget_handle_key(widget_t *widget, int key) {
    if (!widget || !widget->handle_key) return -1;
    return widget->handle_key(widget, key);
}

static int slider_draw(void *widget, WINDOW *win);
static int slider_handle_key(void *widget, int key);

slider_t *slider_create(int x, int y, int w, const char *label, int min, int max, int value, int step) {
    slider_t *s = malloc(sizeof(slider_t));
    if (!s) return NULL;
    
    s->base = *(widget_create(x, y, w, 2, label));
    s->min = min;
    s->max = max;
    s->value = (value < min) ? min : (value > max) ? max : value;
    s->step = step;
    s->on_change = NULL;
    
    s->base.draw = slider_draw;
    s->base.handle_key = slider_handle_key;
    
    return s;
}

void slider_set_value(slider_t *slider, int value) {
    if (!slider) return;
    slider->value = (value < slider->min) ? slider->min : (value > slider->max) ? slider->max : value;
    if (slider->on_change) slider->on_change(slider->value);
}

int slider_get_value(slider_t *slider) {
    return slider ? slider->value : 0;
}

static int slider_draw(void *widget, WINDOW *win) {
    slider_t *s = (slider_t*)widget;
    if (!s) return -1;
    
    int pos = ((s->value - s->min) * (s->base.w - 4)) / (s->max - s->min + 1);
    
    wattron(win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    mvwprintw(win, s->base.y, s->base.x, "%s:", s->base.label);
    
    wattron(win, COLOR_PAIR(s->base.selected ? COLOR_PAIR_HIGHLIGHT : COLOR_PAIR_DEFAULT));
    mvwprintw(win, s->base.y, s->base.x + strlen(s->base.label) + 2, "[");
    
    for (int i = 0; i < s->base.w - 4; i++) {
        if (i <= pos) {
            wattron(win, COLOR_PAIR(COLOR_PAIR_ACTIVE));
            waddch(win, '=');
            wattroff(win, COLOR_PAIR(COLOR_PAIR_ACTIVE));
        } else {
            waddch(win, ' ');
        }
    }
    
    waddch(win, ']');
    wattroff(win, COLOR_PAIR(s->base.selected ? COLOR_PAIR_HIGHLIGHT : COLOR_PAIR_DEFAULT));
    
    wattron(win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    char val_str[16];
    snprintf(val_str, sizeof(val_str), " %d", s->value);
    mvwprintw(win, s->base.y, s->base.x + s->base.w - strlen(val_str), "%s", val_str);
    
    return 0;
}

static int slider_handle_key(void *widget, int key) {
    slider_t *s = (slider_t*)widget;
    if (!s) return -1;
    
    switch (key) {
        case KEY_LEFT:
        case 'h':
            slider_set_value(s, s->value - s->step);
            return 1;
        case KEY_RIGHT:
        case 'l':
            slider_set_value(s, s->value + s->step);
            return 1;
        case KEY_HOME:
            slider_set_value(s, s->min);
            return 1;
        case KEY_END:
            slider_set_value(s, s->max);
            return 1;
    }
    
    return 0;
}

static int switch_draw(void *widget, WINDOW *win);
static int switch_handle_key(void *widget, int key);

switch_t *switch_create(int x, int y, const char *label, int value) {
    switch_t *sw = malloc(sizeof(switch_t));
    if (!sw) return NULL;
    
    sw->base = *(widget_create(x, y, strlen(label) + 8, 1, NULL));
    sw->base.label = strdup(label);
    sw->value = value;
    sw->on_toggle = NULL;
    
    sw->base.draw = switch_draw;
    sw->base.handle_key = switch_handle_key;
    
    return sw;
}

void switch_toggle(switch_t *sw) {
    if (!sw) return;
    sw->value = !sw->value;
    if (sw->on_toggle) sw->on_toggle(sw->value);
}

int switch_get_value(switch_t *sw) {
    return sw ? sw->value : 0;
}

static int switch_draw(void *widget, WINDOW *win) {
    switch_t *sw = (switch_t*)widget;
    if (!sw) return -1;
    
    wattron(win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    mvwprintw(win, sw->base.y, sw->base.x, "%s ", sw->base.label);
    
    wattron(win, COLOR_PAIR(sw->value ? COLOR_PAIR_ACTIVE : COLOR_PAIR_DEFAULT));
    mvwprintw(win, sw->base.y, sw->base.x + strlen(sw->base.label) + 1, 
               sw->value ? "[ON]" : "[OFF]");
    wattroff(win, COLOR_PAIR(sw->value ? COLOR_PAIR_ACTIVE : COLOR_PAIR_DEFAULT));
    
    return 0;
}

static int switch_handle_key(void *widget, int key) {
    switch_t *sw = (switch_t*)widget;
    if (!sw) return -1;
    
    if (key == KEY_ENTER || key == '\n' || key == ' ') {
        switch_toggle(sw);
        return 1;
    }
    
    return 0;
}

static int button_draw(void *widget, WINDOW *win);
static int button_handle_key(void *widget, int key);

button_t *button_create(int x, int y, int w, const char *text) {
    button_t *btn = malloc(sizeof(button_t));
    if (!btn) return NULL;
    
    btn->base = *(widget_create(x, y, w, 1, NULL));
    btn->text = strdup(text);
    btn->on_click = NULL;
    
    btn->base.draw = button_draw;
    btn->base.handle_key = button_handle_key;
    
    return btn;
}

void button_set_text(button_t *btn, const char *text) {
    if (!btn) return;
    free(btn->text);
    btn->text = strdup(text);
}

static int button_draw(void *widget, WINDOW *win) {
    button_t *btn = (button_t*)widget;
    if (!btn) return -1;
    
    wattron(win, COLOR_PAIR(btn->base.selected ? COLOR_PAIR_HIGHLIGHT : COLOR_PAIR_DEFAULT));
    
    int text_len = strlen(btn->text);
    int pad_left = (btn->base.w - text_len) / 2;
    int pad_right = btn->base.w - text_len - pad_left;
    
    waddch(win, '[');
    for (int i = 0; i < pad_left; i++) waddch(win, ' ');
    waddstr(win, btn->text);
    for (int i = 0; i < pad_right; i++) waddch(win, ' ');
    waddch(win, ']');
    
    wattroff(win, COLOR_PAIR(btn->base.selected ? COLOR_PAIR_HIGHLIGHT : COLOR_PAIR_DEFAULT));
    
    return 0;
}

static int button_handle_key(void *widget, int key) {
    button_t *btn = (button_t*)widget;
    if (!btn) return -1;
    
    if (key == KEY_ENTER || key == '\n') {
        if (btn->on_click) btn->on_click();
        return 1;
    }
    
    return 0;
}

static int label_draw(void *widget, WINDOW *win);

label_t *label_create(int x, int y, int w, const char *text, int align) {
    label_t *label = malloc(sizeof(label_t));
    if (!label) return NULL;
    
    label->base = *(widget_create(x, y, w, 1, NULL));
    label->text = strdup(text);
    label->align = align;
    
    label->base.draw = label_draw;
    label->base.handle_key = NULL;
    
    return label;
}

void label_set_text(label_t *label, const char *text) {
    if (!label) return;
    free(label->text);
    label->text = strdup(text);
}

static int label_draw(void *widget, WINDOW *win) {
    label_t *label = (label_t*)widget;
    if (!label) return -1;
    
    wattron(win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    
    int text_len = strlen(label->text);
    int pos = label->base.x;
    
    switch (label->align) {
        case 1:
            pos += (label->base.w - text_len) / 2;
            break;
        case 2:
            pos += label->base.w - text_len;
            break;
    }
    
    mvwprintw(win, label->base.y, pos, "%s", label->text);
    
    return 0;
}

static int menu_draw(void *widget, WINDOW *win);
static int menu_handle_key(void *widget, int key);

menu_t *menu_create(int x, int y, int w, int h, int item_count, const char **items) {
    menu_t *menu = malloc(sizeof(menu_t));
    if (!menu) return NULL;
    
    menu->base = *(widget_create(x, y, w, h, NULL));
    menu->item_count = item_count;
    menu->items = malloc(item_count * sizeof(char*));
    menu->selected_idx = 0;
    menu->on_select = NULL;
    
    for (int i = 0; i < item_count; i++) {
        menu->items[i] = strdup(items[i]);
    }
    
    menu->base.draw = menu_draw;
    menu->base.handle_key = menu_handle_key;
    
    return menu;
}

void menu_set_selected(menu_t *menu, int idx) {
    if (!menu) return;
    menu->selected_idx = (idx < 0) ? 0 : (idx >= menu->item_count) ? menu->item_count - 1 : idx;
}

static int menu_draw(void *widget, WINDOW *win) {
    menu_t *menu = (menu_t*)widget;
    if (!menu) return -1;
    
    for (int i = 0; i < menu->item_count; i++) {
        if (i >= menu->base.h) break;
        
        wattron(win, COLOR_PAIR(i == menu->selected_idx ? COLOR_PAIR_HIGHLIGHT : COLOR_PAIR_DEFAULT));
        mvwprintw(win, menu->base.y + i, menu->base.x, " %s", menu->items[i]);
        wattroff(win, COLOR_PAIR(i == menu->selected_idx ? COLOR_PAIR_HIGHLIGHT : COLOR_PAIR_DEFAULT));
    }
    
    return 0;
}

static int menu_handle_key(void *widget, int key) {
    menu_t *menu = (menu_t*)widget;
    if (!menu) return -1;
    
    switch (key) {
        case KEY_UP:
        case 'k':
            menu_set_selected(menu, menu->selected_idx - 1);
            return 1;
        case KEY_DOWN:
        case 'j':
            menu_set_selected(menu, menu->selected_idx + 1);
            return 1;
        case KEY_ENTER:
        case '\n':
            if (menu->on_select) menu->on_select(menu->selected_idx);
            return 1;
    }
    
    return 0;
}

void draw_border(WINDOW *win, int x, int y, int w, int h) {
    wattron(win, COLOR_PAIR(COLOR_PAIR_BORDER));
    
    mvwaddch(win, y, x, ACS_ULCORNER);
    for (int i = 1; i < w - 1; i++) mvwaddch(win, y, x + i, ACS_HLINE);
    mvwaddch(win, y, x + w - 1, ACS_URCORNER);
    
    for (int i = 1; i < h - 1; i++) {
        mvwaddch(win, y + i, x, ACS_VLINE);
        mvwaddch(win, y + i, x + w - 1, ACS_VLINE);
    }
    
    mvwaddch(win, y + h - 1, x, ACS_LLCORNER);
    for (int i = 1; i < w - 1; i++) mvwaddch(win, y + h - 1, x + i, ACS_HLINE);
    mvwaddch(win, y + h - 1, x + w - 1, ACS_LRCORNER);
    
    wattroff(win, COLOR_PAIR(COLOR_PAIR_BORDER));
}

void draw_title(WINDOW *win, int x, int y, int w, const char *title) {
    wattron(win, COLOR_PAIR(COLOR_PAIR_TITLE));
    int title_len = strlen(title);
    int pos = x + (w - title_len) / 2;
    mvwprintw(win, y, pos, "%s", title);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_TITLE));
}

void draw_highlight(WINDOW *win, int x, int y, int w, int h, int selected) {
    if (!selected) return;
    
    wattron(win, COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
    for (int i = 0; i < h; i++) {
        mvwchgat(win, y + i, x, w, A_NORMAL, COLOR_PAIR(COLOR_PAIR_HIGHLIGHT), NULL);
    }
    wattroff(win, COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
}

int get_center_x(int parent_w, int child_w) {
    return (parent_w - child_w) / 2;
}

int get_center_y(int parent_h, int child_h) {
    return (parent_h - child_h) / 2;
}