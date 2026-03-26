#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <math.h>

#define MAX_ENTITIES 1000
#define NONE -1

#define BASE_PLAYER_SPRITE (Rectangle){1, 70, 16, 16}
#define BASE_BULLET_SPRITE (Rectangle){200, 97, 1, 2}
#define BASE_BLUE_ENEMY_SPRITE (Rectangle){1, 34, 16, 16}
#define BASE_PURPLE_ENEMY_SPRITE (Rectangle){1, 17, 16, 16}

typedef struct Animation
{
	Rectangle* frames;
	int frameCount;
	int currentFrame;
	float ticksPerFrame;
	int lastFrameTick;
} Animation;

typedef struct Assets
{
	Sound playerShoot;
	Sound fighterLoss;
	Sound hitEnemy;
	Sound battleTheme;
	Texture2D spriteSheet;
	Animation blueEnemyMovement;
	Animation purpleEnemyMovement;
	Animation enemyDeath;
	Animation playerDeath;
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
	int diveStartTick;
	Animation deathAnimation;
	Animation movementAnimation;
} EnemyData;

typedef union EntityData
{
	struct
	{
		int lives;
		Animation deathAnimation;
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
	Rectangle baseSprite;
} Entity;

typedef struct GameData
{
	Entity entities[MAX_ENTITIES];
	Configuration config;
	int entityCount;
	int score;
	int maxPlayerProjectiles;
	int currentPlayerProjectiles;
	int maxDivingEnemies;
	int currentDivingEnemies;
	int powerupTicks;
} GameData;

void loadAnimations(Assets* assets)
{
	// Blue enemy movement
	assets->blueEnemyMovement.frames = malloc(sizeof(Rectangle) * 3);
	assets->blueEnemyMovement.frames[0] = (Rectangle){ 1, 34, 16, 16 };
	assets->blueEnemyMovement.frames[1] = (Rectangle){ 18, 34, 16, 16 };
	assets->blueEnemyMovement.frames[2] = (Rectangle){ 35, 34, 16, 16 };
	assets->blueEnemyMovement = (Animation){ .frames = assets->blueEnemyMovement.frames, .frameCount = 3, .currentFrame = 0, .ticksPerFrame = 50, .lastFrameTick = 0 };

	// Purple enemy movement
	assets->purpleEnemyMovement.frames = malloc(sizeof(Rectangle) * 3);
	assets->purpleEnemyMovement.frames[0] = (Rectangle){ 1, 17, 16, 16 };
	assets->purpleEnemyMovement.frames[1] = (Rectangle){ 18, 17, 16, 16 };
	assets->purpleEnemyMovement.frames[2] = (Rectangle){ 35, 17, 16, 16 };
	assets->purpleEnemyMovement = (Animation){ .frames = assets->purpleEnemyMovement.frames, .frameCount = 3, .currentFrame = 0, .ticksPerFrame = 50, .lastFrameTick = 0 };


	// Enemy death
	assets->enemyDeath.frames = malloc(sizeof(Rectangle) * 4);
	assets->enemyDeath.frames[0] = (Rectangle){ 61, 70, 16, 16 };
	assets->enemyDeath.frames[1] = (Rectangle){ 78, 70, 16, 16 };
	assets->enemyDeath.frames[2] = (Rectangle){ 95, 70, 16, 16 };
	assets->enemyDeath.frames[3] = (Rectangle){ 112, 70, 16, 16 };
	assets->enemyDeath = (Animation){ .frames = assets->enemyDeath.frames, .frameCount = 4, .currentFrame = 0, .ticksPerFrame = 15, .lastFrameTick = 0 };

	// Player death
	assets->playerDeath.frames = malloc(sizeof(Rectangle) * 4);
	assets->playerDeath.frames[0] = (Rectangle){ 1, 87, 32, 32 };
	assets->playerDeath.frames[1] = (Rectangle){ 34, 87, 32, 32 };
	assets->playerDeath.frames[2] = (Rectangle){ 67, 87, 32, 32 };
	assets->playerDeath.frames[3] = (Rectangle){ 100, 87, 32, 32 };
	assets->playerDeath = (Animation){ .frames = assets->playerDeath.frames, .frameCount = 4, .currentFrame = 0, .ticksPerFrame = 15, .lastFrameTick = 0 };

}

void initializeGame(GameData* gameData, Assets* assets)
{
	gameData->entityCount = 0;
	gameData->score = 0;
	gameData->maxPlayerProjectiles = 1;
	gameData->currentPlayerProjectiles = 0;
	gameData->currentDivingEnemies = 0;
	gameData->maxDivingEnemies = 5;
	gameData->powerupTicks = 0;
	for (int i = 0; i < MAX_ENTITIES; i++)
	{
		gameData->entities[i] = (Entity){ .type = NONE, .position = {0, 0}, .velocity = {0, 0}, .size = 0 };
	}

	gameData->entities[0] = (Entity){ .type = PLAYER, .state = ALIVE, .position = (Vector2){112, 240}, .velocity = (Vector2){0, 0}, .size = 16, .baseSprite = BASE_PLAYER_SPRITE, .data.player.lives = 3 };
	gameData->entities[gameData->entityCount].data.player.deathAnimation = assets->playerDeath;
	gameData->entityCount++;
	
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			gameData->entities[gameData->entityCount] = (Entity){ .type = ENEMY, .state = ALIVE, .position = (Vector2){32 + j * 16, 40 + i * 16}, .velocity = (Vector2){0.1f, 0}, .size = 16, .baseSprite = BASE_BLUE_ENEMY_SPRITE, .data.enemy.type = BLUE_ENEMY };
			gameData->entities[gameData->entityCount].data.enemy = (EnemyData) {.type = BLUE_ENEMY, .behaviour = IN_FORMATION, .movementAnimation = assets->blueEnemyMovement, .deathAnimation = assets->enemyDeath };
			gameData->entityCount++;
		}
	}

	for (int j = 0; j < 8; j++)
	{
		gameData->entities[gameData->entityCount] = (Entity){ .type = ENEMY, .state = ALIVE, .position = (Vector2){48 + j * 16, 24}, .velocity = (Vector2){0.1f, 0}, .size = 16, .baseSprite = BASE_PURPLE_ENEMY_SPRITE, .data.enemy.type = BLUE_ENEMY };
		gameData->entities[gameData->entityCount].data.enemy = (EnemyData){ .type = PURPLE_ENEMY, .behaviour = IN_FORMATION, .movementAnimation = assets->purpleEnemyMovement, .deathAnimation = assets->enemyDeath };
		gameData->entityCount++;
	}
}

