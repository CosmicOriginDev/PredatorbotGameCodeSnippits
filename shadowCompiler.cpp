#include "shadowCompiler.h"

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../Datatypes/color.h"
#include "../Datatypes/room.h"
#include "../constants.h"
#include "sector.h"
#include "tileData.h"

extern SDL_Renderer* game_renderer;

constexpr Uint8 MAX_SHADOW_ALPHA = 200;
constexpr Uint8 NATURAL_LIGHT_ALPHA = 0;
constexpr Uint8 MIN_POINT_LIGHT_ALPHA = 36;
constexpr Uint8 MAX_POINT_LIGHT_ALPHA = 96;
constexpr float MIN_LIGHT_INTENSITY = 0.05f;
constexpr float TILE_LIGHT_RADIUS = 6.0f;
constexpr float LIGHT_FALLOFF_POWER = 3.0f;
constexpr float HUE_FALLOFF_POWER = 2.0f;
constexpr int LIGHT_BLEED_PASSES = 32;
constexpr Uint8 LIGHT_BLEED_STEP = TILE_SIZE;

namespace
{
	struct LightingPixel
	{
		ColorNames color_name = black;
		float hue_influence = 0.0f;
		Uint8 a = MAX_SHADOW_ALPHA;
	};

	struct ShadowTexel
	{
		Uint8 r = 0;
		Uint8 g = 0;
		Uint8 b = 0;
		Uint8 a = MAX_SHADOW_ALPHA;
	};

	struct ShadowJob
	{
		Room room_snapshot;
		uint64_t request_id = 0;
		uint64_t compiler_generation = 0;
	};

	struct ShadowResult
	{
		std::string room_name;
		int width = 0;
		int height = 0;
		std::vector<ShadowTexel> texels;
		uint64_t request_id = 0;
		uint64_t compiler_generation = 0;
	};

	std::mutex shadow_queue_mutex;
	std::condition_variable shadow_queue_cv;
	std::condition_variable shadow_completion_cv;
	std::queue<ShadowJob> pending_shadow_jobs;
	std::queue<ShadowResult> completed_shadow_jobs;
	std::unordered_map<std::string, uint64_t> latest_room_requests;
	std::thread shadow_worker_thread;
	bool shadow_worker_running = false;
	bool shadow_worker_stop_requested = false;
	uint64_t active_compiler_generation = 1;
	size_t active_shadow_jobs = 0;

	float GetTileLightIntensity(int tile_id)
	{
		if (TileData::HasProperty(tile_id, "light_intensity"))
		{
			return std::max(0.0f, TileData::GetFloatPropertyValue(tile_id, "light_intensity"));
		}

		return 1.0f;
	}

	float GetTileColorIntensity(int tile_id)
	{
		if (TileData::HasProperty(tile_id, "color_intensity"))
		{
			return std::max(0.0f, TileData::GetFloatPropertyValue(tile_id, "color_intensity"));
		}

		return 1.0f;
	}

	float GetLightRadius(float light_intensity)
	{
		return TILE_LIGHT_RADIUS * std::max(0.0f, light_intensity);
	}

	SDL_Color BlendLightColor(ColorNames light_color_name, float tint_strength)
	{
		const float clamped_tint_strength = std::clamp(tint_strength, 0.0f, 1.0f);
		const SDL_Color base_color = color(white);
		const SDL_Color tint_color = color(light_color_name);
		return SDL_Color
		{
			static_cast<Uint8>(std::round(
				(static_cast<float>(base_color.r) * (1.0f - clamped_tint_strength)) +
				(static_cast<float>(tint_color.r) * clamped_tint_strength))),
			static_cast<Uint8>(std::round(
				(static_cast<float>(base_color.g) * (1.0f - clamped_tint_strength)) +
				(static_cast<float>(tint_color.g) * clamped_tint_strength))),
			static_cast<Uint8>(std::round(
				(static_cast<float>(base_color.b) * (1.0f - clamped_tint_strength)) +
				(static_cast<float>(tint_color.b) * clamped_tint_strength))),
			255
		};
	}

	ColorNames GetTileLightColorName(int tile_id)
	{
		if (TileData::HasProperty(tile_id, "light_color"))
		{
			return static_cast<ColorNames>(TileData::GetIntPropertyValue(tile_id, "light_color"));
		}

		return white;
	}

