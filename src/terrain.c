/* Standard Libraries */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

/* Allegro Libraries */
#include <allegro5/allegro5.h>              /* Base Allegro library */
#include <allegro5/allegro_font.h>          /* Allegro Font library */
#include <allegro5/allegro_image.h>         /* Allegro Image library */
#include <allegro5/allegro_primitives.h>    /* Allegro Primatives library */

#include "terrain.h"
#include "random.h"

#define MIN_SUBGRAPH_SIZE 2

Room default_room() {
  Room room = {
    .width              = -1,                   /* width */
    .height             = -1,                   /* height */
    .row_pos            = -1,                   /* row position */
    .col_pos            = -1,                   /* column position */
    .id                 = {""},                 /* id string */
    .texture_map        = {{0}},                /* texture map */
    .door               = NULL,                 /* ALLEGRO_BITMAP* door */
    .type               = R_DEFAULT,            /* room type */
    .is_initialized     = false,                /* is_initialized */
    .is_loaded          = false,                /* is_loaded */
    .is_spawnable       = false,                /* is_spawnable */
    .is_locked          = false,                /* is_locked */
    .room_configuration = {0},                  /* in room_configuration */
    .north_door         = default_hitbox(),     /* north_door hitbox */
    .south_door         = default_hitbox(),     /* south_door hitbox */
    .east_door          = default_hitbox(),     /* east_door hitbox */
    .west_door          = default_hitbox(),     /* west_door hitbox */
    .m_handler_p        = NULL                  /* mob handler pointer */
  };
  return room;
}
/*
 *******************************************************************************
 * Internally Visible Variables
 *******************************************************************************
*/
Mob_Handler current_mh;

/*
 *******************************************************************************
 * Internally Visible Functions
 *******************************************************************************
*/
Room generate_room(int row_pos, int col_pos, Room_Type type) {
    Room r = {
      .width          = 1280, //SCREEN_WIDTH,
      .height         = 960, //SCREEN_HEIGHT,
      .row_pos        = row_pos,
      .col_pos        = col_pos,
      .door           = NULL,
      .type           = type,
      .is_initialized = true,
      .is_loaded      = false,
      .is_spawnable   = true,
      .is_locked      = true,
      .north_door     = default_hitbox(),
      .south_door     = default_hitbox(),
      .east_door      = default_hitbox(),
      .west_door      = default_hitbox()
    };

    /* Generate Texture Map */
    /* TODO: smarter algorithm for generating different size/shaped rooms. */
    int selected_texture = 0;
    for(int i = 0; i < MAX_ROOM_WIDTH_IDX; i++) {
      for(int j = 0; j < MAX_ROOM_HEIGHT_IDX; j++) {
        if (i == 0) {
          switch(j) {
            case 0:
              selected_texture = 8;
              break;
            case MAX_ROOM_HEIGHT_IDX-1:
              selected_texture = 10;
              break;
            default:
              selected_texture = 4;
              break;
          }
        } else if (i == MAX_ROOM_WIDTH_IDX-1) {
          switch(j) {
            case 0:
              selected_texture = 9;
              break;
            case MAX_ROOM_HEIGHT_IDX-1:
              selected_texture = 11;
              break;
            default:
              selected_texture = 5;
              break;
          }
        } else if (j == 0) {
          selected_texture = 6;
        } else if (j == MAX_ROOM_HEIGHT_IDX-1) {
          selected_texture = 7;
        } else {
          selected_texture = rng_percent_chance(0.75)? 3 : rng_random_int(0,2);
        }

        r.texture_map[i][j] = selected_texture;
      }
    }
    /*
     * because there can only be one active mob handler anyways, we will
     * use a reference to the statically allocated one, which will be reused.
    */
    current_mh = default_mob_handler();
    r.m_handler_p = &current_mh;
    /* generate id as row-col, always set to be 3 chars on each side of the dash */
    snprintf(r.id, ID_SIZE, "%03d-%03d", r.row_pos, r.col_pos);

    return r;
}

