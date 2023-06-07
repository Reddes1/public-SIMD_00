#pragma once

#include "magpie.h"

#include "../constants_PARALLEL_shooter.h"
#include "../direction.h"
#include "../shooter_id.h"
#include "sprite_animated/sprite_animated.h"
#include "ping_pong/ping_pong.h"

struct shooters
{
	//
	//Operations and Behaviours
	//
	
	void Update(f32 elapsed);	
	void Render(magpie::renderer& renderer, magpie::_2d::sprite_batch& sb);
	vec2 calculate_bullet_spawn_position(unsigned index, f32 angle) const;

	//
	//Data
	//

	//Animated Sprites Data

	__declspec(align(16)) texture_rect m_TextureRect[NUM_SHOOTERS];

	//Positions
	__declspec(align(16)) float m_PosX[NUM_SHOOTERS];
	__declspec(align(16)) float m_PosY[NUM_SHOOTERS];

	//Scales
	__declspec(align(16)) float m_ScaleX[NUM_SHOOTERS];
	__declspec(align(16)) float m_ScaleY[NUM_SHOOTERS];

	//Frame Data
	__declspec(align(16)) unsigned m_FrameID[NUM_SHOOTERS];
	__declspec(align(16)) unsigned m_FrameCount[NUM_SHOOTERS];
	__declspec(align(16)) unsigned m_FrameOffset[NUM_SHOOTERS];

	//Timings
	__declspec(align(16)) float m_AnimTime[NUM_SHOOTERS];
	__declspec(align(16)) float m_TimeSinceLastUpdate[NUM_SHOOTERS];

	//Dims
	__declspec(align(16)) int m_DimX[NUM_SHOOTERS];
	__declspec(align(16)) int m_DimY[NUM_SHOOTERS];

	//Other Data	
	__declspec(align(16)) float m_DirX[NUM_SHOOTERS];
	__declspec(align(16)) shooter_id m_ID[NUM_SHOOTERS];
	
	magpie::ext::sprite_animated shadow_sprite;
	magpie::ext::sprite_animated gun_sprite;
	magpie::ext::sprite_animated shooter_sprite;
	magpie::ext::ping_pong <f32> gun_angle{ SHOOTER_GUN_ANGLE_MIN, SHOOTER_GUN_ANGLE_SPEED, 0.f, SHOOTER_GUN_ANGLE_MIN, SHOOTER_GUN_ANGLE_MAX };
	magpie::ext::game_timers::duration_t time_since_last_automatic_shot{};
};

bool shooters_initialise(shooters* s, magpie::spritesheet& sheet,
	irect const& boundary);