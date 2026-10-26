
#define _POSIX_C_SOURCE 200809L

#include "seatmap.h"
#include "storage.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 8080
#define REQUEST_SIZE 512
#define RESPONSE_SIZE 8192
#define DATA_FILE "reservation_data.bin"

static int send_all(int client_fd, const char *message)
{
    size_t total = 0;
    size_t length = strlen(message);

    while (total < length) {
        ssize_t sent = send(client_fd, message + total,
                            length - total, 0);

        if (sent <= 0) {
            return 0;
        }

        total += (size_t)sent;
    }

    return 1;
}

static int find_trip(Trip trips[], int trip_id)
{
    int i;

    for (i = 0; i < MAX_TRIPS; i++) {
        if (trips[i].trip_id == trip_id) {
            return i;
        }
    }

    return -1;
}

static void append_response(char response[], size_t response_size,
                            size_t *used, const char *line)
{
    int written;

    if (*used >= response_size) {
        return;
    }

    written = snprintf(response + *used, response_size - *used,
                       "%s", line);

    if (written < 0) {
        return;
    }

    if ((size_t)written >= response_size - *used) {
        *used = response_size - 1;
    } else {
        *used += (size_t)written;
    }
}

static void make_trip_list(Trip trips[], char response[],
                           size_t response_size)
{
    char line[256];
    size_t used = 0;
    int i;
    int j;

    response[0] = '\0';

    append_response(response, response_size, &used,
                    "Available trips and seat status:\n");

    for (i = 0; i < MAX_TRIPS; i++) {
        (void)snprintf(line, sizeof line,
                       "\nTrip %d: %s to %s\nDeparture: %s\n",
                       trips[i].trip_id, trips[i].origin,
                       trips[i].destination, trips[i].departure);
        append_response(response, response_size, &used, line);

        for (j = 0; j < SEATS; j++) {
            const Seat *seat = &trips[i].seatmap.seats[j];

            if (seat->status == SEAT_AVAILABLE) {
                (void)snprintf(line, sizeof line,
                               "Seat %d: available\n", seat->seat_id);
            } else if (seat->status == SEAT_HELD) {
                (void)snprintf(line, sizeof line,
                               "Seat %d: held by %s %s\n",
                               seat->seat_id, seat->first_name,
                               seat->last_name);
            } else {
                (void)snprintf(line, sizeof line,
                               "Seat %d: booked by %s %s\n",
                               seat->seat_id, seat->first_name,
                               seat->last_name);
            }

            append_response(response, response_size, &used, line);
        }

        (void)snprintf(line, sizeof line,
                       "Waitlist size: %d\n", trips[i].waitlist.count);
        append_response(response, response_size, &used, line);
    }
}

static int promote_waitlisted_passenger(Trip *trip)
{
    WaitlistEntry passenger;
    int i;

    if (trip->waitlist.count <= 0) {
        return 0;
    }

    for (i = 0; i < SEATS; i++) {
        if (trip->seatmap.seats[i].status == SEAT_AVAILABLE) {
            if (!waitlist_remove_first(&trip->waitlist, &passenger)) {
                return 0;
            }

            if (!seatmap_assign(&trip->seatmap,
                                trip->seatmap.seats[i].seat_id,
                                passenger.first_name,
                                passenger.last_name)) {
                /* Restore the passenger at the end if assignment fails. */
                (void)waitlist_add(&trip->waitlist,
                                   passenger.first_name,
                                   passenger.last_name);
                return 0;
            }

            return trip->seatmap.seats[i].seat_id;
        }
    }

    return 0;
}