	ShadowResult BuildShadowResult(const ShadowJob& job)
	{
		const Room& room = job.room_snapshot;
		ShadowResult result;
		result.room_name = room.name;
		result.width = room.dim.w;
		result.height = room.dim.h;
		result.request_id = job.request_id;
		result.compiler_generation = job.compiler_generation;

		if (room.dim.w <= 0 || room.dim.h <= 0)
		{
			return result;
		}

		std::vector<std::vector<Uint8>> shadow_alphas(
			room.dim.h,
			std::vector<Uint8>(room.dim.w, MAX_SHADOW_ALPHA));
		std::vector<std::vector<Uint8>> natural_shadow_alphas(
			room.dim.h,
			std::vector<Uint8>(room.dim.w, MAX_SHADOW_ALPHA));
		ShadowCompiler::ApplyNaturalLight(room, shadow_alphas);
		ShadowCompiler::ApplyNaturalLight(room, natural_shadow_alphas);

		for (int y = 0; y < room.dim.h; ++y)
		{
			for (int x = 0; x < room.dim.w; ++x)
			{
				Vec tile_pos = Vec(x, y);
				const int fg_tile = room.map_data_fg[y][x];

				if (ShadowCompiler::IsLightSource(fg_tile))
				{
					ShadowCompiler::ApplyPointLight(room, tile_pos, shadow_alphas, GetTileLightIntensity(fg_tile), GetTileColorIntensity(fg_tile));
				}
			}
		}

		ShadowCompiler::ApplyLightBleed(room, shadow_alphas);

		std::vector<std::vector<LightingPixel>> lighting_pixels(
			room.dim.h,
			std::vector<LightingPixel>(room.dim.w));
		ColorNames natural_light_color_name = room.filter_color == NULL ? white : room.filter_color;

		for (int y = 0; y < room.dim.h; ++y)
		{
			for (int x = 0; x < room.dim.w; ++x)
			{
				lighting_pixels[y][x].a = shadow_alphas[y][x];
				const float natural_hue_influence = std::clamp(
					(static_cast<float>(MAX_SHADOW_ALPHA) - static_cast<float>(natural_shadow_alphas[y][x])) / static_cast<float>(MAX_SHADOW_ALPHA),
					0.0f,
					1.0f);
				lighting_pixels[y][x].color_name = natural_light_color_name;
				lighting_pixels[y][x].hue_influence = natural_hue_influence;
			}
		}

		for (int y = 0; y < room.dim.h; ++y)
		{
			for (int x = 0; x < room.dim.w; ++x)
			{
				const int fg_tile = room.map_data_fg[y][x];
				if (ShadowCompiler::IsLightSource(fg_tile) == false)
				{
					continue;
				}

				Vec light_pos = Vec(x, y);
				ColorNames light_color_name = GetTileLightColorName(fg_tile);
				const float light_intensity = GetTileLightIntensity(fg_tile);
				const float light_radius = GetLightRadius(light_intensity);
				const float color_intensity = GetTileColorIntensity(fg_tile);
				if (light_radius <= 0.0f)
				{
					continue;
				}

				const int min_x = std::max(0, static_cast<int>(std::floor(light_pos.x - light_radius)));
				const int max_x = std::min(static_cast<int>(room.dim.w) - 1, static_cast<int>(std::ceil(light_pos.x + light_radius)));
				const int min_y = std::max(0, static_cast<int>(std::floor(light_pos.y - light_radius)));
				const int max_y = std::min(static_cast<int>(room.dim.h) - 1, static_cast<int>(std::ceil(light_pos.y + light_radius)));

				for (int ly = min_y; ly <= max_y; ++ly)
				{
					for (int lx = min_x; lx <= max_x; ++lx)
					{
						Vec tile_pos = Vec(lx, ly);
						const float distance = Vec::distance(tile_pos, light_pos);
						if (distance > light_radius)
						{
							continue;
						}

						if ((tile_pos == light_pos) == false && ShadowCompiler::HasLineOfSight(room, light_pos, tile_pos) == false)
						{
							continue;
						}

						const float normalized_distance = std::clamp(distance / light_radius, 0.0f, 1.0f);
						const float falloff = std::pow(std::max(0.0f, 1.0f - normalized_distance), LIGHT_FALLOFF_POWER);
						const float contribution = std::clamp(falloff, 0.0f, 1.0f);
						if (contribution <= 0.0f)
						{
							continue;
						}

						const float hue_falloff = std::pow(contribution, HUE_FALLOFF_POWER);
						const float hue_influence = std::clamp(
							std::max(contribution, hue_falloff * color_intensity),
							0.0f,
							1.0f);
						LightingPixel& pixel = lighting_pixels[ly][lx];
						if (hue_influence >= pixel.hue_influence)
						{
							pixel.color_name = light_color_name;
							pixel.hue_influence = hue_influence;
						}
					}
				}
			}
		}

		result.texels.resize(static_cast<size_t>(room.dim.w) * static_cast<size_t>(room.dim.h));
		for (int y = 0; y < room.dim.h; ++y)
		{
			for (int x = 0; x < room.dim.w; ++x)
			{
				const int pixel_index = (y * room.dim.w) + x;
				const SDL_Color palette_color = BlendLightColor(lighting_pixels[y][x].color_name, lighting_pixels[y][x].hue_influence);
				const float visible_light = std::clamp(
					(static_cast<float>(MAX_SHADOW_ALPHA) - static_cast<float>(lighting_pixels[y][x].a)) / static_cast<float>(MAX_SHADOW_ALPHA),
					0.0f,
					1.0f);
				ShadowTexel& texel = result.texels[pixel_index];
				texel.r = static_cast<Uint8>(std::clamp(static_cast<float>(palette_color.r) * visible_light, 0.0f, 255.0f));
				texel.g = static_cast<Uint8>(std::clamp(static_cast<float>(palette_color.g) * visible_light, 0.0f, 255.0f));
				texel.b = static_cast<Uint8>(std::clamp(static_cast<float>(palette_color.b) * visible_light, 0.0f, 255.0f));
				texel.a = lighting_pixels[y][x].a;
			}
		}

		return result;
	}

