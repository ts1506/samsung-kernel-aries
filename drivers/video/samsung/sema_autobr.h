/*  Semaphore Auto Brightness driver
 *  for Samsung Galaxy S I9000
 *
 *   Copyright (c) 2011-2013 stratosk@semaphore.gr
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _SEMA_AUTOBR_H
#define _SEMA_AUTOBR_H

extern int ls_get_adcvalue(void);
extern int bl_update_brightness(int bl);
extern void block_bl_update(void);
extern void unblock_bl_update(void);
extern void block_ls_update(void);
extern void unblock_ls_update(void);

#endif
