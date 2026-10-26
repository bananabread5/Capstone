#include "seatmap.h"
#include "storage.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#define DATA_FILE "reservation_data.bin"

static int read_trip_choice(void)
{
    char line[64];
    char *end;
    long choice;

    printf("Choose a trip number or 0 to quit: ");

    if (fgets(line, sizeof line, stdin) == NULL) {
        return -1;
    }

    choice = strtol(line, &end, 10);

    if (end == line) {
        printf("Please enter a number\n");
        return -2;
    }

    while (isspace((unsigned char)*end)) {
        end++;
    }

    if (*end != '\0') {
        printf("Please enter only a number\n");
        return -2;
    }

    if (choice == 0) {
        return 0;
    }

    if (choice < 1 || choice > MAX_TRIPS) {
        printf("Invalid trip number\n");
        return -2;
    }

    return (int)choice;
}

static void print_trips(const Trip trips[])
{
    int i;

    printf("\nAvailable Trips\n");
    printf("---------------------------------------------------------------\n");

    for (i = 0; i < MAX_TRIPS; i++) {
        int held = 0;
        int booked = 0;
        int j;

        for (j = 0; j < SEATS; j++) {
            if (trips[i].seatmap.seats[j].status == SEAT_HELD) {
                held++;
            }

            if (trips[i].seatmap.seats[j].status == SEAT_BOOKED) {
                booked++;
            }
        }

        printf("%d) %s -> %s | %s | Available %d, Held %d, Booked %d\n",
               trips[i].trip_id,
               trips[i].origin,
               trips[i].destination,
               trips[i].departure,
               seatmap_empty_count(&trips[i].seatmap),
               held,
               booked);
    }
}

int main(void)
{
    Trip trips[MAX_TRIPS];
    int running = 1;

    if (!storage_load(trips, MAX_TRIPS, DATA_FILE)) {
        fprintf(stderr,
                "Could not load data, using default trips");
        trips_initialize(trips, MAX_TRIPS);
    }

    while (running) {
        int choice;

        print_trips(trips);
        choice = read_trip_choice();

        if (choice == -1 || choice == 0) {
            running = 0;
        } else if (choice > 0) {
            seatmap_menu(&trips[choice - 1]);

            if (!storage_save(trips, MAX_TRIPS, DATA_FILE)) {
                fprintf(stderr, "Warning: changes might not be saved\n");
            }
        }
    }

    if (!storage_save(trips, MAX_TRIPS, DATA_FILE)) {
        fprintf(stderr, "Failed to save data\n");
        return EXIT_FAILURE;
    }

    printf("Data saved\n");

    return EXIT_SUCCESS;
}