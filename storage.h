
#ifndef STORAGE_H
#define STORAGE_H

#include "seatmap.h"

int storage_load(Trip trips[], int trip_count, const char *filename);
int storage_save(const Trip trips[], int trip_count, const char *filename);

int storage_lock_exclusive(const char *filename);

int storage_load_locked(Trip trips[], int trip_count,
                        const char *filename);

int storage_save_locked(const Trip trips[], int trip_count,
                        const char *filename);

void storage_unlock(int lock_fd);

#endif
