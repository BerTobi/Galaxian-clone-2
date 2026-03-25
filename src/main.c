#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>

#define MAX_ENTITIES 1000
#define NONE -1

typedef struct Assets
{
	Sound playerShoot;
	Sound fighterLoss;
	Sound hitEnemy;
	Sound battleTheme;
	Texture2D spriteSheet;
} Assets;

typedef struct Configuration
{
	int screenWidth;
	int screenHeight;
} Configuration;

typedef enum projectileType
{
	PLAYER_BULLET,
	ENEMY_BULLET
} ProjectileType;

typedef enum enemyType
{
	BLUE_ENEMY,
	PURPLE_ENEMY
} enemyType;

typedef enum enemyBehaviour
{
	IN_FORMATION,
	DIVING
} enemyBehaviour;

typedef struct EnemyData
{
	enemyType type;
	enemyBehaviour behaviour;
} EnemyData;

typedef union EntityData
{
	struct
	{
		int lives;
	} player;
	EnemyData enemy;
	struct
	{
		ProjectileType type;
	} projectile;
} EntityData;

typedef enum EntityType
{
	PLAYER,
	ENEMY,
	PROJECTILE
} EntityType;

typedef enum EntityState
{
	ALIVE,
	DYING
} EntityState;

typedef struct Entity
{
	EntityType type;
	EntityState state;
	Vector2 position;
	Vector2 velocity;
	int size;
	EntityData data;
} Entity;

typedef struct GameData
{
	Entity entities[MAX_ENTITIES];
	int entityCount;
} GameData;

void initializeGame(GameData* gameData)
{
	gameData->entityCount = 0;
	for (int i = 0; i < MAX_ENTITIES; i++)
	{
		gameData->entities[i] = (Entity){ .type = NONE, .position = {0, 0}, .velocity = {0, 0}, .size = 0 };
	}

	gameData->entities[0] = (Entity){ .type = PLAYER, .state = ALIVE, .position = (Vector2){112, 240}, .velocity = (Vector2){0, 0}, .size = 16, .data.player.lives = 3};
	gameData->entityCount++;
	
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			gameData->entities[gameData->entityCount] = (Entity){ .type = ENEMY, .state = ALIVE, .position = (Vector2){32 + j * 16, 40 + i * 16}, .velocity = (Vector2){0.1f, 0}, .size = 16, .data.enemy.type = BLUE_ENEMY };
			gameData->entities[gameData->entityCount].data.enemy = (EnemyData) {.type = BLUE_ENEMY, .behaviour = IN_FORMATION };
			gameData->entityCount++;
		}
	}

	for (int j = 0; j < 8; j++)
	{
		gameData->entities[gameData->entityCount] = (Entity){ .type = ENEMY, .state = ALIVE, .position = (Vector2){48 + j * 16, 24}, .velocity = (Vector2){0.1f, 0}, .size = 16, .data.enemy.type = BLUE_ENEMY };
		gameData->entities[gameData->entityCount].data.enemy = (EnemyData){ .type = PURPLE_ENEMY, .behaviour = IN_FORMATION };
		gameData->entityCount++;
	}
}

void Update(GameData* gameData, int currentTick, Assets* assets)
{
	if (IsSoundPlaying(assets->battleTheme) == false)
	{
		PlaySound(assets->battleTheme);
		SetSoundVolume(assets->battleTheme, 0.5f);
	}
	for (int i = 0; i < gameData->entityCount; i++)
	{
		gameData->entities[i].position.x += gameData->entities[i].velocity.x;
		gameData->entities[i].position.y += gameData->entities[i].velocity.y;

		if (gameData->entities[i].type == ENEMY)
		{
			if (currentTick % 400 == 0)
			{
				gameData->entities[i].velocity.x *= -1.0f;
			}
			
		}

		if (gameData->entities[i].type == PROJECTILE && gameData->entities[i].data.projectile.type == PLAYER_BULLET )
		{
			for (int j = 0; j < gameData->entityCount; j++)
			{
				if (gameData->entities[j].type == ENEMY && gameData->entities[j].state == ALIVE)
				{
					if (CheckCollisionCircles(gameData->entities[i].position, gameData->entities[i].size / 2.0f, gameData->entities[j].position, gameData->entities[j].size / 2.0f))
					{
						if (i != gameData->entityCount - 1)
						{
							gameData->entities[i] = gameData->entities[gameData->entityCount - 1];
						}
						if (j == gameData->entityCount - 1) j = i;
						gameData->entityCount--;
						if (j != gameData->entityCount - 1)
							gameData->entities[j] = gameData->entities[gameData->entityCount - 1];
						gameData->entityCount--;
						PlaySound(assets->hitEnemy);
						break;
					}
				}
			}
		}
	}
}

