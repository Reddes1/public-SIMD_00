#include "PARALLEL_shooter.h"

#include "interface_bullets.h"
#include "interface_shooters.h"
#include "interface_render.h"
#include "interface_update.h"

#include <bit>    // for std::bit_ceil
#include <chrono> // for std::chrono

#include "SIMD/Tools_PerformanceTimers.h"

bool PARALLEL_shooter::initialise(magpie::renderer& renderer)
{
	//Allocated for bullets struct
	my_bullets = new bullets();
	my_shooters = new shooters();


	if (!spritesheet.initialise(renderer, "data/textures/PARALLEL/shooter/sprites.xml"))
	{
		return false;
	}
	constexpr u32 max_sprites = std::bit_ceil(NUM_SPRITES);
	if (!sprite_batch.initialise(renderer, spritesheet.get_texture(), max_sprites))
	{
		return false;
	}

	{
		uvec2 const screen_dimensions = renderer.get_screen_dimensions();
		setup_boundary(screen_dimensions.x, screen_dimensions.y);
	}

	//Work on this
	if (!bullets_initialise(my_bullets, spritesheet, boundary))
	{
		return false;
	}

	if (!shooters_initialise(my_shooters, spritesheet, boundary))
	{
		return false;
	}


	if (!timers.initialise())
	{
		return false;
	}
	timers.set_update_period_frames(magpie::ext::game_timers::default_timer_id::UPDATE, 240u);
	timers.set_update_period_frames(magpie::ext::game_timers::default_timer_id::RENDER, 120u);

#ifdef MAGPIE_IMGUI
	if (!debug_window.initialise(renderer))
	{
		return false;
	}
#endif // MAGPIE_IMGUI


	return true;
}

