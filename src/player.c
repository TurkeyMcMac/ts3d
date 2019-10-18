#include "player.h"
#include <math.h>
#include <stdlib.h>

void player_init(struct player *player, struct map *map)
{
	player->start = &map->player;
	player->body.pos = player->start->pos;
	player->body.radius = player->start->type->width / 2;
	player->body.health = player->start->type->health;
	player->body.damage = player->start->type->damage;
	player->turn_speed =
		2.5 * chance_to_fraction(player->start->type->turn_chance);
	player->reload_ready =
		1.0 / chance_to_fraction(player->start->type->shoot_chance);
	player->reload = player->reload_ready;
	player->facing = 0;
}

double player_health_fraction(const struct player *player)
{
	return player->body.health / player->start->type->health;
}

double player_reload_fraction(const struct player *player)
{
	return (double)player->reload / player->reload_ready;
}

static bool player_can_shoot(struct player *player)
{
	return player->start->type->bullet
	    && player->reload >= player->reload_ready;
}

void player_tick(struct player *player)
{
	if (!player_can_shoot(player)) ++player->reload;
}

void player_walk(struct player *player, double angle)
{
	angle += player->facing;
	player->body.pos.x += player->start->type->speed * cos(angle);
	player->body.pos.y += player->start->type->speed * sin(angle);
}

void player_turn_ccw(struct player *player)
{
	player->facing += player->turn_speed;
}

void player_turn_cw(struct player *player)
{
	player->facing -= player->turn_speed;
}

bool player_is_dead(const struct player *player)
{
	return player->body.health < 0;
}

bool player_try_shoot(struct player *player, struct ents *ents)
{
	if (!player_can_shoot(player)) return false;
	player->reload = 0;
	ent_id bullet = ents_add(ents, player->start->type->bullet, TEAM_ALLY,
		&player->body.pos);
	d3d_vec_s *bvel = ents_vel(ents, bullet);
	bvel->x = 2 * player->start->type->speed * cos(player->facing);
	bvel->y = 2 * player->start->type->speed * sin(player->facing);
	return true;
}

void player_collide(struct player *player, struct ents *ents)
{
	ENTS_FOR_EACH(ents, e) {
		if (teams_can_collide(player->start->team, ents_team(ents, e)))
			bodies_collide(&player->body, ents_body(ents, e));
	}
}

void player_move_camera(struct player *player, d3d_camera *cam)
{
	*d3d_camera_position(cam) = player->body.pos;
	*d3d_camera_facing(cam) = player->facing;
}
