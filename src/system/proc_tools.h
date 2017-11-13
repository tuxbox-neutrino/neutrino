/*
 * helper functions to interact with files, usually in the proc filesystem
 *
 * (C) 2012 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>
 *
 * License: GPLv2 or later
 *
 */
#ifndef __PROC_TOOLS_H__
#define __PROC_TOOLS_H__
#ifdef __cplusplus
extern "C" {
#endif
int proc_put(const char *path, const char *value, const int len);
int proc_get(const char *path, char *value, const int len);
unsigned int proc_get_hex(const char *path);
#ifdef __cplusplus
}
#endif
#endif