void Draw(RenderTexture2D target, Configuration* config, GameData* gameData, Assets* assets)
{
	BeginTextureMode(target);
	ClearBackground(BLACK);
	for (int i = 0; i < gameData->entityCount; i++)
	{
		switch (gameData->entities[i].type)
		{
			case PLAYER:
				DrawRectangle(gameData->entities[i].position.x, gameData->entities[i].position.y, gameData->entities[i].size - 1, gameData->entities[i].size - 1, RED);
				break;
			case ENEMY:
				if (gameData->entities[i].data.enemy.type == BLUE_ENEMY)
					DrawCircle(gameData->entities[i].position.x, gameData->entities[i].position.y, gameData->entities[i].size / 2, BLUE);
				else
					DrawCircle(gameData->entities[i].position.x, gameData->entities[i].position.y, gameData->entities[i].size / 2, PURPLE);
				break;
			case PROJECTILE:
				DrawRectangle(gameData->entities[i].position.x, gameData->entities[i].position.y, gameData->entities[i].size - 1, gameData->entities[i].size, YELLOW);
				break;
		}
	}
	EndTextureMode();

	BeginDrawing();
	ClearBackground(BLACK);
	DrawTexturePro(target.texture, 
		(Rectangle) {0, 0, 224.0f, -256.0f}, 
		(Rectangle) {0, 0, (float)config->screenWidth, (float)config->screenHeight},
		(Vector2) {0, 0}, 
		0.0f,
		WHITE
	);
	EndDrawing();
}

void shootProjectile(GameData* gameData, Entity* shooter)
{
	ProjectileType type = NONE;
	Vector2 velocity = { 0, 0 };
	if (shooter->type == PLAYER)
	{
		type = PLAYER_BULLET;
		velocity = (Vector2){ 0, -2.0f };
	}
	else if (shooter->type == ENEMY)
	{
		type = ENEMY_BULLET;
		velocity = (Vector2){ 0, 2.0f };
	}
	else
		return;
	
	gameData->entities[gameData->entityCount] = (Entity){ .type = PROJECTILE, .state = ALIVE, .position = (Vector2){shooter->position.x + shooter->size / 2 - 2, shooter->position.y - 8}, .velocity = velocity, .size = 2, .data.projectile.type = type };
	gameData->entityCount++;
}

void HandleInput(GameData* gameData, Assets* assets)
{
	if (IsKeyDown(KEY_LEFT)) gameData->entities[0].velocity = (Vector2){-1.0f, 0};
	else if (IsKeyDown(KEY_RIGHT)) gameData->entities[0].velocity = (Vector2){ 1.0f, 0 };
	else gameData->entities[0].velocity = (Vector2){ 0, 0 };
	if (IsKeyPressed(KEY_LEFT_CONTROL))
	{
		shootProjectile(gameData, &gameData->entities[0]);
		PlaySound(assets->playerShoot);
	}
}

int main(void)
{
	int currentTick = 200;
	double lastTick = GetTime();
	Configuration config = { 896, 1024 };
	InitWindow(config.screenWidth, config.screenHeight, "Galaxian Clone");
	InitAudioDevice();
	SetTargetFPS(60);               

	RenderTexture2D target = LoadRenderTexture(224, 256);
	SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

	GameData* gameData = malloc(sizeof(GameData));
	Assets* assets = malloc(sizeof(Assets));

	assets->playerShoot = LoadSound("res/Shoot.mp3");
	assets->fighterLoss = LoadSound("res/Fighter Loss.mp3");
	assets->hitEnemy = LoadSound("res/Hit Enemy.mp3");
	assets->battleTheme = LoadSound("res/Battle Theme.mp3");
	assets->spriteSheet = LoadTexture("res/Spritesheet.png");

	initializeGame(gameData);

	while (!WindowShouldClose())    
	{
		HandleInput(gameData, assets);
		if (GetTime() - lastTick >= 0.02)
		{
			currentTick++;
			lastTick = GetTime();
			Update(gameData, currentTick, assets);
		}
		Draw(target, &config, gameData, assets);
	}
	CloseWindow();
}