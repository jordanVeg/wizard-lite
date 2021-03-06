#ifndef INCLUDE_TERRAIN_H
#define INCLUDE_TERRAIN_H

#include "collisions.h"
#include "mob.h"
#include "global.h"
#include "mob_handler.h"

#define ID_SIZE         8
#define IMAGE_PATH_SIZE 23

#define DOOR_HEIGHT 128
#define DOOR_WIDTH  32

#define MAX_ROWS 20
#define MAX_COLS 20

#define NUM_ROWS 3
#define NUM_COLS 3

extern int floor_num;

typedef enum {
  BASIC,
  KEY,
  EXIT,
  SHOP,
  EMPTY,
  CHALLENGE
} ROOM_TYPE;

typedef struct room {
    int width, height, row_pos, col_pos;
    char id[ID_SIZE];
    char path_to_map_image[IMAGE_PATH_SIZE];
    ALLEGRO_BITMAP* map;
    ALLEGRO_BITMAP* door;
    ROOM_TYPE type;
    bool is_initialized, is_loaded, is_spawnable, is_locked;
    int room_configuration[4];
    Hitbox north_door, south_door, east_door, west_door;
} Room;

typedef struct floor {
  int number;
  Room map[MAX_ROWS][MAX_COLS];
} Floor;

int load_room(Room* r);

int unload_room(Room* r);

Room* change_rooms(Room map[MAX_ROWS][MAX_COLS], Room* current_room, Mob* p);

void show_room(Room* r);

void generate_floor(Floor* f, int start_row, int start_col);

Room* update_dungeon_state(Floor* floor, Room* room, Mob* player, int num_active_mobs, bool* room_changed);
/* Externally visible for debugging purposes*/
void print_floor(Floor* f);

#endif
