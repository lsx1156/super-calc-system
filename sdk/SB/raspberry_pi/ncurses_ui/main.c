#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <unistd.h>
#include <time.h>

#include "widgets.h"
#include "usb_comm.h"

#define NUM_TABS 6
#define TAB_NAMES {"Overview", "Cluster", "Clocks", "Thermal", "Sample", "Crack"}

typedef struct {
    WINDOW *main_win;
    WINDOW *tab_win;
    WINDOW *content_win;
    WINDOW *status_win;
    
    int current_tab;
    int screen_w, screen_h;
    
    usb_handle_t *usb;
    system_info_t sys_info;
    
    slider_t *rate_slider;
    switch_t *sample_switch;
    switch_t *overclock_switch;
    button_t *save_btn;
    button_t *refresh_btn;
    
    int update_interval;
    time_t last_update;
    
    int exit_flag;
} ui_state_t;

static void draw_tabs(ui_state_t *state);
static void draw_overview(ui_state_t *state);
static void draw_cluster(ui_state_t *state);
static void draw_clocks(ui_state_t *state);
static void draw_thermal(ui_state_t *state);
static void draw_sample(ui_state_t *state);
static void draw_crack(ui_state_t *state);
static void draw_status_bar(ui_state_t *state);
static void handle_key(ui_state_t *state, int key);
static int update_data(ui_state_t *state);
static void save_config(ui_state_t *state);

int main(int argc, char *argv[]) {
    ui_state_t state;
    memset(&state, 0, sizeof(ui_state_t));
    
    system_info_init(&state.sys_info);
    
    state.usb = usb_init(NULL);
    if (!state.usb) {
        printf("USB设备未连接，将使用模拟数据\n");
    }
    
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    init_colors();
    
    getmaxyx(stdscr, state.screen_h, state.screen_w);
    
    state.main_win = newwin(state.screen_h, state.screen_w, 0, 0);
    state.tab_win = newwin(3, state.screen_w, 0, 0);
    state.content_win = newwin(state.screen_h - 6, state.screen_w, 3, 0);
    state.status_win = newwin(3, state.screen_w, state.screen_h - 3, 0);
    
    state.current_tab = 0;
    state.update_interval = 1000;
    state.last_update = time(NULL);
    state.exit_flag = 0;
    
    int rate_khz = state.sys_info.sample_rate / 1000;
    state.rate_slider = slider_create(2, 2, 40, "Sample Rate", 1, 125, rate_khz, 1);
    state.sample_switch = switch_create(2, 5, "Sampling", state.sys_info.pico2_status.run_status);
    state.overclock_switch = switch_create(2, 7, "Overclock", state.sys_info.pico2_status.overclock_freq > 133000000);
    state.save_btn = button_create(state.screen_w - 20, state.screen_h - 5, 15, "Save");
    state.refresh_btn = button_create(state.screen_w - 38, state.screen_h - 5, 15, "Refresh");
    
    while (!state.exit_flag) {
        draw_border(state.main_win, 0, 0, state.screen_w, state.screen_h);
        
        draw_tabs(&state);
        
        wclear(state.content_win);
        draw_border(state.content_win, 0, 0, state.screen_w, state.screen_h - 6);
        
        switch (state.current_tab) {
            case 0: draw_overview(&state); break;
            case 1: draw_cluster(&state); break;
            case 2: draw_clocks(&state); break;
            case 3: draw_thermal(&state); break;
            case 4: draw_sample(&state); break;
            case 5: draw_crack(&state); break;
        }
        
        draw_status_bar(&state);
        
        wrefresh(state.main_win);
        wrefresh(state.tab_win);
        wrefresh(state.content_win);
        wrefresh(state.status_win);
        
        timeout(100);
        int key = wgetch(state.main_win);
        
        if (key != ERR) {
            handle_key(&state, key);
        }
        
        time_t now = time(NULL);
        if (now - state.last_update >= 1) {
            update_data(&state);
            state.last_update = now;
        }
    }
    
    usb_close(state.usb);
    
    endwin();
    
    return 0;
}

