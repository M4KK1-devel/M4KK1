/*
 * M4KK1 4P1 - mdm.c
 * Description: M4KK1 Display Manager - Graphical Login
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../lib/libgui.h"

/* Global variables required by m4sh.h functions */
int out_fd = 1;
char cwd[256] = "/";

/* Login form dimensions */
#define FORM_WIDTH      400
#define FORM_HEIGHT     300
#define FORM_X          200
#define FORM_Y          150

#define INPUT_WIDTH     300
#define INPUT_HEIGHT    30
#define INPUT_X         250
#define USERNAME_Y      220
#define PASSWORD_Y      270

#define BUTTON_WIDTH    100
#define BUTTON_HEIGHT   35
#define BUTTON_X        350
#define BUTTON_Y        330

#define TITLE_X         320
#define TITLE_Y         170

/* Input field state */
#define FIELD_USERNAME  0
#define FIELD_PASSWORD  1

static char username[64] = "";
static char password[64] = "";
static int current_field = FIELD_USERNAME;
static int login_button_pressed = 0;

/* Draw the login form */
static void draw_login_form(void) {
    /* Draw gradient background */
    gui_draw_gradient(0x002040A0, 0x00102050);
    
    /* Draw form background */
    gui_draw_rect(FORM_X, FORM_Y, FORM_WIDTH, FORM_HEIGHT, GUI_COLOR_WHITE);
    gui_draw_rect(FORM_X + 2, FORM_Y + 2, FORM_WIDTH - 4, FORM_HEIGHT - 4, GUI_COLOR_LIGHT);
    
    /* Draw title */
    gui_draw_text(TITLE_X, TITLE_Y, "M4KK1 Login", GUI_COLOR_BLACK, GUI_COLOR_LIGHT);
    
    /* Draw username field */
    gui_draw_text(INPUT_X, USERNAME_Y - 20, "Username:", GUI_COLOR_BLACK, GUI_COLOR_LIGHT);
    gui_draw_input_box(INPUT_X, USERNAME_Y, INPUT_WIDTH, INPUT_HEIGHT, 
                       username, current_field == FIELD_USERNAME);
    
    /* Draw password field */
    gui_draw_text(INPUT_X, PASSWORD_Y - 20, "Password:", GUI_COLOR_BLACK, GUI_COLOR_LIGHT);
    
    /* Mask password with asterisks */
    char masked[64] = "";
    int len = musr_strlen(password);
    for (int i = 0; i < len && i < 63; i++) {
        masked[i] = '*';
    }
    masked[len] = '\0';
    
    gui_draw_input_box(INPUT_X, PASSWORD_Y, INPUT_WIDTH, INPUT_HEIGHT,
                       masked, current_field == FIELD_PASSWORD);
    
    /* Draw login button */
    gui_draw_button(BUTTON_X, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, 
                    "Login", login_button_pressed);
    
    /* Present to screen */
    gui_flip();
}

/* Cached passwd entry from last successful lookup */
static passwd_entry_t cached_entry;
static int cached_entry_valid = 0;

/* Authenticate user via passwd.db */
static int authenticate_user(void) {
    cached_entry_valid = 0;

    /* Look up user in passwd.db */
    if (musr_getpwnam(username, &cached_entry) != 0) {
        return -1;  /* User not found */
    }

    /* Verify password against stored hash */
    if (musr_verify_password(password, cached_entry.password_hash) == 0) {
        return -1;  /* Wrong password */
    }

    cached_entry_valid = 1;
    return 0;  /* Success */
}

/* Launch shell after successful login */
static void launch_shell(void) {
    if (!cached_entry_valid)
        return;

    /* Set user identity */
    m4k_setuid(cached_entry.uid);
    m4k_setgid(cached_entry.gid);
    m4k_chdir(cached_entry.home);

    /* Register session */
    m4k_register_session("tty0", m4k_getpid(), username);

    /* Launch shell */
    m4k_spawn("/bin/m4sh", 0);
}

/* Handle keyboard input */
static void handle_keyboard(void) {
    struct m4k_keyboard_event ev;
    
    if (gui_get_keyboard_event(&ev) > 0) {
        char ch = ev.ascii_char;
        
        if (ch == '\t') {
            /* Tab: switch fields */
            current_field = (current_field == FIELD_USERNAME) ? 
                           FIELD_PASSWORD : FIELD_USERNAME;
        } else if (ch == '\r' || ch == '\n') {
            /* Enter: try login */
            if (authenticate_user() == 0) {
                launch_shell();
                /* If spawn returns, clear password and retry */
                password[0] = '\0';
            } else {
                /* Login failed, clear password */
                password[0] = '\0';
            }
            draw_login_form();
        } else if (ch == '\b' || ch == 0x7F) {
            /* Backspace: delete last character */
            char *field = (current_field == FIELD_USERNAME) ? username : password;
            int len = musr_strlen(field);
            if (len > 0) {
                field[len - 1] = '\0';
                draw_login_form();
            }
        } else if (ch >= 0x20 && ch < 0x7F) {
            /* Printable character: add to current field */
            char *field = (current_field == FIELD_USERNAME) ? username : password;
            int len = musr_strlen(field);
            if (len < 63) {
                field[len] = ch;
                field[len + 1] = '\0';
                draw_login_form();
            }
        }
    }
}