void link_rooms(Room map[MAX_ROWS][MAX_COLS]) {
  for(int i = 0; i < MAX_ROWS; ++i) {
    for(int j = 0; j < MAX_COLS; ++j) {
      /* Verify room is initialized before trying to create doorways */
      if(map[i][j].is_initialized) {
        int room_width  = map[i][j].width;
        int room_height = map[i][j].height;
        /* North */
        if(i > 0) {
          if(map[i-1][j].is_initialized) {
            //printf("found north room.\n");
            create_hitbox(&map[i][j].north_door, room_width/2 - DOOR_HEIGHT/2, 0, DOOR_HEIGHT, DOOR_WIDTH);
            map[i][j].room_configuration[0] = 1;
          }
        }
        /* South */
        if(i < MAX_ROWS-1) {
          if(map[i+1][j].is_initialized) {
            //printf("found south room.\n");
            create_hitbox(&map[i][j].south_door, room_width/2 - DOOR_HEIGHT/2, room_height - DOOR_WIDTH, DOOR_HEIGHT, DOOR_WIDTH);
            map[i][j].room_configuration[1] = 1;
          }
        }
        /* East */
        if(j < MAX_COLS-1) {
          if(map[i][j+1].is_initialized) {
            //printf("found east room.\n");
            create_hitbox(&map[i][j].east_door, room_width - DOOR_WIDTH, room_height/2 - DOOR_HEIGHT/2, DOOR_WIDTH, DOOR_HEIGHT);
            map[i][j].room_configuration[2] = 1;
          }
        }
        /* West */
        if(j > 0) {
          if(map[i][j-1].is_initialized) {
            //printf("found west room.\n");
            create_hitbox(&map[i][j].west_door, 0, room_height/2 - DOOR_HEIGHT/2, DOOR_WIDTH, DOOR_HEIGHT);
            map[i][j].room_configuration[3] = 1;
          }
        }
      }
    }
  }
}

int generate_path_between_rooms(Room map[MAX_ROWS][MAX_COLS], int r1, int c1, int r2, int c2) {
  if(!map[r1][c1].is_initialized && !map[r2][c2].is_initialized) {
    return ERROR;
  }
  int current_row = r1;
  int current_col = c1;

  /* Start with a 50% chance to move in the x or y direction to try to avoid row/col bias */
  double chance = 0.5;
  while((current_row != r2) || (current_col != c2)) {
    if(rng_percent_chance(chance) && current_row != r2) {
      /* Handle Row first */
      if(current_row < r2) current_row+=1;
      else if(current_row > r2) current_row-=1;

      /* Generate room if it is not already initialized */
      if(!map[current_row][current_col].is_initialized) {
        map[current_row][current_col] = generate_room(current_row, current_col, R_HALLWAY);
      }
    }
    else if(current_col != c2) {
      /* Handle Column next */
      if(current_col < c2) current_col+=1;
      else if(current_col > c2) current_col-=1;

      /* Generate room if it is not already initialized */
      if(!map[current_row][current_col].is_initialized) {
        map[current_row][current_col] = generate_room(current_row, current_col, R_HALLWAY);
      }
      /* Set Chance to be 100% as to not waste loops. */
      if(current_col == c2) chance = 1;
    }
  }
  return OK;
}

