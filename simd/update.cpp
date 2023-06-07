#include "../constants_PARALLEL_shooter.h"
#include "bullets.h"
#include "shooters.h"
#include "timers/game_timers.h"

#include <utility> // for std::tie

#include "Tools_PerformanceTimers.h"

//Janky but quick way to get breakdown of individual sections
static QPCPerformanceTimer TIMER_00;
static QPCPerformanceTimer TIMER_01;
static QPCPerformanceTimer TIMER_02;
bool doOnce = true;

static void update_bullets(magpie::ext::game_timers::duration_t const& elapsed, bullets& bs, irect const& boundary)
{
	//Update bullets
	bs.Update(elapsed, boundary);
}
static void update_shooters(magpie::ext::game_timers::duration_t const& elapsed,
	shooters& s)
{
	f32 const elapsed_seconds = std::chrono::duration_cast <std::chrono::duration <f32>> (elapsed).count();

	s.Update(elapsed_seconds);

}

// update the variables in bullets and shooters that rely on data from the other
static void update_combined(magpie::ext::game_timers::duration_t const& elapsed,
	bullets& bs, shooters& s)
{
	s.gun_angle.update(elapsed);
	f32 const gun_angle = s.gun_angle.get();
	// spawn bullets
	s.time_since_last_automatic_shot += elapsed;
	bool const automatic_shot = s.time_since_last_automatic_shot > SHOOTER_GUN_FREQUENCY;

	//If can shoot
	if (automatic_shot)
	{	
		s.time_since_last_automatic_shot -= SHOOTER_GUN_FREQUENCY;

		//Condition Check ( < 0 = not active) and mask
		__m128 zeroes = _mm_set_ps1(0);
		__m128 mask = _mm_set_ps1(0);

		//Pre cos/sin angle for application to X/Y later
		__m128 xCos = _mm_set_ps1(gun_angle);
		xCos = _mm_cos_ps(xCos);
		__m128 ySin = _mm_set_ps1(gun_angle);
		ySin = _mm_sin_ps(ySin);

		//Other/Variables scaled for SIMD
		__m128 temp = _mm_set_ps1(0);
		__m128 shooterDis = _mm_set_ps1(SHOOTER_GUN_DISTANCE);
		__m128 bulletMaxLT = _mm_set_ps1(std::chrono::duration<float>(BULLET_LIFETIME).count());
		__m128 bulletSpeed = _mm_set_ps1(BULLET_SPEED * magpie::INITIAL_RESOLUTION_SCALE);

		//Array pointers for shooters
		__m128* shooterID = (__m128*)s.m_ID;
		__m128* shooterDirX = (__m128*)s.m_DirX;
		__m128* shooterPosX = (__m128*)s.m_PosX;
		__m128* shooterPosY = (__m128*)s.m_PosY;

		//Array pointer for bullets
		__m128* bulletPosX = (__m128*)bs.m_PosX;
		__m128* bulletPosY = (__m128*)bs.m_PosY;
		__m128* bulletVelX = (__m128*)bs.m_VelX;
		__m128* bulletVelY = (__m128*)bs.m_VelY;
		__m128* bulletLT = (__m128*)bs.m_Lifetime;
		__m128* bulletID = (__m128*)bs.m_ShooterID;

		//Holds relative index of the bullets being looked (index * 4)
		unsigned index = 0;	 //Relates the the real index of being checked for bullets

		for (unsigned i(0); i < NUM_SHOOTERS / 4; ++i)
		{
			//Replaces std::tie, looking at 4 indexes at a time
			bool notDone = true; //Exit flag
			do
			{
				//Sample and mask check first index
				mask = _mm_cmple_ps(*bulletLT, zeroes);
				//If any bullet is free, sets flag false and early outs of the while loop
				notDone = _mm_comile_ss(mask, zeroes);

				//Increment relative index
				++index;

				//Increment all bullet pointers to remain in sync for later
				++bulletPosX;
				++bulletPosY;
				++bulletVelX;
				++bulletVelY;
				++bulletLT;
				++bulletID;

			} while (index < NUM_BULLETS / 4 && notDone);
			
			//safety if for testing
			if (!notDone)
			{
				//As the indexes are advanced before checking if the condition is done, 
				//re-align them for the following section (as the last index was valid if it got here)
				--bulletPosX;
				--bulletPosY;
				--bulletVelX;
				--bulletVelY;
				--bulletLT;
				--bulletID;
				
				//
				//shoot >>START<<
				//

				//
				//calculate_bullet_spawn_position >>START<<
				//

				//Get the shooters positions for bullet alignment
				__m128 sPosX = *shooterPosX;
				__m128 sPosY = *shooterPosY;

				//Setup Pos X
				temp = _mm_mul_ps(shooterDis, *shooterDirX);
				temp = _mm_mul_ps(xCos, temp);
				sPosX = _mm_add_ps(sPosX, temp);
				//Setup Pos Y
				temp = _mm_mul_ps(ySin, shooterDis);
				sPosY = _mm_add_ps(sPosY, temp);

				//
				//calculate_bullet_spawn_position >>END<<
				//

				//Set new bullet positions
				*bulletPosX = sPosX;
				*bulletPosY = sPosY;

				//Set velocities
				//(bullet_speed * dir)
				temp = _mm_mul_ps(bulletSpeed, *shooterDirX);
				//VelX = cos * above
				*bulletVelX = _mm_mul_ps(xCos, temp);
				//VelY = sin * bullet_speed
				temp = _mm_mul_ps(ySin, bulletSpeed);
				*bulletVelY = temp;

				//Set ID and lifetime
				*bulletID = *shooterID;
				*bulletLT = bulletMaxLT;

				//
				//shoot >>END<<
				//

			}

			//Advance pointers
			++shooterID;
			++shooterDirX;
			++shooterPosX;
			++shooterPosY;

			//Sub-note: We dont reset the bullet index at this point because we KNOW that the current index is now unusable, 
			//and that previous indexs are also unusable, so we advance from this point to highly likely useable indexes.
		}
	}
}
void update(magpie::ext::game_timers::duration_t const& elapsed,
	bullets& bs, shooters& s,
	irect const& boundary)
{
	if (doOnce)
	{
		TIMER_00.fpName = "Update_Bullets.txt";
		TIMER_01.fpName = "Update_Shooters.txt";
		TIMER_02.fpName = "Update_Combined.txt";
		doOnce = false;
	}

	TIMER_00.StartQPCTimer();
	update_bullets(elapsed, bs, boundary);
	TIMER_00.EndQPCTimer();

	TIMER_01.StartQPCTimer();
	update_shooters(elapsed, s);
	TIMER_01.EndQPCTimer();

	TIMER_02.StartQPCTimer();
	update_combined(elapsed, bs, s);
	TIMER_02.EndQPCTimer();
}
