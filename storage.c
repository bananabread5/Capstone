
#define _POSIX_C_SOURCE 200809L
#include "storage.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

static int lock_file(const char *filename, int operation)
{
    char lockname[256];
    int lock_fd;

    if (snprintf(lockname, sizeof lockname, "%s.lock", filename)
        >= (int)sizeof lockname) {
        errno = ENAMETOOLONG;
        return -1;
    }

    lock_fd = open(lockname, O_CREAT | O_RDWR, 0666);
    if (lock_fd == -1) {
        return -1;
    }

    if (flock(lock_fd, operation) == -1) {
        close(lock_fd);
        return -1;
    }

    return lock_fd;
}

void storage_unlock(int lock_fd)
{
    if (lock_fd >= 0) {
        (void)flock(lock_fd, LOCK_UN);
        (void)close(lock_fd);
    }
}

int storage_lock_exclusive(const char *filename)
{
    if (filename == NULL) {
        errno = EINVAL;
        return -1;
    }

    return lock_file(filename, LOCK_EX);
}

//Load data while the user holds the file lock
int storage_load_locked(Trip trips[], int trip_count,
                        const char *filename)
{
    FILE *file;
    size_t expected_count;
    int i;

    if (trips == NULL || trip_count <= 0 || filename == NULL) {
        return 0;
    }

    file = fopen(filename, "rb");

    if (file == NULL && errno == ENOENT) {
        trips_initialize(trips, trip_count);
        return 1;
    }

    if (file == NULL) {
        perror("Unable to open reservation data");
        return 0;
    }

    expected_count = fread(trips, sizeof *trips,
                           (size_t)trip_count, file);

    if (expected_count != (size_t)trip_count ||
        fgetc(file) != EOF || ferror(file)) {
        fprintf(stderr, "Reservation data has an unexpected size\n");
        (void)fclose(file);
        trips_initialize(trips, trip_count);
        return 0;
    }

    if (fclose(file) != 0) {
        perror("Unable to close reservation data");
        return 0;
    }

    for (i = 0; i < trip_count; i++) {
        if (!trip_validate(&trips[i]) ||
            trips[i].trip_id != i + 1) {
            fprintf(stderr, "Reservation data failed validation\n");
            trips_initialize(trips, trip_count);
            return 0;
        }
    }

    return 1;
}

//Save data while the user already holds the file lock
int storage_save_locked(const Trip trips[], int trip_count,
                        const char *filename)
{
    char tempname[256];
    FILE *file;
    int success = 1;
    int i;

    if (trips == NULL || trip_count <= 0 || filename == NULL) {
        return 0;
    }

    for (i = 0; i < trip_count; i++) {
        if (!trip_validate(&trips[i]) ||
            trips[i].trip_id != i + 1) {
            fprintf(stderr, "Unable to save invalid trip data\n");
            return 0;
        }
    }

    if (snprintf(tempname, sizeof tempname, "%s.tmp", filename)
        >= (int)sizeof tempname) {
        errno = ENAMETOOLONG;
        perror("Temporary filename too long");
        return 0;
    }

    file = fopen(tempname, "wb");
    if (file == NULL) {
        perror("Unable to create temporary reservation data");
        return 0;
    }

    if (fwrite(trips, sizeof *trips, (size_t)trip_count, file)
        != (size_t)trip_count) {
        perror("Unable to write reservation data");
        success = 0;
    }

    if (fclose(file) != 0) {
        perror("Unable to close temporary reservation data");
        success = 0;
    }

    if (success && rename(tempname, filename) != 0) {
        perror("Unable to replace reservation data");
        success = 0;
    }

    if (!success) {
        (void)remove(tempname);
    }

    return success;
}

/* Regular load: acquire a shared lock for this operation. */
int storage_load(Trip trips[], int trip_count, const char *filename)
{
    int lock_fd;
    int success;

    if (trips == NULL || trip_count <= 0 || filename == NULL) {
        return 0;
    }

    lock_fd = lock_file(filename, LOCK_SH);
    if (lock_fd == -1) {
        perror("Unable to lock reservation file");
        return 0;
    }

    success = storage_load_locked(trips, trip_count, filename);
    storage_unlock(lock_fd);

    return success;
}

/* Regular save: acquire an exclusive lock for this operation. */
int storage_save(const Trip trips[], int trip_count, const char *filename)
{
    int lock_fd;
    int success;

    if (trips == NULL || trip_count <= 0 || filename == NULL) {
        return 0;
    }

    lock_fd = lock_file(filename, LOCK_EX);
    if (lock_fd == -1) {
        perror("Unable to lock reservation file");
        return 0;
    }

    success = storage_save_locked(trips, trip_count, filename);
    storage_unlock(lock_fd);

    return success;
}
