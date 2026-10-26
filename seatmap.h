
#ifndef SEATMAP_H
#define SEATMAP_H

#define SEATS 24
#define NAME_LENGTH 50
#define MAX_TRIPS 3
#define TRIP_TEXT_LENGTH 64
#define WAITLIST_CAPACITY 24

typedef enum {
    SEAT_AVAILABLE = 0,
    SEAT_HELD = 1,
    SEAT_BOOKED = 2
} SeatStatus;

typedef struct {
    int seat_id;
    SeatStatus status;
    char first_name[NAME_LENGTH];
    char last_name[NAME_LENGTH];
} Seat;

typedef struct {
    char first_name[NAME_LENGTH];
    char last_name[NAME_LENGTH];
} WaitlistEntry;

typedef struct {
    WaitlistEntry entries[WAITLIST_CAPACITY];
    int count;
} Waitlist;

typedef struct {
    Seat seats[SEATS];
} SeatMap;

typedef struct {
    int trip_id;
    char origin[TRIP_TEXT_LENGTH];
    char destination[TRIP_TEXT_LENGTH];
    char departure[TRIP_TEXT_LENGTH];
    SeatMap seatmap;
    Waitlist waitlist;
} Trip;

void seatmap_initialize(SeatMap *map);
int seatmap_validate(const SeatMap *map);
int seatmap_empty_count(const SeatMap *map);
void seatmap_print_empty(const SeatMap *map);
void seatmap_print_passengers(const SeatMap *map);

int seatmap_hold(SeatMap *map, int seat_id,
                 const char *first_name, const char *last_name);

int seatmap_confirm(SeatMap *map, int seat_id);

int seatmap_assign(SeatMap *map, int seat_id,
                   const char *first_name, const char *last_name);

int seatmap_remove(SeatMap *map, int seat_id);
void seatmap_menu(Trip *trip);

void trips_initialize(Trip trips[], int trip_count);
int trip_validate(const Trip *trip);

int waitlist_add(Waitlist *waitlist,
                 const char *first_name, const char *last_name);

void waitlist_print(const Waitlist *waitlist);

int waitlist_remove_first(Waitlist *waitlist, WaitlistEntry *entry);

#endif