static void draw_tabs(ui_state_t *state) {
    wclear(state.tab_win);
    
    draw_border(state.tab_win, 0, 0, state.screen_w, 3);
    
    const char *tabs[] = {"Overview", "Cluster", "Clocks", "Thermal", "Sample", "Crack"};
    int tab_w = state.screen_w / NUM_TABS;
    
    for (int i = 0; i < NUM_TABS; i++) {
        int x = i * tab_w;
        
        if (i == state.current_tab) {
            wattron(state.tab_win, COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
            mvwprintw(state.tab_win, 1, x + 2, " [%s] ", tabs[i]);
            wattroff(state.tab_win, COLOR_PAIR(COLOR_PAIR_HIGHLIGHT));
        } else {
            wattron(state.tab_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
            mvwprintw(state.tab_win, 1, x + 2, "  %s  ", tabs[i]);
            wattroff(state.tab_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
        }
    }
}

static void draw_overview(ui_state_t *state) {
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_TITLE));
    mvwprintw(state.content_win, 1, 2, "System Overview");
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_TITLE));
    
    int y = 3;
    
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    
    mvwprintw(state.content_win, y++, 2, "Platform: Raspberry Pi Zero 2W");
    mvwprintw(state.content_win, y++, 2, "Architecture: 3-Layer Distributed (Pi + Pico2 + Pico)");
    mvwprintw(state.content_win, y++, 2, "");
    
    mvwprintw(state.content_win, y++, 2, "--- Cluster Status ---");
    mvwprintw(state.content_win, y++, 4, "Pico2: %d/%d online", state.sys_info.pico2_online, state.sys_info.pico2_count);
    mvwprintw(state.content_win, y++, 4, "Pico: %d/%d online", state.sys_info.pico_online, state.sys_info.pico_count);
    mvwprintw(state.content_win, y++, 4, "Total Analog Channels: %d", state.sys_info.pico_online * 4);
    mvwprintw(state.content_win, y++, 4, "Total Digital Channels: %d", state.sys_info.pico_online * 8);
    
    mvwprintw(state.content_win, y++, 2, "");
    mvwprintw(state.content_win, y++, 2, "--- Current Mode ---");
    
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_ACTIVE));
    mvwprintw(state.content_win, y++, 4, "Mode: %s", mode_to_string(state.sys_info.current_mode));
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_ACTIVE));
    
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    
    mvwprintw(state.content_win, y++, 4, "Running: %s", state.sys_info.pico2_status.run_status ? "YES" : "NO");
    mvwprintw(state.content_win, y++, 4, "Sample Rate: %d KSPS", state.sys_info.sample_rate / 1000);
    
    mvwprintw(state.content_win, y++, 2, "");
    mvwprintw(state.content_win, y++, 2, "--- Pi System ---");
    mvwprintw(state.content_win, y++, 4, "CPU Usage: %.1f%%", state.sys_info.pi_cpu_usage);
    mvwprintw(state.content_win, y++, 4, "Memory Usage: %.1f%%", state.sys_info.pi_mem_usage);
    mvwprintw(state.content_win, y++, 4, "Temperature: %.1fC", state.sys_info.pi_temp);
    
    mvwprintw(state.content_win, y++, 2, "");
    mvwprintw(state.content_win, y++, 2, "--- Pico2 Status ---");
    mvwprintw(state.content_win, y++, 4, "Temperature: %.1fC", state.sys_info.pico2_status.core_temp);
    mvwprintw(state.content_win, y++, 4, "Core Voltage: %.3fV", state.sys_info.pico2_status.vcore);
    mvwprintw(state.content_win, y++, 4, "Total Samples: %lu", state.sys_info.pico2_status.total_samples);
    mvwprintw(state.content_win, y++, 4, "Errors: %lu", state.sys_info.pico2_status.error_count);
    
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
}

