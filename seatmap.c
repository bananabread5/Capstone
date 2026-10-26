#include "seatmap.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void clear_input_line(void)
{
    int ch;

    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
}

static int read_line(const char *prompt, char *buffer, size_t size)
{
    size_t length;

    printf("%s", prompt);

    if (fgets(buffer, (int)size, stdin) == NULL) {
        return 0;
    }

    length = strlen(buffer);

    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    } else {
        clear_input_line();
        printf("Input is too long. Please try again.\n");
        return 0;
    }

    if (buffer[0] == '\0') {
        printf("Input cannot be empty.\n");
        return 0;
    }

    return 1;
}

static int read_seat_id(int *seat_id)
{
    char line[64];
    char *end;
    long value;

    printf("Enter seat number (1-%d), or 0 to cancel: ", SEATS);

    if (fgets(line, sizeof line, stdin) == NULL) {
        return 0;
    }

    value = strtol(line, &end, 10);

    if (end == line) {
        printf("Please enter a number.\n");
        return 0;
    }

    while (isspace((unsigned char)*end)) {
        end++;
    }

    if (*end != '\0') {
        printf("Please enter only a number.\n");
        return 0;
    }

    if (value == 0) {
        return 0;
    }

    if (value < 1 || value > SEATS) {
        printf("Invalid seat number.\n");
        return 0;
    }

    *seat_id = (int)value;
    return 1;
}

void seatmap_initialize(SeatMap *map)
{
    int i;

    if (map == NULL) {
        return;
    }

    for (i = 0; i < SEATS; i++) {
        map->seats[i].seat_id = i + 1;
        map->seats[i].status = SEAT_AVAILABLE;
        map->seats[i].first_name[0] = '\0';
        map->seats[i].last_name[0] = '\0';
    }
}

int seatmap_validate(const SeatMap *map)
{
    int i;

    if (map == NULL) {
        return 0;
    }

    for (i = 0; i < SEATS; i++) {
        const Seat *seat = &map->seats[i];

        if (seat->seat_id != i + 1 ||
            seat->status < SEAT_AVAILABLE ||
            seat->status > SEAT_BOOKED ||
            memchr(seat->first_name, '\0', NAME_LENGTH) == NULL ||
            memchr(seat->last_name, '\0', NAME_LENGTH) == NULL) {
            return 0;
        }

        if (seat->status == SEAT_AVAILABLE &&
            (seat->first_name[0] != '\0' ||
             seat->last_name[0] != '\0')) {
            return 0;
        }

        if (seat->status != SEAT_AVAILABLE &&
            (seat->first_name[0] == '\0' ||
             seat->last_name[0] == '\0')) {
            return 0;
        }
    }

    return 1;
}

int seatmap_empty_count(const SeatMap *map)
{
    int i;
    int count = 0;

    if (map == NULL) {
        return 0;
    }

    for (i = 0; i < SEATS; i++) {
        if (map->seats[i].status == SEAT_AVAILABLE) {
            count++;
        }
    }

    return count;
}

void seatmap_print_empty(const SeatMap *map)
{
    int i;

    if (map == NULL) {
        return;
    }

    printf("Available seats: ");

    for (i = 0; i < SEATS; i++) {
        if (map->seats[i].status == SEAT_AVAILABLE) {
            printf("%d ", map->seats[i].seat_id);
        }
    }

    printf("\nAvailable seat count: %d\n",
           seatmap_empty_count(map));
}

void seatmap_print_passengers(const SeatMap *map)
{
    int order[SEATS];
    int count = 0;
    int i;
    int j;

    if (map == NULL) {
        return;
    }

    for (i = 0; i < SEATS; i++) {
        if (map->seats[i].status == SEAT_BOOKED) {
            order[count++] = i;
        }
    }

    for (i = 0; i < count - 1; i++) {
        for (j = i + 1; j < count; j++) {
            if (strcmp(map->seats[order[i]].last_name,
                       map->seats[order[j]].last_name) > 0) {
                int temp = order[i];
                order[i] = order[j];
                order[j] = temp;
            }
        }
    }

    printf("Confirmed passengers (alphabetical by last name):\n");

    if (count == 0) {
        printf("No confirmed passengers.\n");
    }

    for (i = 0; i < count; i++) {
        const Seat *seat = &map->seats[order[i]];

        printf("%s, %s - Seat %d\n",
               seat->last_name,
               seat->first_name,
               seat->seat_id);
    }
}

int seatmap_hold(SeatMap *map, int seat_id,
                 const char *first_name, const char *last_name)
{
    Seat *seat;

    if (map == NULL || first_name == NULL || last_name == NULL ||
        seat_id < 1 || seat_id > SEATS ||
        first_name[0] == '\0' || last_name[0] == '\0' ||
        strlen(first_name) >= NAME_LENGTH ||
        strlen(last_name) >= NAME_LENGTH) {
        return 0;
    }

    seat = &map->seats[seat_id - 1];

    if (seat->status != SEAT_AVAILABLE) {
        return 0;
    }

    strcpy(seat->first_name, first_name);
    strcpy(seat->last_name, last_name);
    seat->status = SEAT_HELD;

    return 1;
}