	void ShadowWorkerLoop()
	{
		while (true)
		{
			ShadowJob job;
			{
				std::unique_lock<std::mutex> lock(shadow_queue_mutex);
				shadow_queue_cv.wait(lock, []
					{
						return shadow_worker_stop_requested || pending_shadow_jobs.empty() == false;
					});

				if (shadow_worker_stop_requested && pending_shadow_jobs.empty())
				{
					return;
				}

				job = std::move(pending_shadow_jobs.front());
				pending_shadow_jobs.pop();
				++active_shadow_jobs;
			}

			ShadowResult result = BuildShadowResult(job);

			{
				std::lock_guard<std::mutex> lock(shadow_queue_mutex);
				completed_shadow_jobs.push(std::move(result));
				if (active_shadow_jobs > 0)
				{
					--active_shadow_jobs;
				}
			}
			shadow_completion_cv.notify_all();
		}
	}
}

void ShadowCompiler::Init()
{
	std::lock_guard<std::mutex> lock(shadow_queue_mutex);
	if (shadow_worker_running)
	{
		return;
	}

	shadow_worker_stop_requested = false;
	shadow_worker_thread = std::thread(ShadowWorkerLoop);
	shadow_worker_running = true;
}

void ShadowCompiler::Shutdown()
{
	{
		std::lock_guard<std::mutex> lock(shadow_queue_mutex);
		if (shadow_worker_running == false)
		{
			return;
		}

		shadow_worker_stop_requested = true;
		while (pending_shadow_jobs.empty() == false)
		{
			pending_shadow_jobs.pop();
		}
		while (completed_shadow_jobs.empty() == false)
		{
			completed_shadow_jobs.pop();
		}
		latest_room_requests.clear();
		active_shadow_jobs = 0;
	}

	shadow_queue_cv.notify_all();
	if (shadow_worker_thread.joinable())
	{
		shadow_worker_thread.join();
	}

	std::lock_guard<std::mutex> lock(shadow_queue_mutex);
	shadow_worker_running = false;
	shadow_worker_stop_requested = false;
	++active_compiler_generation;
}