void PARALLEL_shooter::update()
{
	timers.start_frame();


	timers.start_timer(magpie::ext::game_timers::default_timer_id::UPDATE);
	{
		::update(timers.get_duration(magpie::ext::game_timers::default_timer_id::FRAME),
			*my_bullets, *my_shooters,
			boundary);
	}
	timers.end_timer(magpie::ext::game_timers::default_timer_id::UPDATE);
}
bool PARALLEL_shooter::render(magpie::renderer& renderer)
{
	timers.start_timer(magpie::ext::game_timers::default_timer_id::RENDER);
	{
		if (!renderer.sb_begin(sprite_batch))
		{
			return false;
		}

		::render(renderer,
			sprite_batch,
			*my_bullets, *my_shooters);

		renderer.sb_end(sprite_batch);

		renderer.draw(sprite_batch);
	}
	timers.end_timer(magpie::ext::game_timers::default_timer_id::RENDER);


#ifdef MAGPIE_IMGUI
#ifdef MAGPIE_PLATFORM_WINDOWS
	if (!renderer.get_window_is_minimised())
#endif // MAGPIE_PLATFORM_WINDOWS
	{
		if (!debug_window.render <i32, std::micro>(renderer, timers, 'u'))
		{
			return false;
		}
		if (ImGui::Begin(debug_window.get_window_name()))
		{
			ImGui::Separator();

			// general
			{
#ifdef MAGPIE_COMPUTE
				ImGui::Text("COMPUTE");
#else //MAGPIE_COMPUTE
#ifdef MAGPIE_SIMD
				ImGui::Text("SIMD");
				ImGui::Text("Register Width: %s", magpie::sprintf_commarise_mem((i32)NUM_SIMD_LANES_32 * 32));
#else // MAGPIE_SIMD
#ifdef MAGPIE_THREAD
				ImGui::Text("THREAD");
				ImGui::Text("Num Threads: %s", magpie::sprintf_commarise_mem((i32)THREADPOOL_THREAD_COUNT));
				ImGui::Text("Threadpool Queue Size: %s", magpie::sprintf_commarise_mem((i32)THREADPOOL_QUEUE_SIZE));
#else // MAGPIE_THREAD
#ifdef MAGPIE_CUDA
				ImGui::Text("CUDA");
#else // MAGPIE_CUDA
				ImGui::Text("SCALAR");
#endif // MAGPIE_CUDA
#endif // MAGPIE_THREAD
#endif // MAGPIE_SIMD
#endif // MAGPIE_COMPUTE
			}

			ImGui::Separator();

			// custom
			{
				ImGui::Text("Num Shooters: %s", magpie::sprintf_commarise_mem((i32)NUM_SHOOTERS));

				ImGui::Spacing();

				ImGui::Text("Num Bullets: %s", magpie::sprintf_commarise_mem((i32)NUM_BULLETS));
				u32 const num_active_bullets = bullets_get_num_active(*my_bullets);
				ImGui::Text("Num Active Bullets: %s", magpie::sprintf_commarise_mem((i32)num_active_bullets));
				ImGui::Text("Time Per Active Bullet: %sns",
					magpie::sprintf_commarise_mem(timers.get_avg <f32, std::nano>(magpie::ext::game_timers::default_timer_id::UPDATE) / (f32)num_active_bullets));

				ImGui::Spacing();

				constexpr u32 max_potential_active_bullets = (u32)(BULLET_LIFETIME / SHOOTER_GUN_FREQUENCY) * NUM_SHOOTERS;
				ImGui::Text("Bullet Lifetime: %ss",
					magpie::sprintf_commarise_mem(std::chrono::duration_cast <std::chrono::duration <f32>> (BULLET_LIFETIME).count()));
				ImGui::Text("Shoot Frequency: %sms",
					magpie::sprintf_commarise_mem(std::chrono::duration_cast <std::chrono::duration <i32, std::milli>> (SHOOTER_GUN_FREQUENCY).count()));
				ImGui::Text("Max Potential Active Bullets: %s", magpie::sprintf_commarise_mem((i32)max_potential_active_bullets));

				auto const update_duration = timers.get_avg <magpie::ext::game_timers::duration_t::rep, magpie::ext::game_timers::duration_t::period>(magpie::ext::game_timers::default_timer_id::UPDATE);
				bool const too_slow = update_duration > SHOOTER_GUN_FREQUENCY.count();
				constexpr bool not_enough_bullets = NUM_BULLETS < max_potential_active_bullets;

				////Janky ass plug in to save out data
				//++INTERVAL;
				//if (update_duration != 0.f && INTERVAL >= 240)
				//{
				//	SAVER.StoreAndIncrement(static_cast<double>(update_duration / 1000000.0));
				//	INTERVAL = 0;
				//}					

				if (too_slow || not_enough_bullets)
				{
					ImGui::Spacing();
					ImGui::Text("Reason For Fewer Active Bullets:");

					if (too_slow)
					{
						ImGui::Indent();
						ImGui::Text("avg. Frame Time > Shoot Frequency");
					}
					if constexpr (not_enough_bullets)
					{
						ImGui::Indent();
						ImGui::Text("Not Enough Bullets");
					}
				}
			}
		}
		ImGui::End();
	}
#endif // MAGPIE_IMGUI

	return true;
}

void PARALLEL_shooter::release(magpie::renderer const& renderer)
{
#ifdef MAGPIE_IMGUI
	debug_window.release();
#endif // MAGPIE_IMGUI
	timers.release();

	delete my_shooters;
	delete my_bullets;

	sprite_batch.release(renderer);
	spritesheet.release(renderer);
}

bool PARALLEL_shooter::on_window_resize(u32 new_width, u32 new_height)
{
#ifdef MAGPIE_IMGUI
	debug_window.on_window_resize(new_width, new_height);
#endif // MAGPIE_IMGUI

	setup_boundary(new_width, new_height);

	return true;
}

void PARALLEL_shooter::setup_boundary(u32 new_width, u32 new_height)
{
	// update boundary

	boundary.left = -(i32)new_width / 2 + (i32)BORDER_WIDTH;
	boundary.top = (i32)new_height / 2 - (i32)BORDER_WIDTH;
	boundary.width = (i32)new_width - (i32)BORDER_WIDTH * 2;
	boundary.height = (i32)new_height - (i32)BORDER_WIDTH * 2;

	// no need to move players inside new boundary
	// because the update will clamp the the x,y values into it
}