/* Draw mouse cursor */
static void draw_mouse_cursor(int x, int y) {
    /* 简单的箭头光标 */
    static const uint8_t cursor_pattern[] = {
        0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF,
        0xF8, 0xDC, 0xCC, 0x86, 0x06, 0x03, 0x03, 0x01
    };
    
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            if (cursor_pattern[row] & (0x80 >> col)) {
                /* 绘制黑色像素 */
                gui_draw_rect(x + col, y + row, 1, 1, 0x000000);
            }
        }
    }
}

/* Handle mouse input */
static void handle_mouse(void) {
    struct m4k_mouse_event ev;
    static int mouse_x = 400, mouse_y = 300;
    
    if (gui_get_mouse_event(&ev) > 0) {
        mouse_x += ev.dx;
        mouse_y += ev.dy;
        
        /* Clamp to screen bounds */
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= 800) mouse_x = 799;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= 600) mouse_y = 599;
        
        /* Check button press */
        if (ev.buttons & 1) {
            if (gui_point_in_rect(mouse_x, mouse_y, 
                                  BUTTON_X, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT)) {
                login_button_pressed = 1;
                draw_login_form();
                
                /* Try login */
                if (authenticate_user() == 0) {
                    launch_shell();
                    password[0] = '\0';
                } else {
                    password[0] = '\0';
                }
                
                login_button_pressed = 0;
                draw_login_form();
            }
            
            /* Check field clicks */
            if (gui_point_in_rect(mouse_x, mouse_y,
                                  INPUT_X, USERNAME_Y, INPUT_WIDTH, INPUT_HEIGHT)) {
                current_field = FIELD_USERNAME;
                draw_login_form();
            }
            if (gui_point_in_rect(mouse_x, mouse_y,
                                  INPUT_X, PASSWORD_Y, INPUT_WIDTH, INPUT_HEIGHT)) {
                current_field = FIELD_PASSWORD;
                draw_login_form();
            }
        }
    }
    
    /* 绘制鼠标光标 */
    draw_mouse_cursor(mouse_x, mouse_y);
}

/* Main entry point */
void _start(void) {
    /* Debug: output to serial */
    ser_puts("[MDM] Starting M4KK1 Display Manager...\n");
    
    /* Test framebuffer info */
    struct m4k_framebuffer_info fb;
    int ret = m4k_get_framebuffer_info(&fb);
    ser_puts("[MDM] m4k_get_framebuffer_info returned: ");
    print_u32((uint32_t)ret);
    ser_puts("\n");
    if (ret == 0) {
        ser_puts("[MDM] FB struct fields:\n");
        ser_puts("  phys_addr = 0x");
        print_u32(fb.phys_addr);
        ser_puts("\n");
        ser_puts("  width = ");
        print_u32(fb.width);
        ser_puts("\n");
        ser_puts("  height = ");
        print_u32(fb.height);
        ser_puts("\n");
        ser_puts("  bpp = ");
        print_u32(fb.bpp);
        ser_puts("\n");
        ser_puts("  pitch = ");
        print_u32(fb.pitch);
        ser_puts("\n");
    } else {
        ser_puts("[MDM] ERROR: Cannot get framebuffer info!\n");
    }
    
    /* Test simple draw */
    ser_puts("[MDM] Testing m4k_draw_rect...\n");
    ret = m4k_draw_rect(0, 0, 800, 600, 0x00FF0000);  /* Red screen */
    ser_puts("[MDM] m4k_draw_rect returned: ");
    print_u32((uint32_t)ret);
    ser_puts("\n");
    
    /* Test flip */
    ser_puts("[MDM] Testing m4k_flip...\n");
    ret = m4k_flip();
    ser_puts("[MDM] m4k_flip returned: ");
    print_u32((uint32_t)ret);
    ser_puts("\n");
    
    /* Initialize display */
    ser_puts("[MDM] Drawing login form...\n");
    draw_login_form();
    ser_puts("[MDM] Login form drawn, entering event loop\n");
    
    /* Main event loop */
    for (;;) {
        handle_keyboard();
        handle_mouse();
        
        /* Small delay to prevent busy-waiting */
        for (volatile int i = 0; i < 100000; i++);
    }
}