void ShadowCompiler::InvalidatePendingWork()
{
	std::lock_guard<std::mutex> lock(shadow_queue_mutex);
	while (pending_shadow_jobs.empty() == false)
	{
		pending_shadow_jobs.pop();
	}
	while (completed_shadow_jobs.empty() == false)
	{
		completed_shadow_jobs.pop();
	}
	latest_room_requests.clear();
	++active_compiler_generation;
	active_shadow_jobs = 0;
	shadow_completion_cv.notify_all();
}

void ShadowCompiler::PollCompletedShadowMaps()
{
	std::queue<ShadowResult> ready_results;
	{
		std::lock_guard<std::mutex> lock(shadow_queue_mutex);
		std::swap(ready_results, completed_shadow_jobs);
	}

	if (game_renderer == nullptr || current_sector == nullptr)
	{
		return;
	}

	SDL_PixelFormat* pixel_format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
	if (pixel_format == nullptr)
	{
		return;
	}

	while (ready_results.empty() == false)
	{
		ShadowResult result = std::move(ready_results.front());
		ready_results.pop();

		uint64_t latest_request_id = 0;
		{
			std::lock_guard<std::mutex> lock(shadow_queue_mutex);
			if (result.compiler_generation != active_compiler_generation)
			{
				continue;
			}

			if (latest_room_requests.contains(result.room_name))
			{
				latest_request_id = latest_room_requests[result.room_name];
			}
		}

		if (result.request_id != latest_request_id)
		{
			continue;
		}

		Room* room = current_sector->GetRoomFromName(result.room_name);
		if (room == nullptr)
		{
			continue;
		}

		if (result.width <= 0 || result.height <= 0 || result.texels.empty())
		{
			room->shadow_map.reset();
			continue;
		}

		std::vector<Uint32> pixels(static_cast<size_t>(result.width) * static_cast<size_t>(result.height), 0u);
		for (size_t i = 0; i < result.texels.size(); ++i)
		{
			const ShadowTexel& texel = result.texels[i];
			pixels[i] = SDL_MapRGBA(pixel_format, texel.r, texel.g, texel.b, texel.a);
		}

		SDL_Texture* raw_texture = SDL_CreateTexture(
			game_renderer,
			SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_STATIC,
			result.width,
			result.height);

		if (raw_texture == nullptr)
		{
			continue;
		}

		SDL_SetTextureBlendMode(raw_texture, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(raw_texture, SDL_ScaleModeNearest);

		if (SDL_UpdateTexture(raw_texture, nullptr, pixels.data(), result.width * static_cast<int>(sizeof(Uint32))) != 0)
		{
			SDL_DestroyTexture(raw_texture);
			continue;
		}

		room->shadow_map = std::shared_ptr<SDL_Texture>(raw_texture, SDL_DestroyTexture);
	}

	SDL_FreeFormat(pixel_format);
	shadow_completion_cv.notify_all();
}

void ShadowCompiler::WaitForPendingShadowMaps()
{
	while (true)
	{
		PollCompletedShadowMaps();

		std::unique_lock<std::mutex> lock(shadow_queue_mutex);
		if (pending_shadow_jobs.empty() && completed_shadow_jobs.empty() && active_shadow_jobs == 0)
		{
			return;
		}

		shadow_completion_cv.wait(lock, []
			{
				return completed_shadow_jobs.empty() == false ||
					(pending_shadow_jobs.empty() && active_shadow_jobs == 0);
			});
	}
}

bool ShadowCompiler::IsBlockingTile(int tile_id)
{
	if (tile_id == 0)
	{
		return false;
	}
	
	if (TileData::HasProperty(tile_id, "visible") && TileData::GetBoolPropertyValue(tile_id, "visible") == false)
	{
		return false;
	}

	if (TileData::HasProperty(tile_id, "Light") && TileData::GetBoolPropertyValue(tile_id, "Light"))
	{
		return false;
	}

	if (TileData::HasProperty(tile_id, "collide"))
	{
		return TileData::GetBoolPropertyValue(tile_id, "collide");
	}

	return true;
}

bool ShadowCompiler::IsTransparentTile(int tile_id)
{
	if (tile_id == 0)
	{
		return false;
	}

	return TileData::HasProperty(tile_id, "transparent") && TileData::GetBoolPropertyValue(tile_id, "transparent");
}

bool ShadowCompiler::IsTileInBounds(const Room& room, Vec tile_pos)
{
	return tile_pos.x >= 0 && tile_pos.x < room.dim.w && tile_pos.y >= 0 && tile_pos.y < room.dim.h;
}

bool ShadowCompiler::BlocksNaturalLight(const Room& room, Vec tile_pos)
{
	if (IsTileInBounds(room, tile_pos) == false)
	{
		return true;
	}

	const int fg_tile = room.map_data_fg[static_cast<int>(tile_pos.y)][static_cast<int>(tile_pos.x)];
	const int bg_tile = room.map_data_bg[static_cast<int>(tile_pos.y)][static_cast<int>(tile_pos.x)];

	const bool fg_blocks_natural_light = IsBlockingTile(fg_tile) && IsTransparentTile(fg_tile) == false;
	const bool bg_blocks_natural_light = IsBlockingTile(bg_tile) && IsTransparentTile(bg_tile) == false;
	return fg_blocks_natural_light || bg_blocks_natural_light;
}

bool ShadowCompiler::BlocksLight(const Room& room, Vec tile_pos)
{
	if (IsTileInBounds(room, tile_pos) == false)
	{
		return true;
	}

	const int fg_tile = room.map_data_fg[static_cast<int>(tile_pos.y)][static_cast<int>(tile_pos.x)];
	return IsBlockingTile(fg_tile) && IsTransparentTile(fg_tile) == false;
}

void ShadowCompiler::ApplyNaturalLight(const Room& room, std::vector<std::vector<Uint8>>& shadow_alphas)
{
	for (int x = 0; x < room.dim.w; ++x)
	{
		for (int y = 0; y < room.dim.h; ++y)
		{
			Vec tile_pos = Vec(x, y);
			if (BlocksNaturalLight(room, tile_pos))
			{
				break;
			}

			shadow_alphas[y][x] = std::min(shadow_alphas[y][x], NATURAL_LIGHT_ALPHA);
		}
	}

	for (int y = 0; y < room.dim.h; ++y)
	{
		for (int x = 0; x < room.dim.w; ++x)
		{
			const int fg_tile = room.map_data_fg[y][x];
			const int bg_tile = room.map_data_bg[y][x];
			if (IsBlockingTile(fg_tile) == false && IsBlockingTile(bg_tile) == false)
			{
				Vec tile_pos = Vec(x, y);
				ApplyPointLight(room, tile_pos, shadow_alphas);
			}
		}
	}
}

bool ShadowCompiler::HasLineOfSight(const Room& room, Vec start_pos, Vec end_pos)
{
	Vec current_pos = start_pos;
	const int dx = std::abs(static_cast<int>(end_pos.x - start_pos.x));
	const int dy = std::abs(static_cast<int>(end_pos.y - start_pos.y));
	const int step_x = (start_pos.x < end_pos.x) ? 1 : -1;
	const int step_y = (start_pos.y < end_pos.y) ? 1 : -1;
	int error = dx - dy;

	while ((current_pos == end_pos) == false)
	{
		const int doubled_error = error * 2;
		if (doubled_error > -dy)
		{
			error -= dy;
			current_pos.x += step_x;
		}
		if (doubled_error < dx)
		{
			error += dx;
			current_pos.y += step_y;
		}

		if (current_pos == end_pos)
		{
			return true;
		}

		if (BlocksLight(room, current_pos))
		{
			return false;
		}
	}

	return true;
}

void ShadowCompiler::ApplyPointLight(const Room& room, Vec light_pos, std::vector<std::vector<Uint8>>& shadow_alphas, float light_intensity, float color_intensity)
{
	const float light_radius = GetLightRadius(light_intensity);
	if (light_radius <= 0.0f)
	{
		return;
	}

	const float clamped_color_intensity = std::clamp(color_intensity, 0.0f, 1.0f);
	const float point_light_alpha_floor =
		(static_cast<float>(MIN_POINT_LIGHT_ALPHA) * (1.0f - clamped_color_intensity)) +
		(static_cast<float>(MAX_POINT_LIGHT_ALPHA) * clamped_color_intensity);

	const int min_x = std::max(0, static_cast<int>(std::floor(light_pos.x - light_radius)));
	const int max_x = std::min(static_cast<int>(room.dim.w) - 1, static_cast<int>(std::ceil(light_pos.x + light_radius)));
	const int min_y = std::max(0, static_cast<int>(std::floor(light_pos.y - light_radius)));
	const int max_y = std::min(static_cast<int>(room.dim.h) - 1, static_cast<int>(std::ceil(light_pos.y + light_radius)));

	for (int y = min_y; y <= max_y; ++y)
	{
		for (int x = min_x; x <= max_x; ++x)
		{
			Vec tile_pos = Vec(x, y);
			const float distance = Vec::distance(tile_pos, light_pos);
			if (distance > light_radius)
			{
				continue;
			}

			if ((tile_pos == light_pos) == false && HasLineOfSight(room, light_pos, tile_pos) == false)
			{
				continue;
			}

			const float normalized_distance = std::clamp(distance / light_radius, 0.0f, 1.0f);
			const float intensity = std::pow(std::max(0.0f, 1.0f - normalized_distance), LIGHT_FALLOFF_POWER);
			if (intensity < MIN_LIGHT_INTENSITY)
			{
				continue;
			}

			const Uint8 target_alpha = static_cast<Uint8>(std::round(
				(point_light_alpha_floor * intensity) +
				(static_cast<float>(MAX_SHADOW_ALPHA) * (1.0f - intensity))));
			shadow_alphas[y][x] = std::min(shadow_alphas[y][x], target_alpha);
		}
	}
}

void ShadowCompiler::ApplyLightBleed(const Room& room, std::vector<std::vector<Uint8>>& shadow_alphas)
{
	for (int pass = 0; pass < LIGHT_BLEED_PASSES; ++pass)
	{
		std::vector<std::vector<Uint8>> next_shadow_alphas = shadow_alphas;

		for (int y = 0; y < room.dim.h; ++y)
		{
			for (int x = 0; x < room.dim.w; ++x)
			{
				Vec tile_pos = Vec(x, y);
				if (BlocksLight(room, tile_pos))
				{
					continue;
				}

				const Vec neighbor_offsets[4] =
				{
					Vec(1, 0),
					Vec(-1, 0),
					Vec(0, 1),
					Vec(0, -1)
				};

				for (Vec offset : neighbor_offsets)
				{
					Vec neighbor_pos = tile_pos + offset;
					if (IsTileInBounds(room, neighbor_pos) == false)
					{
						continue;
					}

					if (BlocksLight(room, neighbor_pos))
					{
						continue;
					}

					const int neighbor_x = static_cast<int>(neighbor_pos.x);
					const int neighbor_y = static_cast<int>(neighbor_pos.y);
					const int bled_alpha = std::min(
						static_cast<int>(MAX_SHADOW_ALPHA),
						static_cast<int>(shadow_alphas[neighbor_y][neighbor_x]) + static_cast<int>(LIGHT_BLEED_STEP));

					next_shadow_alphas[y][x] = std::min(next_shadow_alphas[y][x], static_cast<Uint8>(bled_alpha));
				}
			}
		}

		shadow_alphas = next_shadow_alphas;
	}
}

bool ShadowCompiler::IsLightSource(int tile_id)
{
	return (TileData::HasProperty(tile_id, "Light") && TileData::GetBoolPropertyValue(tile_id, "Light")) ||
		(TileData::HasProperty(tile_id, "light") && TileData::GetBoolPropertyValue(tile_id, "light"));
}

void ShadowCompiler::GenerateShadowMap(Room& room)
{
	room.shadow_map.reset();

	if (shadow_worker_running == false)
	{
		Init();
	}

	ShadowJob job;
	job.room_snapshot.pos = room.pos;
	job.room_snapshot.dim = room.dim;
	job.room_snapshot.name = room.name;
	job.room_snapshot.map_data_fg = room.map_data_fg;
	job.room_snapshot.map_data_bg = room.map_data_bg;
	job.room_snapshot.filter_color = room.filter_color;

	{
		std::lock_guard<std::mutex> lock(shadow_queue_mutex);
		job.request_id = ++latest_room_requests[room.name];
		job.compiler_generation = active_compiler_generation;
		pending_shadow_jobs.push(std::move(job));
	}

	shadow_queue_cv.notify_one();
}
