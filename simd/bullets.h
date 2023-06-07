#pragma once

#include "magpie.h"

#include "../shooter_id.h"
#include "sprite_animated/sprite_animated.h"
#include "timers/game_timers.h"

struct bullets
{
	//
	//Operations and Behaviours
	//

	void Update(magpie::ext::game_timers::duration_t const& elapsed, irect const& boundary);
	void Render(magpie::renderer& renderer, magpie::_2d::sprite_batch& sb);

	bool IsActive(unsigned index) { return m_Lifetime[index] > 0.0f; }

	//
	// New Data 
	//

	//Lifetimes
	__declspec(align(16)) float m_Lifetime[NUM_BULLETS] = {};
	//Positions
	__declspec(align(16)) float m_PosX[NUM_BULLETS] = {};
	__declspec(align(16)) float m_PosY[NUM_BULLETS] = {};
	//Velocities
	__declspec(align(16)) float m_VelX[NUM_BULLETS] = {};
	__declspec(align(16)) float m_VelY[NUM_BULLETS] = {};
	//ShooterIDs
	shooter_id m_ShooterID[NUM_BULLETS] = {};

	//Sprite shared between all bullets
	magpie::ext::sprite_animated bullet_sprite = {};
};

bool bullets_initialise(bullets* bs, magpie::spritesheet& sheet,
	irect const& boundary);