#include "anchor.h"
#include "spriteObject.h"
#include <stdexcept>

Anchor::~Anchor()
{
	child_ptr = nullptr;
	rel_pos_map.clear();
};
void Anchor::SwitchAnimationState(std::string new_state)
{
	if (my_sprite->animation_map.count(new_state) > 0)
	{
		current_state = new_state;
	}
}
void Anchor::AddAnimationState(std::string new_state, Vec alt_pos)
{
	if (my_sprite->animation_map.count(new_state) <= 0)
	{
		throw(std::runtime_error("Attempted to add anchor state to an Animation that doesn't exist!"));
		exit(EXIT_FAILURE);
	}
	Vec norm_pos = alt_pos;
	std::tuple<Vec, Vec, Vec, Vec> flip_positions = make_tuple(
		alt_pos, // NO FLIP
		Vec(my_sprite->animation_map[new_state].frame_dim.w - alt_pos.x, 0) + Vec(0, alt_pos.y), // FLIP HORIZONTAL
		Vec(0, my_sprite->animation_map[new_state].frame_dim.h - alt_pos.y) + Vec(alt_pos.x, 0), // FLIP VERTICAL 
		Vec(my_sprite->animation_map[new_state].frame_dim.w - alt_pos.x, my_sprite->animation_map[new_state].frame_dim.h - alt_pos.y) + Vec(alt_pos.x, alt_pos.y) // FLIP BOTH
	);
	rel_pos_map.insert({ new_state, flip_positions });
	current_state = new_state;
}
void Anchor::FlipCorrect()
{
	if (rel_pos_map.contains(current_state))
	{
		if (my_sprite->renderer_flip == SDL_FLIP_NONE)
		{
			rel_pos = std::get<0>(rel_pos_map.find(current_state)->second);
		}
		else if (my_sprite->renderer_flip == SDL_FLIP_HORIZONTAL)
		{
			rel_pos = std::get<1>(rel_pos_map.find(current_state)->second);
		}
		else if (my_sprite->renderer_flip == SDL_FLIP_VERTICAL)
		{
			rel_pos = std::get<2>(rel_pos_map.find(current_state)->second);
		}
		else if (my_sprite->renderer_flip == (SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL))
		{
			rel_pos = std::get<3>(rel_pos_map.find(current_state)->second);
		}
	}
}
void Anchor::operator>>(Anchor& target)
{
	this->child_ptr = &target;
}
Vec Anchor::GetPos()
{
	return Anchor::Getrel_pos() + my_sprite->pos;
}
Vec Anchor::Getrel_pos()
{
	if (my_sprite->pivot_anchor != nullptr && this != my_sprite->pivot_anchor)
	{
		Vec pos_relative_to_pivot = rel_pos - my_sprite->GetPivotRelativePos();
		Vec rotated_pos_relative_to_pivot = Vec::rot(pos_relative_to_pivot, my_sprite->GetAngle());
		Vec origin_pivoted_pos = rotated_pos_relative_to_pivot + my_sprite->GetPivotRelativePos();
		return origin_pivoted_pos;
	}
	else
	{
		return rel_pos;
	}
}