int seatmap_confirm(SeatMap *map, int seat_id)
{
    if (map == NULL || seat_id < 1 || seat_id > SEATS) {
        return 0;
    }

    if (map->seats[seat_id - 1].status != SEAT_HELD) {
        return 0;
    }

    map->seats[seat_id - 1].status = SEAT_BOOKED;

    return 1;
}

int seatmap_assign(SeatMap *map, int seat_id,
                   const char *first_name, const char *last_name)
{
    if (!seatmap_hold(map, seat_id, first_name, last_name)) {
        return 0;
    }

    return seatmap_confirm(map, seat_id);
}

int seatmap_remove(SeatMap *map, int seat_id)
{
    Seat *seat;

    if (map == NULL || seat_id < 1 || seat_id > SEATS) {
        return 0;
    }

    seat = &map->seats[seat_id - 1];

    if (seat->status == SEAT_AVAILABLE) {
        return 0;
    }

    seat->status = SEAT_AVAILABLE;
    seat->first_name[0] = '\0';
    seat->last_name[0] = '\0';

    return 1;
}

int waitlist_add(Waitlist *waitlist,
                 const char *first_name, const char *last_name)
{
    WaitlistEntry *entry;

    if (waitlist == NULL || first_name == NULL || last_name == NULL ||
        first_name[0] == '\0' || last_name[0] == '\0' ||
        strlen(first_name) >= NAME_LENGTH ||
        strlen(last_name) >= NAME_LENGTH ||
        waitlist->count < 0 ||
        waitlist->count >= WAITLIST_CAPACITY) {
        return 0;
    }

    entry = &waitlist->entries[waitlist->count];

    strcpy(entry->first_name, first_name);
    strcpy(entry->last_name, last_name);

    waitlist->count++;

    return 1;
}

void waitlist_print(const Waitlist *waitlist)
{
    int i;

    if (waitlist == NULL || waitlist->count == 0) {
        printf("Waitlist is empty.\n");
        return;
    }

    printf("Waitlist (first in, first out):\n");

    for (i = 0; i < waitlist->count; i++) {
        printf("%d. %s %s\n",
               i + 1,
               waitlist->entries[i].first_name,
               waitlist->entries[i].last_name);
    }
}

int waitlist_remove_first(Waitlist *waitlist, WaitlistEntry *entry)
{
    int i;

    if (waitlist == NULL || entry == NULL || waitlist->count <= 0) {
        return 0;
    }

    *entry = waitlist->entries[0];

    for (i = 1; i < waitlist->count; i++) {
        waitlist->entries[i - 1] = waitlist->entries[i];
    }

    waitlist->count--;

    waitlist->entries[waitlist->count].first_name[0] = '\0';
    waitlist->entries[waitlist->count].last_name[0] = '\0';

    return 1;
}

static void promote_waitlisted_passenger(Trip *trip)
{
    WaitlistEntry entry;
    int seat_id;

    if (trip == NULL || trip->waitlist.count == 0) {
        return;
    }

    for (seat_id = 1; seat_id <= SEATS; seat_id++) {
        if (trip->seatmap.seats[seat_id - 1].status ==
            SEAT_AVAILABLE) {
            break;
        }
    }

    if (seat_id > SEATS) {
        return;
    }

    if (waitlist_remove_first(&trip->waitlist, &entry) &&
        seatmap_hold(&trip->seatmap, seat_id,
                     entry.first_name, entry.last_name)) {
        seatmap_confirm(&trip->seatmap, seat_id);

        printf("Waitlist promotion: %s %s was booked into seat %d.\n",
               entry.first_name, entry.last_name, seat_id);
    }
}

