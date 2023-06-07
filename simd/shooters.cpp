#include "shooters.h"

#include "bullets.h"

#include <random>    // for std::random_device, std::uniform_int_distribution


bool shooters_initialise(shooters* s, magpie::spritesheet& sheet,
	irect const& boundary)
{
	texture_rect const* shadow_sprite_info = sheet.get_sprite_info("shadow.png");
	if (!shadow_sprite_info)
	{
		return false;
	}
	MAGPIE_DASSERT(shadow_sprite_info->width == (i32)DINO_SPRITE_SIZE);
	MAGPIE_DASSERT(shadow_sprite_info->height == (i32)DINO_SPRITE_SIZE);

	texture_rect const* gun_sprite_info = sheet.get_sprite_info("gun.png");
	if (!gun_sprite_info)
	{
		return false;
	}

	std::vector <texture_rect const*> shooter_sprite_infos;
	for (u32 i = 0u; i < (u32)shooter_id::MAX_SIZE; ++i)
	{
		//shooter_sprite_infos.push_back (sheet.get_sprite_info (std::format ("dino_{}.png", i).c_str ())); // c++ 20
		shooter_sprite_infos.push_back(sheet.get_sprite_info(std::string("dino_" + std::to_string(i) + ".png").c_str()));
		if (!shooter_sprite_infos.back())
		{
			return false;
		}
		MAGPIE_DASSERT(shooter_sprite_infos.back()->height == (i32)DINO_SPRITE_SIZE);
	}


	vec2 const default_position = { 0.f, 0.f };
	u32 const default_offset_frames = 0u;
	u32 const default_num_frames = 0u; // single image
	f32 const default_animation_time = -1.f; // no animation

	s->shadow_sprite.initialise(shadow_sprite_info,
		default_position,
		{
		  (f32)DINO_SPRITE_SIZE * magpie::INITIAL_RESOLUTION_SCALE * SHOOTER_SCALE_MODIFER_SHADOW,
		  (f32)DINO_SPRITE_SIZE * magpie::INITIAL_RESOLUTION_SCALE * SHOOTER_SCALE_MODIFER_SHADOW
		},
		default_offset_frames, default_num_frames,
		{ 0, 0 }, default_animation_time);

	s->gun_sprite.initialise(gun_sprite_info,
		default_position,
		{
		  (f32)gun_sprite_info->width * magpie::INITIAL_RESOLUTION_SCALE * SHOOTER_GUN_SCALE_MODIFER,
		  (f32)gun_sprite_info->height * magpie::INITIAL_RESOLUTION_SCALE * SHOOTER_GUN_SCALE_MODIFER
		},
		default_offset_frames, default_num_frames,
		{ 0, 0 }, default_animation_time);

	std::random_device rd;
	std::uniform_int_distribution <u32> frame_id(0u, DINO_ANIM_NUM_FRAMES_IDLE);

	f32 const x_gap = (f32)boundary.width / (f32)((u32)shooter_id::MAX_SIZE + 1u);
	f32 const y_gap = (f32)boundary.height / ((f32)(NUM_SHOOTERS / (u32)shooter_id::MAX_SIZE) + 1.f);

	/////////////////
	/// DoD Setup ///
	/////////////////

	//For each shooter (1024)
	for (unsigned i(0); i < NUM_SHOOTERS; ++i)
	{
		//Emulate Init Portion - parts set later

		//Scale
		s->m_ScaleX[i] = (f32)DINO_SPRITE_SIZE * magpie::INITIAL_RESOLUTION_SCALE * SHOOTER_SCALE_MODIFER;
		s->m_ScaleY[i] = (f32)DINO_SPRITE_SIZE * magpie::INITIAL_RESOLUTION_SCALE * SHOOTER_SCALE_MODIFER;
		//Frame Offset
		s->m_FrameOffset[i] = DINO_ANIM_OFFSET_FRAMES_IDLE;
		//Frame Count
		s->m_FrameCount[i] = DINO_ANIM_NUM_FRAMES_IDLE;
		//Dims
		s->m_DimX[i] = DINO_SPRITE_SIZE;
		s->m_DimY[i] = DINO_SPRITE_SIZE;
		//Anim Timers
		s->m_AnimTime[i] = SHOOTER_ANIMATION_TIME;
		s->m_TimeSinceLastUpdate[i] = 0.f;

		//Emulate For Loop

		//ID Preping
		u32 const x_id = i / (NUM_SHOOTERS / (u32)shooter_id::MAX_SIZE);
		u32 const y_id = i % (NUM_SHOOTERS / (u32)shooter_id::MAX_SIZE);

		//Re-set Rect
		s->m_TextureRect[i] = *shooter_sprite_infos[x_id];
		//Re-set positions
		s->m_PosX[i] = (f32)boundary.left + x_gap * ((f32)x_id + 1.f);
		s->m_PosY[i] = (f32)boundary.top - y_gap * ((f32)y_id + 1.f);
		//Re-set frame ID
		s->m_FrameID[i] = frame_id(rd);

		//Determine facing direction + set it
		direction const face = s->m_PosX[i] < 0.f ? direction::right : direction::left;
		s->m_DirX[i] = (float)face;
		//Set ID
		s->m_ID[i] = (shooter_id)x_id;
	}

	return true;
}

