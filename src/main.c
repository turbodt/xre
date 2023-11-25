#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "xre.h"
#include "./kbhit.h"
#include "./terminal.h"
#include "./msleep.h"
#include "./screen.h"
#include "./use_time.h"
#include "./use_timespec.h"


struct Box {
    int x;
    int y;
    size_t width;
    size_t height;
};


#define KEY_PRESS_DEBOUNCE_S 0.05


static Screen * screen = NULL;
static void screen_init(struct ScreenSize const * screen_size) {
    screen = screen_alloc(stdout, screen_size);
}
static void screen_clean_up(void) {
    screen_destroy(screen);
    screen = NULL;
}


//------------------------------------------------------------------------------


void draw_box(struct Box const *box, char bg, char has_border) {
    ScreenCoordinates coords = {0,0};

    for (int i = 0; i < (int) box->height; i++) {
        for (int j = 0; j < (int) box->width; j++) {
            coords.x = box->x + j;
            coords.y = box->y + i;
            screen_printf(screen, &coords, "%c", bg);
        }
    }

    if (!has_border) {
        return;
    }

    for (int j = 0; j < (int) box->width; j++) {
        coords.x = box->x + j;
        coords.y = box->y - 1;
        screen_printf(screen, &coords, "-");
        coords.y = box->y + (int) box->height;
        screen_printf(screen, &coords, "-");
    }

    for (int i = 0; i < (int) box->height; i++) {
        coords.x = box->x -1;
        coords.y = box->y +i;
        screen_printf(screen, &coords, "|");
        coords.x = box->x + (int) box->width;
        screen_printf(screen, &coords, "|");
    }

    coords.x = box->x - 1;
    coords.y = box->y - 1;
    screen_printf(screen, &coords, ".");

    coords.x = box->x - 1;
    coords.y = box->y + (int) box->height;
    screen_printf(screen, &coords, "'");

    coords.x = box->x + (int) box->width;
    coords.y = box->y - 1;
    screen_printf(screen, &coords, ".");

    coords.x = box->x + (int) box->width;
    coords.y = box->y + (int) box->height;
    screen_printf(screen, &coords, "'");
};


//------------------------------------------------------------------------------


char use_time_interval(struct XREContext *ctx, double seconds) {
    struct StateTimeSpec * last_time_state = xre_use_timespec(
        ctx,
        (struct timespec){0,0}
    );
    struct timespec last_time = xre_state_get_timespec(last_time_state);

    struct timespec now = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &now);

    long int sdiff = now.tv_sec - last_time.tv_sec;
    long int ndiff = ((sdiff * 1000000000L) + now.tv_nsec) - last_time.tv_nsec;

    if ((long int)(seconds * 1000000000L) > ndiff) {
        return 0;
    };

    xre_state_set_timespec(last_time_state, now);
    return 1;
}


//------------------------------------------------------------------------------

struct Timer;

struct Timer {
    char is_set;
    struct timespec set_at;
    double duration_sec;
};

void * timer_alloc(va_list args) {
    (void) args;

    struct Timer * timer = (struct Timer *) malloc(sizeof(struct Timer));

    timer->is_set = 0;
    timer->set_at = (struct timespec){0, 0};
    timer->duration_sec = 0;

    return timer;
};

void timer_set(struct Timer * timer, double seconds) {
    clock_gettime(CLOCK_MONOTONIC, &timer->set_at);

    timer->duration_sec = seconds;

    timer->is_set = 1;
};

void timer_clear(struct Timer * timer) {
    timer->is_set = 0;
};

double timer_get_time_sec(struct Timer const *timer) {
    if (!timer->is_set) {
        return 0.0;
    }

    struct timespec now = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &now);

    double current_sec = (now.tv_nsec - timer->set_at.tv_nsec) / 1000000000;
    current_sec += now.tv_sec - timer->set_at.tv_sec;

    return current_sec;
};

