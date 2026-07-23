#pragma once

void mkrn_device_tree_init(void);
int mkrn_device_tree_open(const char *path, int flags, int *out_fd);
int mkrn_device_tree_close(int fd);
int mkrn_device_tree_read(int fd, void *buf, uint32_t count);
int mkrn_device_tree_write(int fd, const void *buf, uint32_t count);
int mkrn_device_tree_getdents(int fd, struct mkrn_vfs_dirent *buf, uint32_t max);
int mkrn_device_tree_is_dt_fd(int fd);
