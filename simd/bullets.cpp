#include "bullets.h"

#include "../constants_PARALLEL_shooter.h"

#include <chrono>    // for std::chrono
#include <numeric>   // for std::accumulate

bool bullets_initialise(bullets* bs, magpie::spritesheet& sheet,
	irect const& boundary)
{
	texture_rect const* bullets_sprite_info = sheet.get_sprite_info("bullet.png");
	if (!bullets_sprite_info)
	{
		return false;
	}


	vec2 const default_position = { 0.f, 0.f };
	f32 const default_animation_time = -1.f; // no animation

	bs->bullet_sprite.initialise(bullets_sprite_info,
		default_position,
		{
		  (f32)BULLET_SPRITE_SIZE * magpie::INITIAL_RESOLUTION_SCALE * BULLET_SCALE_MODIFER,
		  (f32)BULLET_SPRITE_SIZE * magpie::INITIAL_RESOLUTION_SCALE * BULLET_SCALE_MODIFER
		},
		BULLET_ANIM_OFFSET_FRAMES, BULLET_ANIM_NUM_FRAMES,
		{ BULLET_SPRITE_SIZE, BULLET_SPRITE_SIZE }, default_animation_time);


	vec2 const velocity = { 0.f, 0.f };
	shooter_id const id = shooter_id::shooter_0;

	for (u32 i = 0u; i < NUM_BULLETS; ++i)
	{
		//Set positions
		bs->m_PosX[i] = default_position.x;
		bs->m_PosY[i] = default_position.y;
		//Set Velocities
		bs->m_VelX[i] = velocity.x;
		bs->m_VelY[i] = velocity.y;
		//Set IDs
		bs->m_ShooterID[i] = id;
		bs->m_Lifetime[i] = -1.f;
	}

	return true;
}

u32 bullets_get_num_active(bullets& bs)
{
	unsigned count = 0;
	for (unsigned i(0); i < NUM_BULLETS; ++i)
		if (bs.IsActive(i))
			++count;
	return count;
}

void bullets::Update(magpie::ext::game_timers::duration_t const& elapsed, irect const& boundary)
{
	f32 const elapsed_seconds = std::chrono::duration_cast <std::chrono::duration <f32>> (elapsed).count();

	//Get container pointers
	__m128* posX = (__m128*)m_PosX;
	__m128* posY = (__m128*)m_PosY;
	__m128* velX = (__m128*)m_VelX;
	__m128* velY = (__m128*)m_VelY;
	__m128* lt = (__m128*)m_Lifetime;

	//Condition Checks
	__m128 posXCon1 = _mm_set_ps1((float)boundary.left);
	__m128 posXCon2 = _mm_set_ps1((float)(boundary.left + boundary.width));
	__m128 posYCon1 = _mm_set_ps1((float)(boundary.top - boundary.height));
	__m128 posYCon2 = _mm_set_ps1((float)boundary.top);

	//Mask
	__m128 mask00;
	__m128 compMask;

	//Other
	__m128 const es = _mm_set_ps1((float)elapsed_seconds);
	__m128 const zeroes = _mm_set_ps1(0.f);
	__m128 const negOnes = _mm_set_ps1(-1.f);
		

	for (unsigned i(0); i < NUM_BULLETS / 4; ++i)
	{
		//
		//Pre-amble
		// 

		//Temp container for value holding
		__m128 temp = _mm_set_ps1(0);
		//mask for is active conditional mask
		__m128 activeMask = _mm_set_ps1(0);

		//Construct mask for what can and cant be updated this frame
		activeMask = _mm_cmpgt_ps(*lt, zeroes);

		//
		//Lifetime downtick
		//

		temp = _mm_blendv_ps(zeroes, es, activeMask);
		*lt = _mm_sub_ps(*lt, temp);

		//
		//Position
		//		

		//Do velX * es
		temp = _mm_mul_ps(*velX, es);
		//AND result against mask
		temp = _mm_and_ps(temp, activeMask);
		//Add to pos X
		*posX = _mm_add_ps(*posX, temp);

		//Do velY * es
		temp = _mm_mul_ps(*velY, es);
		//AND result against mask
		temp = _mm_and_ps(temp, activeMask);
		//Add to pos Y
		*posY = _mm_add_ps(*posY, temp);

		//
		//Boundary Collision
		//

		//
		//X + declarations
		// 

		//Mask Operations

		compMask = _mm_cmple_ps(*posX, posXCon1);
		mask00 = _mm_cmpgt_ps(*posX, posXCon2);
		compMask = _mm_or_ps(compMask, mask00);
		compMask = _mm_and_ps(compMask, activeMask);


		//Perform clamping effect, starting with max (reusing this later for Y)
		temp = _mm_max_ps(*posX, posXCon1);
		//Then min
		temp = _mm_min_ps(temp, posXCon2);
		//Blend mask with clamp effect to set or not set new position value
		*posX = _mm_blendv_ps(*posX, temp, compMask);

		//Now, invert vel and set or not set new vel
		temp = _mm_mul_ps(*velX, negOnes);
		//Apply to vel
		*velX = _mm_blendv_ps(*velX, temp, compMask);

		//
		//Y
		//

		//Regenerate masks + clamp
		compMask = _mm_cmple_ps(*posY, posYCon1);
		mask00 = _mm_cmpgt_ps(*posY, posYCon2);
		compMask = _mm_or_ps(compMask, mask00);
		compMask = _mm_and_ps(compMask, activeMask);

		temp = _mm_max_ps(*posY, posYCon1);
		temp = _mm_min_ps(temp, posYCon2);

		//Now blend like before
		*posY = _mm_blendv_ps(*posY, temp, compMask);

		//Now, invert vel and set or not set new vel
		temp = _mm_mul_ps(*velY, negOnes);
		*velY = _mm_blendv_ps(*velY, temp, compMask);


		//Increment pointers
		++posX;
		++posY;
		++velX;
		++velY;
		++lt;
	}
}

void bullets::Render(magpie::renderer& renderer, magpie::_2d::sprite_batch& sb)
{
	for(unsigned i(0); i < NUM_BULLETS;++i)
		if (IsActive(i))
		{
			bullet_sprite.position = { m_PosX[i], m_PosY[i] };
			bullet_sprite.frame_id = (u32)m_ShooterID[i];

			bullet_sprite.render(renderer, sb);
		}
}