static void draw_cluster(ui_state_t *state) {
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_TITLE));
    mvwprintw(state.content_win, 1, 2, "Cluster Topology");
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_TITLE));
    
    int y = 3;
    
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    
    mvwprintw(state.content_win, y++, 2, "Pico2 #0 (RP2350)");
    mvwprintw(state.content_win, y++, 4, "Status: %s", state.sys_info.pico2_online ? "ONLINE" : "OFFLINE");
    mvwprintw(state.content_win, y++, 4, "Temperature: %.1fC", state.sys_info.pico2_status.core_temp);
    mvwprintw(state.content_win, y++, 4, "Frequency: %lu MHz", state.sys_info.pico2_status.overclock_freq / 1000000);
    mvwprintw(state.content_win, y++, 4, "Version: %d.%d.%d", 1, 0, 0);
    
    mvwprintw(state.content_win, y++, 2, "");
    mvwprintw(state.content_win, y++, 2, "Connected Pico Nodes (RP2040):");
    
    for (int i = 0; i < state.sys_info.pico_count; i++) {
        if (i >= state.screen_h - 12) break;
        
        if (state.sys_info.pico2_online) {
            wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_SUCCESS));
            mvwprintw(state.content_win, y++, 4, "[OK] Pico #%d: 4 ADC + 8 Digital", i);
            wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_SUCCESS));
        } else {
            wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_ERROR));
            mvwprintw(state.content_win, y++, 4, "[--] Pico #%d: Disconnected", i);
            wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_ERROR));
        }
    }
    
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
}

static void draw_clocks(ui_state_t *state) {
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_TITLE));
    mvwprintw(state.content_win, 1, 2, "Clock Control");
    wattroff(state.content_win, COLOR_PAIR_TITLE);
    
    int y = 3;
    
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    
    mvwprintw(state.content_win, y++, 2, "--- Pico2 (RP2350) ---");
    mvwprintw(state.content_win, y++, 4, "Current Frequency: %lu MHz", state.sys_info.pico2_status.overclock_freq / 1000000);
    
    if (state.sys_info.pico2_status.overclock_freq > 150000000) {
        wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_WARNING));
        mvwprintw(state.content_win, y++, 4, "Status: OVERCLOCKED (240MHz)");
        wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_WARNING));
    } else {
        wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
        mvwprintw(state.content_win, y++, 4, "Status: DEFAULT (150MHz)");
    }
    
    mvwprintw(state.content_win, y++, 2, "");
    mvwprintw(state.content_win, y++, 2, "--- Pico (RP2040) ---");
    
    if (state.sys_info.pico2_status.overclock_freq > 133000000) {
        wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_WARNING));
        mvwprintw(state.content_win, y++, 4, "Current Frequency: 200 MHz (OVERCLOCKED)");
        wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_WARNING));
    } else {
        wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
        mvwprintw(state.content_win, y++, 4, "Current Frequency: 133 MHz (DEFAULT)");
    }
    
    mvwprintw(state.content_win, y++, 2, "");
    mvwprintw(state.content_win, y++, 2, "--- Overclock Control ---");
    
    switch_draw(state.overclock_switch, state.content_win);
    
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
}

