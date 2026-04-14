#pragma once
#include <string>
#include <bit>
union Vec
{
	struct
	{
		float x;
		float y;
	};
	//used for width and height
	struct
	{
		float w;
		float h;
	};
	//! Creates a new empy Vec
	Vec();
	//! Creates a Vec with an x of xParam and a y of yParam 
	Vec(float xParam, float yParam);
	//! Creates a Vec facing dir with a length of mag
	Vec(Vec dir, float mag);
	//! Used to help display vectors on the command line for debugging
	operator std::string();
	//! tests if two vecs have the same x and same y
	friend bool operator==(const Vec &a, const Vec &b);
	//! Assigns a Vec to be equal to another Vec
	Vec& operator=(Vec b);
	//! Adds vecs <a.x+b.x, a.y+b.y>
	Vec operator+(Vec b);
	//! Subtracts vecs <a.x-b.x, a.y-b.y>
	Vec operator-(Vec b);
	//! Scales a Vec <a.x*b, a.y*b>
	Vec operator*(float b);
	//! Scales a Vec by a Vec
	Vec operator*(Vec b);
	//! Dot product
	static float dot(Vec a, Vec b);
	//! Returns the length of a Vec
	static float length(Vec a);
	//! Returns how far apart two vecs are.
	static float distance(Vec a, Vec b);
	//! Returns approx distance. Fast and sloppy.
	static float manhattan_distance(Vec a, Vec b);
	//! Rounds down the x and y of a Vec to the nearest integer
	static Vec floor(Vec a);
	//! Sets the Vec's length to be 1 while still maintaining the same direction
	static Vec norm(Vec a);
	/**
	* Returns a Vec where:
	* if a.x is positive, then the new Vec's x is 1
	* if a.x is negative, then the new Vec's x is -1
	* if a.y is positive, then the new Vec's y is 1
	* if a.y is negative, then the new Vec's y is -1
	*/
	static Vec sign(Vec a);
	//! returns a Vec that moves from a to b as p moves from 0 to 1.
	static Vec lerp(Vec a, Vec b, float p);
	//! returns a float representing how far c in between a and b from 0 to 1
	static float inverseLerp(Vec a, Vec b, Vec c);
	//! Snaps a Vec to the top left corner of the tile grid
	static Vec snapTile(Vec a);
	//! Returns a new Vec equal to in, but clamped between min and max
	static Vec clamp(Vec in, Vec min, Vec max);
	//! Returns a new Vec equal to in, but clamped between min and max by length
	static Vec clamp(Vec in, float min, float max);
	//! Rotates a Vec by angle in radians
	static Vec rot(Vec a, double angle);
	//! Returns the angle a Vec makes in radians
	static float angle(Vec dir);
	//! Returns the angle between 2 vectors in radians.
	static float angle_diff(Vec a, Vec b);
	//! Returns a Vec perpendicular to a
	static Vec perp(Vec a);
	//! Converts a Vec in the world to where it is on the screen
	static Vec worldSpaceToScreenSpace(Vec a);
	//! Converts a Vec on the screen to where it is in the world
	static Vec screenSpaceToWorldSpace(Vec a);
	//! Returns a random direction
	static Vec randomDir();

};
namespace std
{
	template<>
	struct hash<Vec>
	{
		std::size_t operator() (const Vec& v) const
		{
			int hx = floor(v.x);
			int hy = floor(v.y);
			return static_cast<size_t>(hx) ^ static_cast<size_t>(hy) << 1;
		}

	};
}