Room bsp_step(Room map[MAX_ROWS][MAX_COLS],
              int init_row_pos,
              int init_col_pos,
              int start_row,
              int end_row,
              int start_col,
              int end_col) {
  /*
  * Here are the steps to the Binary Space Partitioning (BSP) algorithm:
  * 1. check if we are in exit condition
  *   -> if so, return a generated room within the range.
  * 2. else decide wether to split the map vertically or horizontally
  *   -> split current map sections, and call bsp_step on each subgraph.
  *     -> This should (eventually) return rooms R1 and R2
  *   -> create path between the R1 and R2
  *   -> return either R1 or R2 (for future linking)
  */

  /* Recursive exit condition: if subgraph is <= minimum size, generate a room and exit */
  if((end_row-start_row <= MIN_SUBGRAPH_SIZE) || (end_col-start_col <= MIN_SUBGRAPH_SIZE)) {
    int row_pos, col_pos;
    bool is_start_room = false;
    /* Check if starting position is in subgraph. If so, use that as the generated room */
    if((init_row_pos >= start_row && init_row_pos <= end_row) &&
       (init_col_pos >= start_col && init_col_pos <= end_col)) {
      row_pos = init_row_pos;
      col_pos = init_col_pos;
      is_start_room = true;
    }
    else {
      /* Otherwise, generate random position within row/col range */
      row_pos = rng_random_int(start_row, end_row);
      col_pos = rng_random_int(start_col, end_col);
    }
    /* Check if room is initialized, if not, generate new room.*/
    if(!map[row_pos][col_pos].is_initialized) {
      Room r = default_room();
      r = generate_room(row_pos, col_pos, R_BASIC);
      map[row_pos][col_pos] = r;
    }

    if(is_start_room) map[row_pos][col_pos].type = R_START;

    /* Return selected room position, whether it be generated or selected */
    return map[row_pos][col_pos];
  }
  else {
    bool vertical_splice = rng_percent_chance(0.5);
    Room r1 = default_room();
    Room r2 = default_room();
    Room output = default_room();
    int bisect_length;
    /* Choose to splice the subtree vertically or horizontally, 50% chance either way */
    /*
    * TODO: right now each section bisects in half perfectly, it may be intersting to
    * have a more dynamic system...
    */
    if(vertical_splice) {
      bisect_length = floor((end_row-start_row)/2);
      //bisect_length = rng_random_int(2, (end_row - 2));
      r1 = bsp_step(map, init_row_pos, init_col_pos, start_row, start_row+bisect_length, start_col, end_col);
      r2 = bsp_step(map, init_row_pos, init_col_pos, start_row+bisect_length+1, end_row, start_col, end_col);
    }
    else {
      bisect_length = floor((end_col-start_col)/2);
      //bisect_length = rng_random_int(2, (end_col - 2));
      r1 = bsp_step(map, init_row_pos, init_col_pos, start_row, end_row, start_col, start_col+bisect_length);
      r2 = bsp_step(map, init_row_pos, init_col_pos, start_row, end_row, start_col+bisect_length+1, end_col);
    }

    /* Once the two subgraphs return, create a path between their generated rooms */
    generate_path_between_rooms(map, r1.row_pos, r1.col_pos, r2.row_pos, r2.col_pos);

    /* Randomly select one of the 2 connected rooms, and choose that as the output */
    output = rng_percent_chance(0.5)? r1 : r2;
    return output;
  }
}

/*
* Randomly distribute a room attribute type across a generated floor.
*/
void distr_attribute(Floor* f, int amount, Room_Type type) {
  int pos = 0;
  typedef struct coord{
    int row;
    int col;
  } Coord;
  Coord available_coords[MAX_ROWS * MAX_COLS] = {{0,0}};
  int total = 0;

  //Populate list of available (rows/cols)
  for(int i = 0; i < MAX_ROWS; i++) {
    for(int j = 0; j < MAX_COLS; j++) {
        if(f->map[i][j].type == R_BASIC || f->map[i][j].type == R_HALLWAY) {
          available_coords[total].row = i;
          available_coords[total].col = j;
          total++;
        }
    }
  }
  /* Loop until all the rooms have been distributed */
  for(int iter = 0; iter < amount; iter++) {
    //select which room to distribute

    pos = rng_random_int(0, total-1);
    f->map[available_coords[pos].row][available_coords[pos].col].type = type;

    for(int i = 0; i < total-2; i++) {
      if(i >= pos) {
        available_coords[i] = available_coords[i+1];
      }
    }
    total-=1;
  }
}

/*
 *******************************************************************************
 * Externally Visible Functions
 *******************************************************************************
*/
int load_room(Room* r) {
  if(r->is_initialized && !r->is_loaded) {
    /* load in graphics for room */
    r->door = al_load_bitmap("../assets/door.png");
    if(!r->door) {
        printf("(load_room): couldn't load door image.\n");
        return ERROR;
    }
    /* Spawn in Mobs and other things based on room type */

    switch(r->type) {
      case R_BASIC:
        initialize_handler(r->m_handler_p, 100);
        break;
      case R_CHALLENGE:
        initialize_handler(r->m_handler_p, 100);
        break;
      default:
        current_mh = default_mob_handler();
        break;
    }

    if(r->is_spawnable && r->m_handler_p->is_initialized) {
      spawn_mobs(r->m_handler_p, r->width, r->height, 1);
    }

    r->is_loaded = true;
    //printf("Loaded Room %s\n", r->id);
    return OK;
  } else {
      printf("Room %s load error: Initialization Status: %d, Load Status: %d\n", r->id, r->is_initialized, r->is_loaded);
    return ERROR;
  }
}