void seatmap_menu(Trip *trip)
{
    char line[64];
    char first_name[NAME_LENGTH];
    char last_name[NAME_LENGTH];
    int seat_id;

    if (trip == NULL) {
        return;
    }

    for (;;) {
        printf("\nTrip %d: %s -> %s | Departure: %s\n",
               trip->trip_id,
               trip->origin,
               trip->destination,
               trip->departure);

        printf("A) Show availability\n");
        printf("B) Show available seat numbers\n");
        printf("C) List confirmed passengers\n");
        printf("D) Hold a seat\n");
        printf("E) Confirm a held seat\n");
        printf("F) Cancel a seat hold/booking\n");
        printf("G) Join waitlist\n");
        printf("H) View waitlist\n");
        printf("I) Return to trip list\n");
        printf("Choose an option: ");

        if (fgets(line, sizeof line, stdin) == NULL) {
            return;
        }

        switch (tolower((unsigned char)line[0])) {
        case 'a': {
            int i;
            int held = 0;
            int booked = 0;

            for (i = 0; i < SEATS; i++) {
                if (trip->seatmap.seats[i].status == SEAT_HELD) {
                    held++;
                }

                if (trip->seatmap.seats[i].status == SEAT_BOOKED) {
                    booked++;
                }
            }

            printf("Available: %d | Held: %d | Booked: %d | Total: %d\n",
                   seatmap_empty_count(&trip->seatmap),
                   held,
                   booked,
                   SEATS);
            break;
        }

        case 'b':
            seatmap_print_empty(&trip->seatmap);
            break;

        case 'c':
            seatmap_print_passengers(&trip->seatmap);
            break;

        case 'd':
            if (!read_seat_id(&seat_id)) {
                break;
            }

            if (!read_line("Enter first name: ",
                           first_name, sizeof first_name) ||
                !read_line("Enter last name: ",
                           last_name, sizeof last_name)) {
                break;
            }

            if (seatmap_hold(&trip->seatmap, seat_id,
                             first_name, last_name)) {
                printf("Seat %d is held. Select E to confirm it.\n",
                       seat_id);
            } else {
                printf("Hold failed: seat unavailable or name invalid.\n");
            }
            break;

        case 'e':
            if (!read_seat_id(&seat_id)) {
                break;
            }

            if (seatmap_confirm(&trip->seatmap, seat_id)) {
                printf("Seat %d booking confirmed.\n", seat_id);
            } else {
                printf("Confirmation failed: seat is not held.\n");
            }
            break;

        case 'f':
            if (!read_seat_id(&seat_id)) {
                break;
            }

            if (seatmap_remove(&trip->seatmap, seat_id)) {
                printf("Seat released.\n");
                promote_waitlisted_passenger(trip);
            } else {
                printf("Cancellation failed: seat is already available.\n");
            }
            break;

        case 'g':
            if (!read_line("Enter first name: ",
                           first_name, sizeof first_name) ||
                !read_line("Enter last name: ",
                           last_name, sizeof last_name)) {
                break;
            }

            if (waitlist_add(&trip->waitlist,
                             first_name, last_name)) {
                printf("Added to the waitlist at position %d.\n",
                       trip->waitlist.count);
            } else {
                printf("Could not join the waitlist: it may be full or the name invalid.\n");
            }
            break;

        case 'h':
            waitlist_print(&trip->waitlist);
            break;

        case 'i':
            return;

        default:
            printf("Choose A through I.\n");
            break;
        }
    }
}

void trips_initialize(Trip trips[], int trip_count)
{
    static const char *origins[] = {
        "Minneapolis", "St. Paul", "Rochester"
    };

    static const char *destinations[] = {
        "Chicago", "Duluth", "Minneapolis"
    };

    static const char *departures[] = {
        "2026-11-01 08:00",
        "2026-11-02 10:30",
        "2026-11-03 14:15"
    };

    int i;

    if (trips == NULL) {
        return;
    }

    for (i = 0; i < trip_count; i++) {
        trips[i].trip_id = i + 1;

        snprintf(trips[i].origin, sizeof trips[i].origin,
                 "%s", origins[i % 3]);

        snprintf(trips[i].destination, sizeof trips[i].destination,
                 "%s", destinations[i % 3]);

        snprintf(trips[i].departure, sizeof trips[i].departure,
                 "%s", departures[i % 3]);

        seatmap_initialize(&trips[i].seatmap);

        memset(&trips[i].waitlist, 0, sizeof trips[i].waitlist);
    }
}

int trip_validate(const Trip *trip)
{
    int i;

    if (trip == NULL ||
        trip->trip_id <= 0 ||
        memchr(trip->origin, '\0', sizeof trip->origin) == NULL ||
        memchr(trip->destination, '\0', sizeof trip->destination) == NULL ||
        memchr(trip->departure, '\0', sizeof trip->departure) == NULL ||
        trip->origin[0] == '\0' ||
        trip->destination[0] == '\0' ||
        trip->departure[0] == '\0' ||
        !seatmap_validate(&trip->seatmap) ||
        trip->waitlist.count < 0 ||
        trip->waitlist.count > WAITLIST_CAPACITY) {
        return 0;
    }

    for (i = 0; i < trip->waitlist.count; i++) {
        if (memchr(trip->waitlist.entries[i].first_name,
                   '\0', NAME_LENGTH) == NULL ||
            memchr(trip->waitlist.entries[i].last_name,
                   '\0', NAME_LENGTH) == NULL ||
            trip->waitlist.entries[i].first_name[0] == '\0' ||
            trip->waitlist.entries[i].last_name[0] == '\0') {
            return 0;
        }
    }

    return 1;
}