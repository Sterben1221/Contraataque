#ifndef MAP_H_INC
#define MAP_H_INC
#include "gfx.h"

#define MAP_W 24
#define MAP_H 24
#define CELL 2.0f
#define WALL_H 3.2f
#define CRATE_H 0.95f
#define CRATE_IN 0.12f

enum { T_FLOOR = 0, T_WALL = 1, T_CRATE = 2 };

void map_init(void);
int map_tile(int cx, int cz);
int map_walkable(int cx, int cz);
/* empuja un circulo (jugador) fuera de paredes/cajas */
void map_collide(vec3 *pos, float radius);
/* altura del suelo bajo el jugador (0 o arriba de una caja) */
float map_ground(vec3 pos, float radius);
/* rayo 3D: devuelve distancia al primer obstaculo (o maxd) */
float map_ray(vec3 from, vec3 dir, float maxd);
void map_draw(const camera_t *cam);
/* puntos de aparicion */
int map_spawn_count(int team);
vec3 map_spawn(int team, int idx);
/* lista de celdas libres (para objetivos de bots) */
int map_free_count(void);
int map_free_cell(int i);
/* BFS: llena dist[] (MAP_W*MAP_H) con la distancia en celdas hasta 'goal' */
void map_bfs(int goal, uint16_t *dist);
void map_draw_radar(float x, float y, float s);

#endif