static void draw_thermal(ui_state_t *state) {
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_TITLE));
    mvwprintw(state.content_win, 1, 2, "Thermal Monitoring");
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_TITLE));
    
    int y = 3;
    
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    
    mvwprintw(state.content_win, y++, 2, "--- Raspberry Pi Zero 2W ---");
    
    float pi_temp = state.sys_info.pi_temp;
    if (pi_temp >= 70.0) {
        wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_ERROR));
    } else if (pi_temp >= 55.0) {
        wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_WARNING));
    } else {
        wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_SUCCESS));
    }
    mvwprintw(state.content_win, y++, 4, "Temperature: %.1fC", pi_temp);
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    
    mvwprintw(state.content_win, y++, 2, "");
    mvwprintw(state.content_win, y++, 2, "--- Pico2 (RP2350) ---");
    
    float pico2_temp = state.sys_info.pico2_status.core_temp;
    if (pico2_temp >= 70.0) {
        wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_ERROR));
    } else if (pico2_temp >= 55.0) {
        wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_WARNING));
    } else {
        wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_SUCCESS));
    }
    mvwprintw(state.content_win, y++, 4, "Temperature: %.1fC", pico2_temp);
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    
    mvwprintw(state.content_win, y++, 4, "Core Voltage: %.3fV", state.sys_info.pico2_status.vcore);
    
    mvwprintw(state.content_win, y++, 2, "");
    mvwprintw(state.content_win, y++, 2, "--- Pico (RP2040) ---");
    
    for (int i = 0; i < state.sys_info.pico_count; i++) {
        if (i >= state.screen_h - 15) break;
        
        float temp = 35.0 + (float)i * 2.5;
        if (temp >= 70.0) {
            wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_ERROR));
        } else if (temp >= 55.0) {
            wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_WARNING));
        } else {
            wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_SUCCESS));
        }
        mvwprintw(state.content_win, y++, 4, "Pico #%d: %.1fC", i, temp);
        wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    }
    
    mvwprintw(state.content_win, y++, 2, "");
    mvwprintw(state.content_win, y++, 2, "--- Temperature Limits ---");
    mvwprintw(state.content_win, y++, 4, "Warning: 55C");
    mvwprintw(state.content_win, y++, 4, "Shutdown: 70C");
    
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
}

static void draw_sample(ui_state_t *state) {
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_TITLE));
    mvwprintw(state.content_win, 1, 2, "Sample Configuration");
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_TITLE));
    
    int y = 3;
    
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    
    mvwprintw(state.content_win, y++, 2, "--- Sample Rate ---");
    slider_draw(state.rate_slider, state.content_win);
    y += 2;
    
    mvwprintw(state.content_win, y++, 2, "--- Controls ---");
    switch_draw(state.sample_switch, state.content_win);
    y += 2;
    
    mvwprintw(state.content_win, y++, 2, "--- Current Status ---");
    wattron(state.content_win, COLOR_PAIR(state.sys_info.pico2_status.run_status ? COLOR_PAIR_SUCCESS : COLOR_PAIR_DEFAULT));
    mvwprintw(state.content_win, y++, 4, "Sampling: %s", state.sys_info.pico2_status.run_status ? "ACTIVE" : "INACTIVE");
    wattroff(state.content_win, COLOR_PAIR(state.sys_info.pico2_status.run_status ? COLOR_PAIR_SUCCESS : COLOR_PAIR_DEFAULT));
    
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    mvwprintw(state.content_win, y++, 4, "Rate: %d KSPS", state.sys_info.sample_rate / 1000);
    mvwprintw(state.content_win, y++, 4, "Total Samples: %lu", state.sys_info.pico2_status.total_samples);
    
    mvwprintw(state.content_win, y++, 2, "");
    mvwprintw(state.content_win, y++, 2, "--- Analog Channels ---");
    
    for (int i = 0; i < 4 * state.sys_info.pico_online; i++) {
        if (i >= state.screen_h - 15) break;
        float val = 1.5 + 0.5 * sin((float)i * 0.5);
        mvwprintw(state.content_win, y++, 4, "CH%d: %.3fV", i, val);
    }
    
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
}