int unload_room(Room* r) {
  if(r->is_loaded) {
    al_destroy_bitmap(r->door);
    r->is_loaded = false;
    return OK;
  } else {
    printf("Room is not loaded, and cannot be unloaded.\n");
    return ERROR;
  }
}

Room* change_rooms(Room map[MAX_ROWS][MAX_COLS], Room* current_room, Mob* p) {
  /* TODO: implement exception handling via status */
  int status;
  int curr_row = current_room->row_pos;
  int curr_col = current_room->col_pos;

  /* Check for north door collision */
  if(is_collision(&p->hb, &current_room->north_door) && map[curr_row-1][curr_col].is_initialized) {
    status = load_room(&map[curr_row-1][curr_col]);
    status = unload_room(current_room);
    move_mob(p, p->position[0], map[curr_row-1][curr_col].height - PLAYER_HEIGHT - DOOR_WIDTH - 1);
    return &map[curr_row-1][curr_col];
  }
  /* Check for south door collision */
  else if(is_collision(&p->hb, &current_room->south_door) && map[curr_row+1][curr_col].is_initialized) {
    status = load_room(&map[curr_row+1][curr_col]);
    status = unload_room(current_room);
    move_mob(p, p->position[0], DOOR_WIDTH + 1);
    return &map[curr_row+1][curr_col];
  }
  /* Check for east door collision */
  else if(is_collision(&p->hb, &current_room->east_door) && map[curr_row][curr_col+1].is_initialized) {
    status = load_room(&map[curr_row][curr_col+1]);
    status = unload_room(current_room);
    move_mob(p, 1 + DOOR_WIDTH, p->position[1]);
    return &map[curr_row][curr_col+1];
  }
  /* Check for west door collision */
  else if(is_collision(&p->hb, &current_room->west_door) && map[curr_row][curr_col-1].is_initialized) {
    status = load_room(&map[curr_row][curr_col-1]);
    status = unload_room(current_room);
    move_mob(p, map[curr_row][curr_col-1].width-DOOR_WIDTH-PLAYER_WIDTH-1, p->position[1]);
    return &map[curr_row][curr_col-1];
  }
  else {
    return current_room;
  }
}

void generate_floor(Floor* f, int floor_num, int init_row, int init_col) {
  /* Fill floor map with rooms:
  *  Current Algorithm is using Binary Space Partitioning with the caveat of a starting square.
  */
  for(int i = 0; i < MAX_ROWS; ++i) {
    for(int j = 0; j < MAX_COLS; ++j) {
        f->map[i][j] = default_room();
    }
  }

  /* set floor number and floor "size" */
  f->key_found = false;
  f->number = floor_num;

  switch(f->number){
    default:
      f->texture_p = al_load_bitmap("../assets/forest_texture.png");
      break;
  }
  if(f->texture_p == NULL) {
    printf("(generate_floor): ERROR loading floor texture sheet.\n");
  }

  f->start_row = constrain(0, MAX_ROWS, MAX_ROWS/2 - (4 + f->number));
  f->start_col = constrain(0, MAX_COLS, MAX_COLS/2 - (4 + f->number));
  f->stop_row  = constrain(0, MAX_ROWS-1, MAX_ROWS/2 + (4 + f->number));
  f->stop_col  = constrain(0, MAX_COLS-1, MAX_COLS/2 + (4 + f->number));

  bsp_step(f->map, init_row, init_col, f->start_row, f->stop_row, f->start_col, f->stop_col);
  //bsp_step(f->map, start_row, start_col, 5, MAX_ROWS-5, 5, MAX_COLS-5);
  link_rooms(f->map);

  /* Once the floor layout is generated, Need to populate it with...stuff */
  /* ALWAYS generate 1 key and 1 exit per floor */
  distr_attribute(f, 1, R_KEY);
  distr_attribute(f, 1, R_EXIT);
  /*
   * TODO: figure out a better method of calculating how many shops/challenge
   * rooms to generate, here are some thoughts:
   *  1. purely based off floor number rand(1, floor_num)
   *  2. Based off the total number of available rooms on the floor
   */
  distr_attribute(f, rng_random_int(1, f->number+1), R_SHOP);
  distr_attribute(f, rng_random_int(1, f->number), R_CHALLENGE);
  printf("Generated Floor %d\n",f->number);
  print_floor(f);
}