void shooters::Update(f32 elapsed)
{
	///////////////////
	/// SIMD Update ///
	///////////////////

	//Get container pointers
	__m128i* frames = (__m128i*)m_FrameCount;
	__m128i* frameIDs = (__m128i*)m_FrameID;
	__m128* animTimes = (__m128*)m_AnimTime;
	__m128* lastUpdates = (__m128*)m_TimeSinceLastUpdate;
	__m128i* dimX = (__m128i*)m_DimX;
	__m128i* dimY = (__m128i*)m_DimY;

	//Break down dims later

	//Conditions
	__m128 zeroes = _mm_set_ps1(0.f); //Condition check for first 4 if
	__m128 ones = _mm_set_ps1(1.f);   //Modulo replacement
	__m128 fours = _mm_set_ps1(4.f);
	//Other
	__m128 elapsedT = _mm_set_ps1(elapsed);
	__m128 temp = _mm_set_ps1(0.f);

	//Masks
	__m128 compMask = _mm_set_ps1(0.f);
	__m128 mask00 = _mm_set_ps1(0.f);

	for (unsigned i(0); i < NUM_SHOOTERS / 4; ++i)
	{
		//
		//Pre-amble
		//

		//Converted version of frames & ID for use with calculations
		__m128 frameC = _mm_cvtepi32_ps(*frames);
		__m128 idC = _mm_cvtepi32_ps(*frameIDs);

		//
		// IF statement 00
		//

		//Construct and composite masks for each condition
		compMask = _mm_cmpgt_ps(frameC, zeroes);
		mask00 = _mm_cmpgt_ps(_mm_castsi128_ps(*dimX), zeroes);
		compMask = _mm_and_ps(compMask, mask00);
		mask00 = _mm_cmpgt_ps(_mm_castsi128_ps(*dimY), zeroes);
		compMask = _mm_and_ps(compMask, mask00);
		mask00 = _mm_cmpgt_ps(*animTimes, zeroes);
		compMask = _mm_and_ps(compMask, mask00);

		//Calculate new value and apply based on mask
		temp = _mm_add_ps(*lastUpdates, elapsedT);
		*lastUpdates = _mm_blendv_ps(*lastUpdates, temp, compMask);

		//
		// IF statement 01
		//

		//Reuse mask 00, and check against next condition
		temp = _mm_div_ps(*animTimes, frameC);
		mask00 = _mm_cmpgt_ps(*lastUpdates, temp);
		compMask = _mm_and_ps(compMask, mask00);

		//Add 1 to frame, and reset timer based on mask
		*lastUpdates = _mm_blendv_ps(*lastUpdates, zeroes, compMask);
		temp = _mm_add_ps(idC, ones);
		temp = _mm_blendv_ps(idC, temp, compMask);

		//Modulo replacement (check for OOB frame bounds, and replace with 1 if so)
		mask00 = _mm_cmpgt_ps(temp, fours);
		compMask = _mm_and_ps(compMask, mask00);

		temp = _mm_blendv_ps(temp, ones, compMask);

		//Now set frames (with conversion)
		*frameIDs = _mm_cvtps_epi32(temp);

		++frames;
		++frameIDs;
		++animTimes;
		++lastUpdates;
	}
}

void shooters::Render(magpie::renderer& renderer, magpie::_2d::sprite_batch& sb)
{
	//////////////////
	/// DoD Render ///
	//////////////////

	for (unsigned i(0); i < NUM_SHOOTERS; ++i)
	{
		shadow_sprite.position = { m_PosX[i], m_PosY[i] };
		shadow_sprite.render(renderer, sb);

		//Load up shooter_sprite with information required
		shooter_sprite.position = { m_PosX[i], m_PosY[i] };
		shooter_sprite.info = &m_TextureRect[i];
		shooter_sprite.scale.x = m_ScaleX[i] * (f32)m_DirX[i];
		shooter_sprite.scale.y = m_ScaleY[i];
		shooter_sprite.offset_frames = m_FrameOffset[i];
		shooter_sprite.frame_id = m_FrameID[i];
		shooter_sprite.dim = { m_DimX[i], m_DimY[i] };
		shooter_sprite.render(renderer, sb);

		gun_sprite.position = calculate_bullet_spawn_position(i, gun_angle.get());
		gun_sprite.render(renderer, sb);
	}
}

vec2 shooters::calculate_bullet_spawn_position(unsigned index, f32 angle) const
{
	vec2 position = { m_PosX[index], m_PosY[index] };
	position.x += std::cos(angle) * SHOOTER_GUN_DISTANCE * (f32)m_DirX[index];
	position.y += std::sin(angle) * SHOOTER_GUN_DISTANCE;
	return position;
}
