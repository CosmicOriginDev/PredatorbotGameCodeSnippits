#include "spriteObject.h"
#include "../../camera.h"
#include "../../debug.h"
using namespace std;
extern SDL_Renderer* game_renderer;
int SpriteObject::alignCallCounter;
SpriteObject::SpriteObject()
{
	animation_frame = 0;
	current_layer = FGFRONT;
	current_screen_rect = SDL_Rect();
	current_sprite_rect = SDL_Rect();
	renderer_flip = SDL_FLIP_NONE;
	angle = 0;
	pivot_rel_pos=Vec();
	root = false;
	pivot_anchor = nullptr;
	pos = Vec();
}
SpriteObject::~SpriteObject()
{
	anchor_map.clear();
	animation_map.clear();
}
void SpriteObject::AlignAll()
{
	if (anchor_map.size() <= 0)
		return;

	if (root)
	{
		alignCallCounter = 0;
		Align();
	}
	else
	{
		unordered_map<string, Anchor>::iterator it = anchor_map.begin();
		while (it != anchor_map.end())
		{
			throw std::runtime_error("AlignAll is not on a root! Recursively searching for root! This will slow down the program!");
			it->second.my_sprite->AlignAll(); // if not a root sprite, call this function to find the root
			it++;
		}
	}
}
void SpriteObject::Align()
{
	alignCallCounter++;
	if (alignCallCounter > 100)
	{
		throw(std::runtime_error("Align Call limit exceeded! :( Check for Anchor loops!"));
		exit(EXIT_FAILURE);
	}
	if (anchor_map.size() <= 0)
		return;

	unordered_map<string, Anchor>::iterator it= anchor_map.begin();
	while (it != anchor_map.end())
	{
		it->second.SwitchAnimationState(current_animation_name);
		it->second.FlipCorrect();
		if (it->second.child_ptr != nullptr)
		{
			string childAnim = it->second.child_ptr->my_sprite->current_animation_name;
			it->second.child_ptr->my_sprite->pos = this->pos + this->animation_map[current_animation_name].frame_offset + it->second.Getrel_pos() - it->second.child_ptr->Getrel_pos() - it->second.child_ptr->my_sprite->animation_map[childAnim].frame_offset;
			it->second.child_ptr->my_sprite->Align();
		}
		it++;
	}

}
void SpriteObject::SetPivot(string anchor_name)
{
	pivot_anchor = &GetAnchor(anchor_name);
}
void SpriteObject::AddAnimation(string name, Animation animParam)
{
	animation_map[name] = animParam;
}
Vec SpriteObject::GetPivotRelativePos()
{
	return pivot_anchor->Getrel_pos();
}
void SpriteObject::SetAngle(float angle_param)
{
	angle = angle_param;
}
void SpriteObject::Rotate(float angle_param)
{
	angle += angle_param;
}
float SpriteObject::GetAngle()
{
	return angle;
}
Anchor& SpriteObject::AddAnchor(string name)
{
	Anchor& new_anchor = anchor_map[name];
	new_anchor.my_sprite = this;
	new_anchor.child_ptr = nullptr;
	return new_anchor;
	
}
Anchor& SpriteObject::GetAnchor(string name)
{
	return anchor_map.find(name)->second;
}
void SpriteObject::Draw()
{
	
	if (prev_animation_state != current_animation_name)
	{
		animation_frame = 0;
		prev_animation_state = current_animation_name;
	}
	
	Animation current_animation = animation_map[current_animation_name];

	if (current_animation.sheet == nullptr)
	{
		throw(std::runtime_error("Missing texture on SpriteObject!"));
		return;
	}
	if (current_animation.loop)
	{
		if (animation_frame >= current_animation.frame_count)
		{
			animation_frame = 0;
			animation_done = true;
		}
		else
		{
			animation_done = false;
		}
	}
	else
	{
		if (animation_frame >= current_animation.frame_count)
		{
			animation_done = true;
			if (!current_animation.next.empty())
			{
				current_animation_name = current_animation.next;
				animation_frame = 0;
			}
			else
			{
				animation_frame = current_animation.frame_count - 1;
			}
		}
		else
		{
			animation_done = false;
		}

	}
	current_sprite_rect = SDL_Rect(animation_frame * animation_map[current_animation_name].frame_dim.w, 0, animation_map[current_animation_name].frame_dim.w, animation_map[current_animation_name].frame_dim.h);
	current_screen_rect = SDL_Rect(pos.x - Camera::GetCameraPos().x + animation_map[current_animation_name].frame_offset.x, pos.y - Camera::GetCameraPos().y + animation_map[current_animation_name].frame_offset.y, animation_map[current_animation_name].frame_dim.w, animation_map[current_animation_name].frame_dim.h);
	
	SDL_SetRenderTarget(game_renderer, DrawSystem::layer_textures[current_layer]);
	if (pivot_anchor != nullptr)
	{
		pivot_rel_pos = pivot_anchor->Getrel_pos();
	}
	SDL_Point pivot_point = SDL_Point(pivot_rel_pos.x, pivot_rel_pos.y);
	int ret = SDL_RenderCopyEx(game_renderer, animation_map[current_animation_name].sheet.get(), &current_sprite_rect, &current_screen_rect, angle * (180 / PI), &pivot_point, renderer_flip);
	if (ret < 0)
	{
		std::string error = "ERROR WITH DRAWING SPRITEOBJECT: " + std::string::basic_string(SDL_GetError());
		std::cout << error << std::endl;
		throw(std::runtime_error(error));
		exit(EXIT_FAILURE);
	}
	animation_frame++;
}
void SpriteObject::DisplayAnchors()
{
	unordered_map<string, Anchor>::iterator it = anchor_map.begin();
	while (it != anchor_map.end())
	{
		Vec v = this->pos + it->second.rel_pos+this->animation_map[current_animation_name].frame_offset;
		Debug::DrawCross(v.x,v.y);
		it++;
	}
}

int SpriteObject::GetAnimationFrame()
{
	return animation_frame;
}