static void draw_crack(ui_state_t *state) {
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_TITLE));
    mvwprintw(state.content_win, 1, 2, "Crack Engine");
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_TITLE));
    
    int y = 3;
    
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    
    mvwprintw(state.content_win, y++, 2, "--- Current Mode ---");
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_ACTIVE));
    mvwprintw(state.content_win, y++, 4, "Mode: %s", mode_to_string(state.sys_info.current_mode));
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_ACTIVE));
    
    wattron(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    
    mvwprintw(state.content_win, y++, 2, "");
    mvwprintw(state.content_win, y++, 2, "--- Hash Types ---");
    mvwprintw(state.content_win, y++, 4, "[*] MD5");
    mvwprintw(state.content_win, y++, 4, "[*] SHA-1");
    mvwprintw(state.content_win, y++, 4, "[*] SHA-256");
    mvwprintw(state.content_win, y++, 4, "[*] SHA-512");
    
    mvwprintw(state.content_win, y++, 2, "");
    mvwprintw(state.content_win, y++, 2, "--- Brute Force Settings ---");
    mvwprintw(state.content_win, y++, 4, "Key Length: 4-16 characters");
    mvwprintw(state.content_win, y++, 4, "Charset: 0-9, a-f (hex)");
    mvwprintw(state.content_win, y++, 4, "Max Workers: 1 (Zero 2W)");
    
    mvwprintw(state.content_win, y++, 2, "");
    mvwprintw(state.content_win, y++, 2, "--- Progress ---");
    mvwprintw(state.content_win, y++, 4, "Status: Ready");
    mvwprintw(state.content_win, y++, 4, "Processed: 0");
    mvwprintw(state.content_win, y++, 4, "Found: 0");
    
    wattroff(state.content_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
}

static void draw_status_bar(ui_state_t *state) {
    wclear(state.status_win);
    
    wattron(state.status_win, COLOR_PAIR(COLOR_PAIR_BORDER));
    mvwaddch(state.status_win, 0, 0, ACS_HLINE);
    for (int i = 1; i < state.screen_w - 1; i++) mvwaddch(state.status_win, 0, i, ACS_HLINE);
    mvwaddch(state.status_win, 0, state.screen_w - 1, ACS_HLINE);
    wattroff(state.status_win, COLOR_PAIR(COLOR_PAIR_BORDER));
    
    wattron(state.status_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
    
    mvwprintw(state.status_win, 1, 2, "SuperCalc V1.0 | Zero 2W");
    
    mvwprintw(state.status_win, 1, state.screen_w - 80, 
              "Mode: %s | Pico2: %d/%d | Pico: %d/%d", 
              mode_to_string(state.sys_info.current_mode),
              state.sys_info.pico2_online, state.sys_info.pico2_count,
              state.sys_info.pico_online, state.sys_info.pico_count);
    
    mvwprintw(state.status_win, 1, state.screen_w - 30, "↑↓:Tab ←→:Nav Enter:Select");
    
    wattroff(state.status_win, COLOR_PAIR(COLOR_PAIR_DEFAULT));
}

static void handle_key(ui_state_t *state, int key) {
    switch (key) {
        case KEY_UP:
        case 'k':
            if (state.current_tab > 0) state.current_tab--;
            break;
            
        case KEY_DOWN:
        case 'j':
            if (state.current_tab < NUM_TABS - 1) state.current_tab++;
            break;
            
        case KEY_LEFT:
        case 'h':
            if (state.current_tab > 0) state.current_tab--;
            break;
            
        case KEY_RIGHT:
        case 'l':
            if (state.current_tab < NUM_TABS - 1) state.current_tab++;
            break;
            
        case KEY_ENTER:
        case '\n':
            if (state.current_tab == 2) {
                switch_toggle(state.overclock_switch);
                system_set_overclock(state.usb, switch_get_value(state.overclock_switch));
            } else if (state.current_tab == 4) {
                switch_toggle(state.sample_switch);
                if (switch_get_value(state.sample_switch)) {
                    system_start_sample(&state->sys_info, state->usb);
                } else {
                    system_stop_sample(&state->sys_info, state->usb);
                }
            }
            break;
            
        case 's':
        case 'S':
            save_config(state);
            break;
            
        case 'r':
        case 'R':
            update_data(state);
            break;
            
        case 27:
            state->exit_flag = 1;
            break;
    }
}

static int update_data(ui_state_t *state) {
    system_info_update(&state->sys_info, state->usb);
    
    int rate_khz = state->sys_info.sample_rate / 1000;
    slider_set_value(state->rate_slider, rate_khz);
    
    return 0;
}

static void save_config(ui_state_t *state) {
    FILE *f = fopen("/home/pi/super_calc/config/config.json", "w");
    if (!f) return;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"mode\": %d,\n", state->sys_info.current_mode);
    fprintf(f, "  \"sample_rate\": %d,\n", state->sys_info.sample_rate);
    fprintf(f, "  \"overclock\": %d\n", switch_get_value(state->overclock_switch));
    fprintf(f, "}\n");
    
    fclose(f);
}