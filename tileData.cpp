#include "tileData.h"
#include <iostream>
#include "sector.h"
using namespace std;

std::mutex TileData::tile_data_mutex;
unordered_map<int, string> TileData::tile_classes;
unordered_map<string, int> TileData::tile_classes_id;
unordered_map<int, unordered_map<string, bool>> TileData::bool_tile_properties;
unordered_map<int, unordered_map<string,int>> TileData::int_tile_properties;
unordered_map<int, unordered_map<string, float>> TileData::float_tile_properties;
unordered_map<int, unordered_map<string, string>> TileData::string_tile_properties;
void TileData::Clear()
{
	lock_guard<mutex> lock(tile_data_mutex);
	tile_classes.clear();
	tile_classes_id.clear();
	bool_tile_properties.clear();
	int_tile_properties.clear();
	float_tile_properties.clear();
	string_tile_properties.clear();
	master_tileset_map.clear();
}
void TileData::AddClass(int id, string data)
{
	lock_guard<mutex> lock(tile_data_mutex);
	tile_classes.insert({ id, data });
	tile_classes_id.insert({ data, id });
}
void TileData::AddProperty(int id, string name, int data)
{
	lock_guard<mutex> lock(tile_data_mutex);
	if (int_tile_properties.contains(id) == false)
		int_tile_properties.insert({ id, unordered_map<string, int>() });
	int_tile_properties[id].insert({ name, data });
}
void TileData::AddProperty(int id, string name, float data)
{
	lock_guard<mutex> lock(tile_data_mutex);
	if (float_tile_properties.contains(id) == false)
		float_tile_properties.insert({ id, unordered_map<string, float>() });
	float_tile_properties[id].insert({ name, data });
}
void TileData::AddProperty(int id, string name, string data)
{
	lock_guard<mutex> lock(tile_data_mutex);
	if (string_tile_properties.contains(id) == false)
		string_tile_properties.insert({ id, unordered_map<string, string>() });
	string_tile_properties[id].insert({ name, data });
}
void TileData::AddProperty(int id, string name, bool data)
{
	lock_guard<mutex> lock(tile_data_mutex);
	if (bool_tile_properties.contains(id) == false)
		bool_tile_properties.insert({ id, unordered_map<string, bool>() });
	bool_tile_properties[id].insert({ name, data });
}
bool TileData::HasClass(int id)
{
	if (id == 0) // don't bother checking if id is 0 because empty cant have classes
		return false;
	lock_guard<mutex> lock(tile_data_mutex);
	return tile_classes.contains(id);
}
string TileData::GetClass(int id)
{
	if (id == 0) // don't bother checking if id is 0 because empty cant have classes
		return "";
	lock_guard<mutex> lock(tile_data_mutex);
	auto tile_class = tile_classes.find(id);
	if (tile_class == tile_classes.end())
		return "";
	return tile_class->second;
}
int TileData::GetId(string className)
{
	lock_guard<mutex> lock(tile_data_mutex);
	auto tile_class_id = tile_classes_id.find(className);
	if (tile_class_id == tile_classes_id.end())
		return 0;
	return tile_class_id->second;
}
bool TileData::HasProperty(int id, string name)
{
	if (id == 0) // don't bother checking if id is 0 because empty cant have properties
		return false;

	lock_guard<mutex> lock(tile_data_mutex);
	auto string_props = string_tile_properties.find(id);
	auto int_props = int_tile_properties.find(id);
	auto float_props = float_tile_properties.find(id);
	auto bool_props = bool_tile_properties.find(id);
	return (string_props != string_tile_properties.end() && string_props->second.contains(name)) ||
		(int_props != int_tile_properties.end() && int_props->second.contains(name)) ||
		(float_props != float_tile_properties.end() && float_props->second.contains(name)) ||
		(bool_props != bool_tile_properties.end() && bool_props->second.contains(name));
}

bool TileData::GetBoolPropertyValue(int id, string name)
{
	if (id == 0) // don't bother checking if id is 0 because empty cant have properties
		return false;
	lock_guard<mutex> lock(tile_data_mutex);
	auto bool_props = bool_tile_properties.find(id);
	if (bool_props == bool_tile_properties.end() || bool_props->second.contains(name) == false)
		return false;
	return bool_props->second.at(name);
}

int TileData::GetIntPropertyValue(int id, string name)
{
	if (id == 0) // don't bother checking if id is 0 because empty cant have properties
		return 0;
	lock_guard<mutex> lock(tile_data_mutex);
	auto int_props = int_tile_properties.find(id);
	if (int_props == int_tile_properties.end() || int_props->second.contains(name) == false)
		return 0;
	return int_props->second.at(name);
}

float TileData::GetFloatPropertyValue(int id, string name)
{
	if (id == 0) // don't bother checking if id is 0 because empty cant have properties
		return 0.0f;
	lock_guard<mutex> lock(tile_data_mutex);
	auto float_props = float_tile_properties.find(id);
	if (float_props != float_tile_properties.end() && float_props->second.contains(name))
		return float_props->second.at(name);

	auto int_props = int_tile_properties.find(id);
	if (int_props != int_tile_properties.end() && int_props->second.contains(name))
		return static_cast<float>(int_props->second.at(name));

	return 0.0f;
}

string TileData::GetStringPropertyValue(int id, string name)
{
	if (id == 0) // don't bother checking if id is 0 because empty cant have properties
		return "";
	lock_guard<mutex> lock(tile_data_mutex);
	auto string_props = string_tile_properties.find(id);
	if (string_props == string_tile_properties.end() || string_props->second.contains(name) == false)
		return "";
	return string_props->second.at(name);
}

void TileData::DisplayData()
{
	lock_guard<mutex> lock(tile_data_mutex);
	for (const auto& [id, className] : tile_classes) {
		cout << "id = " << id << endl;
		cout << "class = " << className << endl;
		cout << "properties:" << endl;
		if (int_tile_properties.contains(id))
		{
			for (const auto& [name, data] : int_tile_properties[id]) {
				cout << "	int "<<name << " = " << data << endl;
			}
		}
		if (float_tile_properties.contains(id))
		{
			for (const auto& [name, data] : float_tile_properties[id]) {
				cout << "	float " << name << " = " << data << endl;
			}
		}
		if (string_tile_properties.contains(id))
		{
			for (const auto& [name, data] : string_tile_properties[id]) {
				cout << "	string " << name << " = " << data << endl;
			}
		}
		if (bool_tile_properties.contains(id))
		{
			for (const auto& [name, data] : bool_tile_properties[id]) {
				cout << "	bool " << name << " = " << data << endl;
			}
		}
		cout << endl;
	}
}