/*
* Destroy_Floor
* ============
* Remove any artifacts from a floor that is no longer being used.
*/
void destroy_floor(Floor* floor_p) {
  al_destroy_bitmap(floor_p->texture_p);
}

/*
* ====================
* Update_Dungeon_State
* ====================
* Update all artifacts of the current dungeon state including mobs and room
* changes.
*/
Room* update_dungeon_state(Floor* floor, Room* room, Mob* player) {
  /*
  * No mobs on screen, means we can start checking to see if we need to change
  * rooms.
  */
  update_all_active_mobs(room->m_handler_p, room->width, room->height);

  if(room->m_handler_p->mob_count <= 0) {
    room->is_locked    = false;
    room->is_spawnable = false;
    Room* new_room = change_rooms(floor->map, room, player);
    if(strcmp(new_room->id, room->id) != 0) {
      room = new_room;
      //print_floor(floor);
    }
  }
  else {
    if(room->is_spawnable) {
      room->is_locked = true;
    }
  }
  /* TODO: This is where I will probably unlock the door to the next floor */

  return room;
}

void draw_room(Room* r, ALLEGRO_BITMAP* room_texture_p, double delta_time) {
  if(!r->is_loaded) {
    printf("(draw_room): Trying to display unloaded room: %s.\n", r->id);
    exit(1);
  }
  if(!room_texture_p) {
    printf("(draw_room): Provided texture map not loaded.\n");
    exit(1);
  }
  //al_draw_bitmap(r->map, 0, 0, 0);
  /* draw tiles based on generated texture map */
  for(int i = 0; i < MAX_ROOM_WIDTH_IDX; i++) {
    for(int j = 0; j < MAX_ROOM_HEIGHT_IDX; j++) {
      al_draw_bitmap_region(room_texture_p,
                            (r->texture_map[i][j]%4)*PX_PER_TILE,
                            floor(r->texture_map[i][j]/4)*PX_PER_TILE,
                            PX_PER_TILE,
                            PX_PER_TILE,
                            i * PX_PER_TILE,
                            j * PX_PER_TILE,
                            0);
    }
  }

  if(r->m_handler_p->is_initialized) {
    draw_all_active_mobs(r->m_handler_p, delta_time);
  }

  /* draw doors of the room as well in order: N, S, E, W */
  if(!r->is_locked) {
    if(r->room_configuration[0] == 1) {
      al_draw_rotated_bitmap(r->door, 0, DOOR_HEIGHT/2, r->width/2, 0, ALLEGRO_PI/2, 0);
    }
    if(r->room_configuration[1] == 1) {
      al_draw_rotated_bitmap(r->door, 0, DOOR_HEIGHT/2, r->width/2, r->height - DOOR_WIDTH, ALLEGRO_PI/2, ALLEGRO_FLIP_HORIZONTAL);
    }
    if(r->room_configuration[2] == 1) {
      al_draw_bitmap(r->door, r->width - DOOR_WIDTH, r->height/2 - DOOR_HEIGHT/2, ALLEGRO_FLIP_HORIZONTAL);
    }
    if(r->room_configuration[3] == 1) {
      al_draw_bitmap(r->door, 0, r->height/2 - DOOR_HEIGHT/2, 0);
    }
  }
}

void print_floor(Floor* f) {
  char room_token;
  for(int i = 0; i < MAX_ROWS; ++i){
    printf("%02d. |", i);
    for(int j = 0; j < MAX_COLS; ++j) {
      if(f->map[i][j].is_loaded) {
        room_token = 'P';
      }
      else if(f->map[i][j].is_initialized) {
        switch(f->map[i][j].type) {
          case R_BASIC:
            room_token = '.';
            break;
          case R_CHALLENGE:
            room_token = 'C';
            break;
          case R_DEFAULT:
            room_token = '?';
            break;
          case R_EXIT:
            room_token = 'E';
            break;
          case R_HALLWAY:
            room_token = '-';
            break;
          case R_KEY:
            room_token = 'K';
            break;
          case R_SHOP:
            room_token = 'S';
            break;
          case R_START:
            room_token = 'O';
            break;
          default:
            room_token = '?';
            break;
        }
      }
      else {
        room_token = ' ';
      }
      printf("%c", room_token);
    }
    printf("|\n");
  }
}
