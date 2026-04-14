#pragma once
#include "../../Datatypes/vec.h"
#include <unordered_map>
#include <utility>
#include <string>
struct SpriteObject;
using namespace std;
struct Anchor {
	~Anchor();
	void SwitchAnimationState(string new_state);
	void AddAnimationState(string new_state, Vec alt_pos);
	void operator>>(Anchor& target);
	void FlipCorrect();
	Vec GetPos();
	Vec Getrel_pos();
protected:
	friend struct SpriteObject;
	Anchor* child_ptr;
	SpriteObject* my_sprite;
	Vec rel_pos;
	string current_state;
	// pair <NORMAL POS, FLIPPED POS>
	unordered_map<string, tuple<Vec, Vec, Vec, Vec>> rel_pos_map;

};