double timer_get_progress(struct Timer const *timer) {
    double current_sec = timer_get_time_sec(timer);
    if (timer->duration_sec < current_sec) {
        return 1.0;
    }

    return current_sec / timer->duration_sec;
}

char timer_is_done(struct Timer const *timer) {
    return timer_get_progress(timer) >= 1.0;
};

char timer_is_running(struct Timer const *timer) {
    if (!timer->is_set) {
        return 0;
    }

    return 1 - timer_is_done(timer);
};

struct Timer * use_timer(struct XREContext *ctx) {
    return (struct Timer *) xre_use_ref(ctx, timer_alloc, free);
}


//------------------------------------------------------------------------------


int use_pressed_key(struct XREContext *ctx) {
    struct XREStateInt * last_pressed_state = xre_use_int(ctx, EOF);
    char debouce_time_has_passed = use_time_interval(ctx, KEY_PRESS_DEBOUNCE_S);

    int last_pressed = xre_state_get_int(last_pressed_state);
    int key_c = kbhit();
    ungetc(key_c, stdin);

    if (last_pressed != EOF && !debouce_time_has_passed) {
        return EOF;
    }

    xre_state_set_int(last_pressed_state, key_c);

    return xre_state_get_int(last_pressed_state);
}


//------------------------------------------------------------------------------


double use_fps(struct XREContext * ctx, double interval_s) {

    char has_timer_passed = use_time_interval(ctx, interval_s);
    struct XREStateInt * frame_count_state = xre_use_int(ctx, 0);
    struct XREStateDouble * fps_state = xre_use_double(ctx, 0.0);

    int frame_count = xre_state_get_int(frame_count_state);

    if (has_timer_passed) {
        xre_state_set_double(fps_state, (double) frame_count / interval_s);
        xre_state_set_int(frame_count_state, 0);
    } else {
        frame_count++;
        xre_state_set_int(frame_count_state, frame_count);
    }

    return xre_state_get_double(fps_state);
};


//------------------------------------------------------------------------------


void box_screen_component(struct XREContext * ctx, va_list props) {
    (void) props;

    static double const x_delta = 1.5;
    static double const y_delta = 1.0;

    int pressed_key = use_pressed_key(ctx);

    struct XREStateDouble * pos_x_state = xre_use_double(ctx, 37.5);
    double pos_x = xre_state_get_double(pos_x_state);
    struct XREStateDouble * pos_y_state = xre_use_double(ctx, 4.0);
    double pos_y = xre_state_get_double(pos_y_state);

    struct Box box = {(int) pos_x, (int) pos_y, 4, 2};
    ScreenSize const * screen_size = screen_get_size(screen);

    ScreenCoordinates text_coords = {0, 0};

    if (pressed_key == 'k' && box.y > 0) {
        pos_y -= y_delta;
        xre_state_set_double(pos_y_state, pos_y);
    } else if (pressed_key == 'j' && box.y + box.height < screen_size->rows) {
        pos_y += y_delta;
        xre_state_set_double(pos_y_state, pos_y);
    } else if (pressed_key == 'h' && box.x > 0) {
        pos_x -= x_delta;
        xre_state_set_double(pos_x_state, pos_x);
    } else if (pressed_key == 'l' && box.x + box.width < screen_size->cols) {
        pos_x += x_delta;
        xre_state_set_double(pos_x_state, pos_x);
    }

    draw_box(&box, 'X', 0);

    text_coords.y = screen_size->rows -2;
    screen_printf(screen, &text_coords, "Use h, j, k, l to move");
};


//------------------------------------------------------------------------------


