#pragma once
#include "../../Datatypes/vec.h"
#include "../../Datatypes/animation.h"
#include "anchor.h"
#include <unordered_map>
#include <list>
#include <string>
#include "../../draw.h"
#include "../../constants.h"

using namespace std;
struct SpriteObject {
	SpriteObject();
	~SpriteObject();
	bool root;
	Vec pos;
	Layer current_layer;
	int dir;
	string current_animation_name;
	bool animation_done;

	SDL_RendererFlip renderer_flip;
	void AddAnimation(string name, Animation animParam);
	void SetPivot(string anchor_name);
	Vec GetPivotRelativePos();
	Anchor& AddAnchor(string name);
	Anchor& GetAnchor(string name);
	void SetAngle(float angle_param);
	//! Sets the angle to angle_param
	void Rotate(float angle_param);
	//! Changes the angle by angle_param
	float GetAngle();
	void AlignAll();
	void Draw();
	void DisplayAnchors();
	int GetAnimationFrame();
protected:
	friend class Anchor;
	int animation_frame;
	unordered_map<string, Anchor> anchor_map;
	unordered_map<string, Animation> animation_map;
	void Align();
	static int alignCallCounter; //used to prevent infinite recursion;
	SDL_Rect current_sprite_rect;
	SDL_Rect current_screen_rect;
private:
	float angle;
	Anchor* pivot_anchor;
	Vec pivot_rel_pos;
	string prev_animation_state;
};