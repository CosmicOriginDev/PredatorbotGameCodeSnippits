#include "vec.h"
#include <algorithm>
#include <string>
#include "../tileManager.h"
#include <cmath>
#include "../camera.h"
#include <utility>
#include "rnd.h"

Vec::Vec()
{
	x = 0;
	y = 0;
}
Vec::Vec(float xParam, float yParam)
{
	x = xParam;
	y = yParam;
}
Vec::Vec(Vec dir, float mag)
{
	Vec v = Vec::norm(dir)*mag;
	x = v.x;
	y = v.y;
}
Vec::operator std::string() { 
	std::string output = "<" + std::to_string(x) + ", " + std::to_string(y) + ">";
	return output;
}
Vec& Vec::operator=(Vec b)
{
	if (this != &b)
	{
		this->x = b.x;
		this->y = b.y;
	}
	return *this;
}


// check if two vectors are equal
bool operator == (const Vec & a, const Vec & b)
{
	return(a.x == b.x && a.y == b.y);
}
// Add two vectors
Vec Vec::operator+(Vec b)
{
	return Vec(this->x + b.x, this->y + b.y);
}
// Subtract two vectors
Vec Vec::operator-(Vec b)
{
	return Vec(this->x - b.x, this->y - b.y);
}
// Scale a vector
Vec Vec::operator*(float b)
{
	return Vec(this->x * b, this->y * b);
}
// Scale a vector by a vector
Vec Vec::operator*(Vec b)
{
	return Vec(this->x * b.x, this->y * b.y);
}
float Vec::dot(Vec a, Vec b)
{
	return a.x * b.x + a.y * b.y;
}
float Vec::length(Vec a)
{
	return sqrt(pow(a.x, 2) + pow(a.y, 2));
};
float Vec::distance(Vec a, Vec b)
{
	return sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2));
};
float Vec::manhattan_distance(Vec a, Vec b)
{
	return abs(a.x - b.x) + abs(a.y - b.y);
}
Vec Vec::floor(Vec a)
{
	return Vec((int) a.x, (int)a.y);
}
Vec Vec::norm(Vec a)
{
	float l = Vec::length(a);
	Vec out = Vec(a.x / l, a.y / l);
	return out;
}
Vec Vec::clamp(Vec in, Vec min, Vec max)
{
	Vec out = Vec();
	// Make sure min is always less than max. 
	// If this is not the case swap values.
	if (min.x > max.x)
	{
		std::swap(min.x, max.x);
	}
	if (min.y > max.y)
	{
		std::swap(min.y, max.y);
	}
	out.x = std::clamp(in.x, min.x, max.x);
	out.y = std::clamp(in.y, min.y, max.y);
	return out;
}
Vec Vec::clamp(Vec in, float min, float max)
{
	if (Vec::length(in) > max)
	{
		return Vec::norm(in) * max;
	}
	else if (Vec::length(in) < min)
	{
		return Vec::norm(in) * min;
	}
	else
	{
		return in;
	}
}
Vec Vec::sign(Vec a)
{
	Vec out = Vec(a.x / abs(a.x), a.y / abs(a.y));
	return out;
}
//linearly interpolate between two vectors
Vec Vec::lerp(Vec a, Vec b, float p)
{
	float t = std::clamp(p, (float)0, (float)1);
	return Vec(a*(1 - p) + (b*p));
}
float Vec::inverseLerp(Vec a, Vec b, Vec c)
{
	Vec x = Vec::clamp(c, a, b);
	return Vec::dot(x - a, b - a) / Vec::dot(b-a,b-a);
}
Vec Vec::snapTile(Vec a)
{
	Vec b = Vec::floor(a);
	int x = b.x;
	int y = b.y;
	x -= x % TILE_SIZE;
	y -= y % TILE_SIZE;
	return Vec(x, y);
}
Vec Vec::rot(Vec a, double angle) // rotates a Vec by an angle in radians
{
	double cosTheta = std::cos(angle);
	double sinTheta = std::sin(angle);
	Vec out = Vec(
		a.x * cosTheta - a.y * sinTheta,
		a.x * sinTheta + a.y * cosTheta
	);
	return out;
		
	
}
float Vec::angle(Vec dir)
{
	if (dir == Vec())
	{
		return 0;
	}
	else
	{
		return atan2f(dir.y,dir.x);
	}
}
float Vec::angle_diff(Vec a, Vec b)
{
	if (a == Vec() || b == Vec() || a == b)
	{
		return 0;
	}
	else
	{
		return atan2f(a.x*b.y-a.y*b.x, a.x*b.x+a.y*b.y)-(PI/2);
	}
}
Vec Vec::perp(Vec a)
{
	return Vec(-a.y, a.x);
}
Vec Vec::worldSpaceToScreenSpace(Vec a)
{
	Vec camVec = Vec(Camera::GetCameraPos().x, Camera::GetCameraPos().y);
	return a - camVec;
}
Vec Vec::screenSpaceToWorldSpace(Vec a)
{
	Vec camVec = Vec(Camera::GetCameraPos().x, Camera::GetCameraPos().y);
	return a + camVec;
}
Vec Vec::randomDir()
{
	float x = (Rnd::Random() * 2) - 1;
	float y = (Rnd::Random() * 2) - 1;
	return Vec(x, y);
}