void title_screen_component(struct XREContext * ctx, va_list props) {
    (void) props;
    (void) ctx;

    static char const text[] =
        "This is a screen with some text in it\n"
        "\n"
        "         _______     ____  ____  ________  \n"
        "        |_   __ \\   |_  _||_  _||_   __  | \n"
        "          | |__) |    \\ \\  / /    | |_ \\_| \n"
        "          |  __ /      > `' <     |  _| _  \n"
        "         _| |  \\ \\_  _/ /'`\\ \\_  _| |__/ | \n"
        "        |____| |___||____||____||________| \n"
        "\n"
        "and a title.\n"
        "";

    ScreenCoordinates text_coords = {4, 1};

    for (char const * start_ptr = text; *start_ptr != '\0'; start_ptr++) {
        size_t line_len = 0;
        char const * end_ptr = start_ptr;

        while (*end_ptr != '\0' && *end_ptr != '\n') {
            end_ptr++;
            line_len++;
        };

        char * line_str = (char *) malloc(sizeof(char) * (line_len + 1));
        line_str[line_len] = '\0';

        strncpy(line_str, start_ptr, line_len);

        screen_printf(screen, &text_coords, "%s", line_str);

        free(line_str);

        start_ptr = end_ptr;

        text_coords.y++;
    }

};


//------------------------------------------------------------------------------


void draw_transition(char const * title) {

    size_t title_len = strlen(title);

    ScreenSize const * screen_size = screen_get_size(screen);
    struct Box box = {
        .x = 0,
        .y = screen_size->rows / 2 - 2,
        .width = screen_size->cols,
        .height = 5
    };

    draw_box(&box, ' ', 1);

    ScreenCoordinates text_coords = {
        (screen_size->cols - title_len) / 2,
        screen_size->rows/2
    };
    screen_printf(screen, &text_coords, title);
}


//------------------------------------------------------------------------------


void app(struct XREContext * ctx, va_list props) {

    static size_t const children_count = 3;
    static Component const children[] = {
        title_screen_component,
        box_screen_component,
        box_screen_component,
    };
    static char const * children_titles[] = {
        "Title",
        "Box 1",
        "Box 2"
    };
    static double const TRANSITION_DURATION = 0.5;

    int * should_exit = va_arg(props, int *);

    ScreenSize const * screen_size = screen_get_size(screen);
    double fps = use_fps(ctx, 0.5);
    ScreenCoordinates text_coords = {0, 0};
    int pressed_key = use_pressed_key(ctx);
    struct Timer * timer = use_timer(ctx);

    struct XREStateInt * child_index_state = xre_use_int(ctx, 0);
    int child_index = xre_state_get_int(child_index_state);

    if (pressed_key == 27) {
        *should_exit = 1;
    } else if (pressed_key == 'n') {
        child_index = (child_index + 1) % children_count;

        timer_set(timer, TRANSITION_DURATION);

        xre_state_set_int(child_index_state, child_index);
    } else if (pressed_key == 'p') {
        child_index = (child_index + children_count - 1) % children_count;

        timer_set(timer, TRANSITION_DURATION);

        xre_state_set_int(child_index_state, child_index);
    }

    xre_use_ikey(child_index, children[child_index], ctx);
    if (timer_is_running(timer)) {
        draw_transition(children_titles[child_index]);
    }

    text_coords.x = 0;
    text_coords.y = screen_size->rows -1;
    screen_printf(screen, &text_coords, "Press <ESC> to terminate.");

    text_coords.x = screen_size->cols - 37;
    text_coords.y = screen_size->rows -1;
    screen_printf(screen, &text_coords, "Press 'p', 'n' to move among screens.");

    text_coords.x = 0;
    text_coords.y = 0;
    screen_printf(screen, &text_coords, "FPS: %d", (int)fps);

    kbhit();
};


//------------------------------------------------------------------------------


int main(void) {

    double const SPF = 0.016;
    struct TerminalSize terminal_size = get_terminal_size();
    struct ScreenSize const screen_size = {
        .rows=terminal_size.rows - 2,
        .cols=terminal_size.cols - 2
    };

    kb_init();
    screen_init(&screen_size);
    struct XREContext * root_context = xre_context_root_alloc();

    int exit = 0;
    while (!exit) {
        xre_use_root(app, root_context, &exit);
        screen_render(screen);

        msleep((long int)(1000 * SPF));
    }
    screen_render_clear(screen);

    xre_context_destroy(root_context);
    screen_clean_up();
    kb_clean_up();
};
