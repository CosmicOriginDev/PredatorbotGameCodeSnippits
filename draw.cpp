#include "draw.h"
#include <array>
#include "constants.h"
#include <iostream>
#include "MapManager/sector.h"
#include "MapManager/emissionGenerator.h"
#include "MapManager/tileData.h"
#include "Datatypes/color.h"
#include "camera.h"
#include "tileManager.h"
#include <algorithm>
extern SDL_Renderer* game_renderer;
extern Room* current_room;
SDL_Texture* DrawSystem::layer_textures[LAYER_COUNT];
SDL_Color DrawSystem::current_filter_color;
SDL_Texture* DrawSystem::sky_texture;
SDL_Rect DrawSystem::world_render_rect;
Vec DrawSystem::game_offset;
void DrawSystem::Init()
{
	for (int i = 0; i < std::size(DrawSystem::layer_textures); i++)
	{
		SDL_Texture* ret = SDL_CreateTexture(game_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, RENDER_WIDTH, RENDER_HEIGHT);
		if (ret == nullptr)
		{
			std::cout << "ERROR WITH MAKING LAYER: " << SDL_GetError() << std::endl;
			exit(EXIT_FAILURE);
		}
		layer_textures[i] = ret;
		
	}
}
void DrawSystem::CleanUp()
{
	for (int i = 0; i < std::size(DrawSystem::layer_textures); i++)
	{
		if (layer_textures[i] != nullptr)
		{
			SDL_DestroyTexture(layer_textures[i]);
			layer_textures[i] = nullptr;
		}
	}
}
void DrawSystem::DrawScreen()
{
	
	SDL_SetRenderTarget(game_renderer, NULL);
	for (int i = 0; i < std::size(DrawSystem::layer_textures); i++)
	{
		SDL_Rect r = SDL_Rect(0, 0, RENDER_WIDTH, RENDER_HEIGHT); // used to cut off data out of bounds and keep things secure
		SDL_SetTextureBlendMode(layer_textures[i], i == EMIT ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
		if (i != UI && i != DEBUG)
		{
			SDL_Rect destRect = world_render_rect;
			SDL_SetTextureColorMod(layer_textures[i], 255, 255, 255);
			Vec screenRoomPos = Vec::worldSpaceToScreenSpace(current_room->pos*TILE_SIZE);
			SDL_Rect clip = SDL_Rect(screenRoomPos.x, screenRoomPos.y, current_room->dim.w*TILE_SIZE, current_room->dim.h*TILE_SIZE);
			SDL_RenderSetClipRect(game_renderer, &clip);
			int ret = SDL_RenderCopy(game_renderer, layer_textures[i], &r, &destRect);
			if (ret < 0)
			{
				std::cout << "ERROR WITH DRAWING LAYER: " << SDL_GetError() << std::endl;
				exit(EXIT_FAILURE);
			}
		}
		else
		{
			SDL_Rect destRect = SDL_Rect(game_offset.x, game_offset.y, RENDER_WIDTH, RENDER_HEIGHT);
			SDL_SetTextureColorMod(layer_textures[i], 255,255,255);
			SDL_RenderSetClipRect(game_renderer, NULL);
			int ret = SDL_RenderCopy(game_renderer, layer_textures[i], &r, &destRect);
			if (ret < 0)
			{
				std::cout << "ERROR WITH DRAWING LAYER: " << SDL_GetError() << std::endl;
				exit(EXIT_FAILURE);
			}
		}

		
		
	}
	SDL_RenderPresent(game_renderer);
	// clear all continous running checks here
	world_render_rect = SDL_Rect(game_offset.x, game_offset.y, RENDER_WIDTH, RENDER_HEIGHT);
}
void DrawSystem::ClearScreen()
{
	for (int i = 0; i < std::size(DrawSystem::layer_textures); i++)
	{
		SDL_SetRenderTarget(game_renderer, layer_textures[i]);
		SDL_SetRenderDrawColor(game_renderer, 0, 0, 0, 0);
		SDL_RenderClear(game_renderer);
	}
	SDL_SetRenderTarget(game_renderer, NULL);
	SDL_RenderClear(game_renderer);
}
void DrawSystem::SetFilter(SDL_Color colorParam)
{
	current_filter_color = colorParam;
}
void DrawSystem::Stretch(Vec offsetParam)
{
	world_render_rect.x -= offsetParam.x;
	world_render_rect.w += offsetParam.x;
	world_render_rect.y -= offsetParam.y;
	world_render_rect.h += offsetParam.y;
	
}
void DrawSystem::DrawSky()
{
	int skyX;
	skyX = 0-(int)(Camera::GetCameraPos().x / 4) % RENDER_WIDTH;
	SDL_Rect skyRect1 = SDL_Rect(skyX, 0, RENDER_WIDTH, RENDER_HEIGHT);
	SDL_Rect skyRect2 = SDL_Rect(skyX + RENDER_WIDTH, 0, RENDER_WIDTH, RENDER_HEIGHT);
	SDL_SetRenderTarget(game_renderer, layer_textures[SKY]);
	SDL_RenderCopy(game_renderer, sky_texture, NULL, &skyRect1);
	SDL_RenderCopy(game_renderer, sky_texture, NULL, &skyRect2);
}
SDL_Texture* DrawSystem::DrawShadowMap()
{
	if (current_room == nullptr || current_room->shadow_map == nullptr)
	{
		return nullptr;
	}

	SDL_Texture* shadow_texture = current_room->shadow_map.get();
	SDL_SetTextureBlendMode(shadow_texture, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(shadow_texture, SDL_ScaleModeLinear);
	SDL_SetRenderTarget(game_renderer, layer_textures[SHADOW]);
	SDL_SetTextureBlendMode(layer_textures[SHADOW], SDL_BLENDMODE_BLEND);
	Vec room_screen_pos = Vec::worldSpaceToScreenSpace(current_room->pos * TILE_SIZE);
	SDL_FRect destination_rect = SDL_FRect(
		room_screen_pos.x,
		room_screen_pos.y,
		current_room->dim.w * TILE_SIZE,
		current_room->dim.h * TILE_SIZE);

	SDL_RenderCopyF(game_renderer, shadow_texture, nullptr, &destination_rect);

	return layer_textures[SHADOW];
}

SDL_Texture* DrawSystem::DrawEmissionMap()
{
	if (current_room == nullptr)
	{
		return nullptr;
	}

	Vec camera_pos = Camera::GetCameraPos();
	const int start_x = static_cast<int>(camera_pos.x / TILE_SIZE);
	const int start_y = static_cast<int>(camera_pos.y / TILE_SIZE);
	const int visible_width = (RENDER_WIDTH / TILE_SIZE);
	const int visible_height = (RENDER_HEIGHT / TILE_SIZE);
	const int offset_x = 0 - static_cast<int>(camera_pos.x);
	const int offset_y = 0 - static_cast<int>(camera_pos.y);

	SDL_SetRenderTarget(game_renderer, layer_textures[EMIT]);
	SDL_SetTextureBlendMode(layer_textures[EMIT], SDL_BLENDMODE_ADD);

	SDL_Rect destination_rect = SDL_Rect(0, 0, TILE_SIZE, TILE_SIZE);
	auto draw_emission_tile = [&](int tile_id, int world_x, int world_y)
	{
		if (tile_id == 0)
		{
			return;
		}

		if (TileData::HasProperty(tile_id, "visible") && TileData::GetBoolPropertyValue(tile_id, "visible") == false)
		{
			return;
		}

		SDL_Texture* emission_texture = EmissionGenerator::GetEmissionTexture(tile_id);
		if (emission_texture == nullptr)
		{
			return;
		}

		destination_rect.x = (world_x * TILE_SIZE) + offset_x;
		destination_rect.y = (world_y * TILE_SIZE) + offset_y;
		SDL_RenderCopy(game_renderer, emission_texture, nullptr, &destination_rect);
	};

	for (int world_x = start_x - 1; world_x <= start_x + visible_width + 1; world_x++)
	{
		for (int world_y = start_y - 1; world_y <= start_y + visible_height + 1; world_y++)
		{
			draw_emission_tile(TileManager::GetRoomTileBG(world_x, world_y), world_x, world_y);
		}
	}

	for (int world_x = start_x - 1; world_x <= start_x + visible_width + 1; world_x++)
	{
		for (int world_y = start_y - 1; world_y <= start_y + visible_height + 1; world_y++)
		{
			draw_emission_tile(TileManager::GetRoomTileFG(world_x, world_y), world_x, world_y);
		}
	}

	return layer_textures[EMIT];
}