void ShootProjectile(GameData* gameData, Entity* shooter, Assets* assets)
{
	ProjectileType type = NONE;
	Vector2 velocity = { 0, 0 };
	if (shooter->type == PLAYER && gameData->currentPlayerProjectiles < gameData->maxPlayerProjectiles)
	{
		type = PLAYER_BULLET;
		velocity = (Vector2){ 0, -2.0f };
		gameData->currentPlayerProjectiles++;
		PlaySound(assets->playerShoot);
	}
	else if (shooter->type == ENEMY)
	{
		type = ENEMY_BULLET;
		velocity = (Vector2){ 0, 2.0f };
	}
	else
		return;

	gameData->entities[gameData->entityCount] = (Entity){ .type = PROJECTILE, .state = ALIVE, .position = (Vector2){shooter->position.x, shooter->position.y}, .velocity = velocity, .size = 2, .baseSprite = BASE_BULLET_SPRITE, .data.projectile.type = type };
	gameData->entityCount++;
}

void KillEntity(GameData* gameData, int index)
{
	if (index != gameData->entityCount - 1)
	{
		gameData->entities[index] = gameData->entities[gameData->entityCount - 1];
		gameData->entityCount--;
	}
	else gameData->entityCount--;
}

void gameOverScreen(GameData* gameData)
{
	BeginDrawing();
	DrawText("GAME OVER", gameData->config.screenWidth / 2 - MeasureText("GAME OVER", 60) / 2, gameData->config.screenHeight / 2 - 30, 60, WHITE);
	EndDrawing();
	WaitTime(5.0);
	return;
}

