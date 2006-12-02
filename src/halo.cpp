/* $Id$ */
/*
   Copyright (C) 2003-5 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "display.hpp"
#include "gl_image.hpp"
#include "gl_image_cache.hpp"
#include "halo.hpp"
#include "image.hpp"
#include "preferences.hpp"
#include "sdl_utils.hpp"
#include "util.hpp"
#include "video.hpp"
#include "wassert.hpp"
#include "serialization/string_utils.hpp"

#include <algorithm>
#include <map>

namespace halo
{

namespace {
display* disp = NULL;

class effect
{
public:
	effect(int xpos, int ypos, const animated<std::string>::anim_description& img, ORIENTATION orientation,bool infinite);

	void set_location(int x, int y);

	void render();
	void unrender();

	bool expired() const;
private:

	const std::string& current_image();
	void rezoom();

	animated<std::string> images_;

	std::string current_image_;

	ORIENTATION orientation_;

	int origx_, origy_, x_, y_;
	double origzoom_, zoom_;
	surface surf_, buffer_;
	SDL_Rect rect_;
};

std::map<int,effect> haloes;
int halo_id = 1;

bool hide_halo = false;


effect::effect(int xpos, int ypos, const animated<std::string>::anim_description& img, ORIENTATION orientation,bool infinite)
: images_(img), orientation_(orientation), origx_(xpos), origy_(ypos), x_(xpos), y_(ypos),
  origzoom_(disp->zoom()), zoom_(disp->zoom()), rect_(empty_rect)
{
	wassert(disp != NULL);
	// std::cerr << "Constructing halo sequence from image " << img << "\n";

	set_location(xpos,ypos);

	images_.start_animation(0,infinite);

	if(!images_.animation_finished()) {
		images_.update_last_draw_time();
	}

	current_image_ = "";
	rezoom();
}

void effect::set_location(int x, int y)
{
	const gamemap::location zero_loc(0,0);
	x_ = origx_ = x - disp->get_location_x(zero_loc);
	y_ = origy_ = y - disp->get_location_y(zero_loc);
	origzoom_ = disp->zoom();

	if(zoom_ != origzoom_) {
		rezoom();
	}
}

const std::string& effect::current_image()
{
	static const std::string r = "";

	const std::string& res = images_.get_current_frame();

	return res;
}

void effect::rezoom()
{
}

void effect::render()
{
	if(disp == NULL) {
		return;
	}

	images_.update_last_draw_time();
	const std::string& img_name = current_image();

	const gl::image& img = gl::get_image(img_name);

	const gamemap::location zero_loc(0,0);
	const int screenx = disp->get_location_x(zero_loc);
	const int screeny = disp->get_location_y(zero_loc);

	const int xpos = x_ + screenx - img.width()/2;
	const int ypos = y_ + screeny - img.height()/2;

	SDL_Rect rect = {xpos,ypos,img.width(),img.height()};
	rect_ = rect;
	SDL_Rect clip_rect = disp->map_area();
	if(rects_overlap(rect,clip_rect) == false) {
		buffer_.assign(NULL);
		return;
	}

	img.draw(xpos,ypos);
}

void effect::unrender()
{
}

bool effect::expired() const
{
	return images_.animation_finished();
}

}

manager::manager(display& screen) : old(disp)
{
	disp = &screen;
}

manager::~manager()
{
	haloes.clear();
	disp = old;
}

halo_hider::halo_hider() : old(hide_halo)
{
	render();
	hide_halo = true;
}

halo_hider::~halo_hider()
{
	hide_halo = old;
	unrender();
}

int add(int x, int y, const std::string& image, ORIENTATION orientation, bool infinite)
{
	const int id = halo_id++;
	animated<std::string>::anim_description image_vector;
	std::vector<std::string> items = utils::split(image);
	std::vector<std::string>::const_iterator itor = items.begin();
	for(; itor != items.end(); ++itor) {
		const std::vector<std::string>& items = utils::split(*itor, ':');
		std::string str;
		int time;

		if(items.size() > 1) {
			str = items.front();
			time = atoi(items.back().c_str());
		} else {
			str = *itor;
			time = 100;
		}
		image_vector.push_back(animated<std::string>::frame_description(time,std::string(str)));

	}
	haloes.insert(std::pair<int,effect>(id,effect(x,y,image_vector,orientation,infinite)));
	return id;
}

void set_location(int handle, int x, int y)
{
	const std::map<int,effect>::iterator itor = haloes.find(handle);
	if(itor != haloes.end()) {
		itor->second.set_location(x,y);
	}
}

void remove(int handle)
{
	haloes.erase(handle);
}

void render()
{
	if(hide_halo || preferences::show_haloes() == false) {
		return;
	}

	for(std::map<int,effect>::iterator i = haloes.begin(); i != haloes.end(); ) {
		if(i->second.expired()) {
			haloes.erase(i++);
		} else {
			i->second.render();
			++i;
		}
	}
}

void unrender()
{
	if(hide_halo || preferences::show_haloes() == false) {
		return;
	}

	for(std::map<int,effect>::reverse_iterator i = haloes.rbegin(); i != haloes.rend(); ++i) {
		i->second.unrender();
	}
}

}
