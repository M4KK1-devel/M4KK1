/*
 * M4KK1 4P1 - icons_data.h  [GENERATED - do not edit by hand]
 * Description: externs for the PNG-converted icon bitmap set.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#ifndef ICONS_DATA_H
#define ICONS_DATA_H

#define ICON_DATA_SIZE 32

#include <stdint.h>

extern uint32_t icon_apps_altr[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_apps_backup[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_apps_clock[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_apps_info[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_apps_logview[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_dir[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_dir_config[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_dir_desktop[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_dir_documents[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_dir_downloads[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_dir_home[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_dir_music[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_net_054[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_net_055[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_net_056[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_net_057[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_net_058[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_net_059[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_apps_automission[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_apps_mail[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_apps_user[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_apps_users[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_files_document[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_files_http[ICON_DATA_SIZE * ICON_DATA_SIZE];
extern uint32_t icon_files_loc_file[ICON_DATA_SIZE * ICON_DATA_SIZE];

struct icon_data_entry {
    char *name;
    uint32_t *pixels;
};

extern struct icon_data_entry icon_data_table[];

/*
 * icons_data_find - Look up an icon bitmap by logical name
 * ("dir", "dir_home", "clock", "net_054", ...).
 *
 * Return: pointer to 32x32 ARGB pixels, NULL if unknown name.
 */
uint32_t *icons_data_find(const char *name);

#endif /* ICONS_DATA_H */