void Update(GameData* gameData, int currentTick, Assets* assets)
{
	if (IsSoundPlaying(assets->battleTheme) == false)
	{
		PlaySound(assets->battleTheme);
		SetSoundVolume(assets->battleTheme, 0.5f);
	}
	if (gameData->powerupTicks > 0) 	
	{
		gameData->maxPlayerProjectiles = 2;
		gameData->powerupTicks--;
	}
	else
	{
		gameData->maxPlayerProjectiles = 1;
	}
	for (int i = 0; i < gameData->entityCount; i++)
	{
		gameData->entities[i].position.x += gameData->entities[i].velocity.x;
		gameData->entities[i].position.y += gameData->entities[i].velocity.y;

		if (gameData->entities[i].type == ENEMY)
		{
			if (gameData->entities[i].state == ALIVE)
			{
				if (gameData->entities[i].data.enemy.behaviour == IN_FORMATION)
				{
					if (currentTick % 400 == 0)
					{
						gameData->entities[i].velocity.x *= -1.0f;
					}

					if (gameData->currentDivingEnemies < gameData->maxDivingEnemies)
					{
						int divingChance = rand() % 10000;

						if (divingChance < 2)
						{
							gameData->entities[i].data.enemy.behaviour = DIVING;
							gameData->entities[i].data.enemy.diveStartTick = currentTick;
							gameData->currentDivingEnemies++;
						}
					}
				}

				if (gameData->entities[i].data.enemy.behaviour == DIVING)
				{
					int shootChance = rand() % 10000;
					float t = (currentTick - gameData->entities[i].data.enemy.diveStartTick) / 50.0f;
					if (gameData->entities[i].data.enemy.type == BLUE_ENEMY)
					{
						gameData->entities[i].velocity = (Vector2){ cosf(t) * 1.5f, sinf(t * 0.5f) * 0.5f };
						if (shootChance < 20)
						{
							ShootProjectile(gameData, &gameData->entities[i], assets);
						}
					}
					else if (gameData->entities[i].data.enemy.type == PURPLE_ENEMY)
					{
						gameData->entities[i].velocity = (Vector2){ cosf(t) * 3.5f, sinf(t * 0.7f) * 0.7f };
						if (shootChance < 30)
						{
							ShootProjectile(gameData, &gameData->entities[i], assets);
						}
					}
				}

				if (currentTick - gameData->entities[i].data.enemy.movementAnimation.lastFrameTick >= gameData->entities[i].data.enemy.movementAnimation.ticksPerFrame)
				{
					gameData->entities[i].data.enemy.movementAnimation.currentFrame = (gameData->entities[i].data.enemy.movementAnimation.currentFrame + 1) % gameData->entities[i].data.enemy.movementAnimation.frameCount;
					gameData->entities[i].data.enemy.movementAnimation.lastFrameTick = currentTick;
				}
			}

			else if (gameData->entities[i].state == DYING)
			{
				if (currentTick - gameData->entities[i].data.enemy.deathAnimation.lastFrameTick >= gameData->entities[i].data.enemy.deathAnimation.ticksPerFrame)
				{
					gameData->entities[i].data.enemy.deathAnimation.currentFrame = (gameData->entities[i].data.enemy.deathAnimation.currentFrame + 1);
					gameData->entities[i].data.enemy.deathAnimation.lastFrameTick = currentTick;
				}
				if (gameData->entities[i].data.enemy.deathAnimation.currentFrame >= gameData->entities[i].data.enemy.deathAnimation.frameCount)
				{
					KillEntity(gameData, i);
				}
			}
			
			
		}

		if (gameData->entities[i].type == PROJECTILE && gameData->entities[i].data.projectile.type == PLAYER_BULLET )
		{
			for (int j = 0; j < gameData->entityCount; j++)
			{
				if (gameData->entities[j].type == ENEMY && gameData->entities[j].state == ALIVE)
				{
					if (CheckCollisionCircles(gameData->entities[i].position, gameData->entities[i].size / 2.5f, gameData->entities[j].position, gameData->entities[j].size / 2.5f))
					{
						if (gameData->entities[j].data.enemy.behaviour == DIVING)
						{
							gameData->currentDivingEnemies--;
						}
						if (gameData->entities[j].data.enemy.type == PURPLE_ENEMY)
						{
							gameData->score += 40;
							gameData->powerupTicks += 500;
						}
						else
						{
							gameData->score += 20;
						}
						if (j == gameData->entityCount - 1) j = i;
						KillEntity(gameData, i);
						gameData->entities[j].state = DYING;
						PlaySound(assets->hitEnemy);
						gameData->currentPlayerProjectiles--;
						
						break;
					}
				}
			}

			
		}

		if (gameData->entities[i].type == PROJECTILE)
		{
			if (gameData->entities[i].position.y < 0 || gameData->entities[i].position.y > 256)
			{
				if (gameData->entities[i].data.projectile.type == PLAYER_BULLET)
				{
					gameData->currentPlayerProjectiles--;
				}
				KillEntity(gameData, i);
			}
		}

		if (gameData->entities[i].type == PLAYER)
		{
			if (gameData->entities[i].state == ALIVE)
			{
				for (int j = 0; j < gameData->entityCount; j++)
				{
					if ((gameData->entities[j].type == PROJECTILE && gameData->entities[j].data.projectile.type == ENEMY_BULLET) || (gameData->entities[j].type == ENEMY && gameData->entities[j].state == ALIVE))
					{
						if (CheckCollisionCircles(gameData->entities[i].position, gameData->entities[i].size / 2.5f, gameData->entities[j].position, gameData->entities[j].size / 2.5f))
						{
							PlaySound(assets->fighterLoss);
							gameData->entities[i].state = DYING;

						}
					}
				}
			}

			else if (gameData->entities[i].state == DYING)
			{
				if (currentTick - gameData->entities[i].data.player.deathAnimation.lastFrameTick >= gameData->entities[i].data.player.deathAnimation.ticksPerFrame)
				{
					gameData->entities[i].data.player.deathAnimation.currentFrame = (gameData->entities[i].data.player.deathAnimation.currentFrame + 1);
					gameData->entities[i].data.player.deathAnimation.lastFrameTick = currentTick;
				}
				if (gameData->entities[i].data.player.deathAnimation.currentFrame >= gameData->entities[i].data.player.deathAnimation.frameCount)
				{
					gameOverScreen(gameData);
					initializeGame(gameData, assets);
				}
			}
		}
	}
}