static void process_request(const char *request, char response[],
                            size_t response_size)
{
    char command[16];
    char first_name[NAME_LENGTH];
    char last_name[NAME_LENGTH];
    int trip_id = 0;
    int seat_id = 0;
    int trip_index;
    int lock_fd;
    int changed = 0;
    int success = 0;
    int promoted_seat = 0;
    Trip trips[MAX_TRIPS];

    first_name[0] = '\0';
    last_name[0] = '\0';
    response[0] = '\0';

    if (sscanf(request, "%15s", command) != 1) {
        (void)snprintf(response, response_size, "Error: empty request\n");
        return;
    }

    lock_fd = storage_lock_exclusive(DATA_FILE);

    if (lock_fd == -1) {
        (void)snprintf(response, response_size,
                       "Error: Could not lock data\n");
        return;
    }

    if (!storage_load_locked(trips, MAX_TRIPS, DATA_FILE)) {
        (void)snprintf(response, response_size,
                       "Error: Could not load data\n");
        storage_unlock(lock_fd);
        return;
    }

    if (strcmp(command, "LIST") == 0) {
        make_trip_list(trips, response, response_size);
        storage_unlock(lock_fd);
        return;
    }

    if (strcmp(command, "HOLD") == 0) {
        if (sscanf(request, "%15s %d %d %49s %49s",
                   command, &trip_id, &seat_id,
                   first_name, last_name) != 5) {
            (void)snprintf(response, response_size,
                           "Error: Use HOLD_trip_seat_firstname_lastname\n");
            storage_unlock(lock_fd);
            return;
        }
    } else if (strcmp(command, "CONFIRM") == 0 ||
               strcmp(command, "CANCEL") == 0) {
        if (sscanf(request, "%15s %d %d",
                   command, &trip_id, &seat_id) != 3) {
            (void)snprintf(response, response_size,
                           "Error: Use %s trip seat\n", command);
            storage_unlock(lock_fd);
            return;
        }
    } else if (strcmp(command, "WAITLIST") == 0) {
        if (sscanf(request, "%15s %d %49s %49s",
                   command, &trip_id,
                   first_name, last_name) != 4) {
            (void)snprintf(response, response_size,
                           "Error: Use WAITLIST_trip_firstname_lastname\n");
            storage_unlock(lock_fd);
            return;
        }
    } else {
        (void)snprintf(response, response_size,
                       "Error: Unknown command use LIST, Hold, "
                       "CONFIRM, CANCEL, or WAITLIST\n");
        storage_unlock(lock_fd);
        return;
    }

    trip_index = find_trip(trips, trip_id);

    if (trip_index < 0) {
        (void)snprintf(response, response_size,
                       "Error: Trip ID must be between 1 and %d\n",
                       MAX_TRIPS);
        storage_unlock(lock_fd);
        return;
    }

    if (strcmp(command, "HOLD") == 0) {
        success = seatmap_hold(&trips[trip_index].seatmap, seat_id,
                               first_name, last_name);
        changed = success;
    } else if (strcmp(command, "CONFIRM") == 0) {
        success = seatmap_confirm(&trips[trip_index].seatmap, seat_id);
        changed = success;
    } else if (strcmp(command, "CANCEL") == 0) {
        success = seatmap_remove(&trips[trip_index].seatmap, seat_id);
        changed = success;

        if (success) {
            promoted_seat =
                promote_waitlisted_passenger(&trips[trip_index]);
        }
    } else if (strcmp(command, "WAITLIST") == 0) {
        success = waitlist_add(&trips[trip_index].waitlist,
                               first_name, last_name);
        changed = success;
    }

    if (!success) {
        (void)snprintf(response, response_size,
                       "Error: Check the trip, seat "
                       "status, and waitlist capacity\n");
        storage_unlock(lock_fd);
        return;
    }

    if (changed &&
        !storage_save_locked(trips, MAX_TRIPS, DATA_FILE)) {
        (void)snprintf(response, response_size,
                       "Error: change could not be saved\n");
        storage_unlock(lock_fd);
        return;
    }

    if (strcmp(command, "HOLD") == 0) {
        (void)snprintf(response, response_size,
                       "SUCCESS: Seat %d on trip %d is held for %s %s\n",
                       seat_id, trip_id, first_name, last_name);
    } else if (strcmp(command, "CONFIRM") == 0) {
        (void)snprintf(response, response_size,
                       "SUCCESS: Seat %d on trip %d is confirmed\n",
                       seat_id, trip_id);
    } else if (strcmp(command, "CANCEL") == 0) {
        if (promoted_seat > 0) {
            (void)snprintf(response, response_size,
                           "SUCCESS: Seat %d was cancelled. The next "
                           "waitlisted passenger was assigned seat %d\n",
                           seat_id, promoted_seat);
        } else {
            (void)snprintf(response, response_size,
                           "SUCCESS: Seat %d on trip %d was cancelled\n",
                           seat_id, trip_id);
        }
    } else {
        (void)snprintf(response, response_size,
                       "SUCCESS: Passenger %s %s joined trip %d's waitlist\n",
                       first_name, last_name, trip_id);
    }

    storage_unlock(lock_fd);
}

int main(void)
{
    int server_fd;
    int client_fd;
    int reuse = 1;
    struct sockaddr_in server_address;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("Could not create server socket");
        return EXIT_FAILURE;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof reuse) == -1) {
        perror("Could not set socket options");
        close(server_fd);
        return EXIT_FAILURE;
    }

    memset(&server_address, 0, sizeof server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&server_address,
             sizeof server_address) == -1) {
        perror("Could not bind server socket");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 10) == -1) {
        perror("Could not listen on server socket");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("Reservation server listening on port %d\n", SERVER_PORT);
    printf("Press Ctrl+C to stop the server\n");

    for (;;) {
        char request[REQUEST_SIZE];
        char response[RESPONSE_SIZE];
        size_t received = 0;

        client_fd = accept(server_fd, NULL, NULL);

        if (client_fd == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("Could not connect to client");
            continue;
        }

        while (received < sizeof request - 1) {
            ssize_t count = recv(client_fd, request + received,
                                 sizeof request - 1 - received, 0);

            if (count <= 0) {
                break;
            }

            received += (size_t)count;

            if (memchr(request, '\n', received) != NULL) {
                break;
            }
        }

        request[received] = '\0';

        if (received > 0) {
            char *newline = strchr(request, '\n');

            if (newline != NULL) {
                *newline = '\0';
            }

            process_request(request, response, sizeof response);
            (void)send_all(client_fd, response);
        }

        close(client_fd);
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
