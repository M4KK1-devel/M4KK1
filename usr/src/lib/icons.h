/*
 * M4KK1 4P1 - icons.h
 * Description: 32x32 ARGB icon set declarations
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#ifndef ICONS_H
#define ICONS_H

#define ICON_SIZE 32

extern uint32_t icon_folder[ICON_SIZE * ICON_SIZE];
extern uint32_t icon_file[ICON_SIZE * ICON_SIZE];
extern uint32_t icon_text[ICON_SIZE * ICON_SIZE];
extern uint32_t icon_code[ICON_SIZE * ICON_SIZE];
extern uint32_t icon_terminal[ICON_SIZE * ICON_SIZE];
extern uint32_t icon_gear[ICON_SIZE * ICON_SIZE];
extern uint32_t icon_launchpad[ICON_SIZE * ICON_SIZE];
extern uint32_t icon_window[ICON_SIZE * ICON_SIZE];
extern uint32_t icon_error[ICON_SIZE * ICON_SIZE];

void icons_init(void);

#endif /* ICONS_H */