void Draw(RenderTexture2D target, GameData* gameData, Assets* assets)
{
	BeginTextureMode(target);
	ClearBackground(BLACK);
	for (int i = 0; i < gameData->entityCount; i++)
	{
		Rectangle spritePosition = (Rectangle){ (int)(gameData->entities[i].position.x - (gameData->entities[i].size / 2.0f)), (int)(gameData->entities[i].position.y - (gameData->entities[i].size / 2.0f)), gameData->entities[i].size, gameData->entities[i].size };
		switch (gameData->entities[i].type)
		{
			case PLAYER:
				if (gameData->entities[i].state == ALIVE)
				{
					DrawTexturePro(assets->spriteSheet, gameData->entities[i].baseSprite, spritePosition, (Vector2) { 0, 0 }, 0.0f, WHITE);
				}
				else if (gameData->entities[i].state == DYING)
				{
					DrawTexturePro(assets->spriteSheet, gameData->entities[i].data.player.deathAnimation.frames[gameData->entities[i].data.player.deathAnimation.currentFrame], spritePosition, (Vector2) { 0, 0 }, 0.0f, WHITE);
				}		
				break;
			case ENEMY:
				if (gameData->entities[i].state == ALIVE)
				{
					DrawTexturePro(assets->spriteSheet, gameData->entities[i].data.enemy.movementAnimation.frames[gameData->entities[i].data.enemy.movementAnimation.currentFrame], spritePosition, (Vector2) { 0, 0 }, 0.0f, WHITE);					
				}
				
				else if (gameData->entities[i].state == DYING)
				{
					DrawTexturePro(assets->spriteSheet, gameData->entities[i].data.enemy.deathAnimation.frames[gameData->entities[i].data.enemy.deathAnimation.currentFrame], spritePosition, (Vector2) { 0, 0 }, 0.0f, WHITE);
				}
				break;
			default:
				DrawTexturePro(assets->spriteSheet, gameData->entities[i].baseSprite, spritePosition, (Vector2) { 0, 0 }, 0.0f, WHITE);
				break;
		}
	}
	char scoreText[20];
	sprintf(scoreText, "SCORE: %i", gameData->score);
	DrawText(scoreText, 8, 5, 8, WHITE);
	EndTextureMode();

	BeginDrawing();
	ClearBackground(BLACK);
	DrawTexturePro(target.texture, 
		(Rectangle) {0, 0, 224.0f, -256.0f}, 
		(Rectangle) {0, 0, (float)gameData->config.screenWidth, (float)gameData->config.screenHeight},
		(Vector2) {0, 0}, 
		0.0f,
		WHITE
	);
	EndDrawing();
}

