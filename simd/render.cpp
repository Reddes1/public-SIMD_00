#include "magpie.h"

#include "bullets.h"
#include "shooters.h"


static void render_bullets(magpie::renderer& renderer,
	magpie::_2d::sprite_batch& sb,
	bullets& bs)
{
	bs.Render(renderer, sb);
}
static void render_shooters(magpie::renderer& renderer,
	magpie::_2d::sprite_batch& sb,
	shooters& s)
{
	s.Render(renderer, sb);
}
void render(magpie::renderer& renderer,
	magpie::_2d::sprite_batch& sb,
	bullets& bs, shooters& s)
{
	render_bullets(renderer, sb, bs);
	render_shooters(renderer, sb, s);
}