void HandleInput(GameData* gameData, Assets* assets)
{
	if (IsKeyDown(KEY_LEFT)) gameData->entities[0].velocity = (Vector2){-1.0f, 0};
	else if (IsKeyDown(KEY_RIGHT)) gameData->entities[0].velocity = (Vector2){ 1.0f, 0 };
	else gameData->entities[0].velocity = (Vector2){ 0, 0 };
	if (IsKeyPressed(KEY_LEFT_CONTROL)) ShootProjectile(gameData, &gameData->entities[0], assets);

}

void CleanUp(GameData* gameData, Assets* assets)
{
	free(gameData);
	free(assets);
}

int main(void)
{
	srand(time(NULL));
	int currentTick = 200;
	double lastTick = GetTime();
	GameData* gameData = malloc(sizeof(GameData));
	gameData->config = (Configuration){ .screenWidth = 896, .screenHeight = 1024 };
	InitWindow(gameData->config.screenWidth, gameData->config.screenHeight, "Galaxian Clone");
	InitAudioDevice();
	SetTargetFPS(60);               

	RenderTexture2D target = LoadRenderTexture(224, 256);
	SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

	Assets* assets = malloc(sizeof(Assets));
	assert(gameData != NULL && assets != NULL);

	assets->playerShoot = LoadSound("res/Shoot.mp3");
	assets->fighterLoss = LoadSound("res/Fighter Loss.mp3");
	assets->hitEnemy = LoadSound("res/Hit Enemy.mp3");
	assets->battleTheme = LoadSound("res/Battle Theme.mp3");

	Image spritesheetImage = LoadImage("res/Spritesheet.png");
	ImageFormat(&spritesheetImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
	ImageColorReplace(&spritesheetImage, BLACK, (Color) { 0, 0, 0, 0 });
	assets->spriteSheet = LoadTextureFromImage(spritesheetImage);
	UnloadImage(spritesheetImage);
	SetTextureFilter(assets->spriteSheet, TEXTURE_FILTER_POINT);

	loadAnimations(assets);

	initializeGame(gameData, assets);

	while (!WindowShouldClose())    
	{
		HandleInput(gameData, assets);
		if (GetTime() - lastTick >= 0.01)
		{
			currentTick++;
			lastTick = GetTime();
			Update(gameData, currentTick, assets);
		}
		Draw(target, gameData, assets);
	}
	CloseWindow();
	CleanUp(gameData, assets);
}