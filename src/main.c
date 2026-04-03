#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

#include "raylib.h"

// Types

#define u8 uint8_t
#define u16 uint16_t

// Game constants

#define MAX_ENTITIES 1000
#define NONE -1

#define BASE_PLAYER_SPRITE (Rectangle){1, 70, 16, 16}
#define BASE_BULLET_SPRITE (Rectangle){200, 97, 1, 2}
#define BASE_BLUE_ENEMY_SPRITE (Rectangle){1, 34, 16, 16}
#define BASE_PURPLE_ENEMY_SPRITE (Rectangle){1, 17, 16, 16}

#define SPRITE_TO_HITBOX_SCALE 0.4f
#define GAME_WIDTH 224
#define GAME_HEIGHT 256
#define GAME_SCALE_FACTOR 4.0f

#define BLUE_ENEMY_SCORE 20
#define PURPLE_ENEMY_SCORE 40
#define DIVING_CHANCE 2
#define BLUE_ENEMY_SHOOT_CHANCE 20
#define PURPLE_ENEMY_SHOOT_CHANCE 30
#define ENEMY_TICKS_PER_DIRECTION 400

#define MAX_DIVING_ENEMIES 5
#define MAX_PLAYER_PROJECTILES 1
#define MAX_POWERUP_PROJECTILES 2

#define TICK_DURATION 0.01f //In seconds
#define POWERUP_DURATION 600

typedef enum NetworkMode
{
	HOST,
	CLIENT
} NetworkMode;

typedef enum PacketType
{
	PKT_HEARTBEAT,
	PKT_INPUT,
	PKT_STATE,
	PKT_START,
	PKT_GAMEOVER
} PacketType;

typedef enum GameState
{
	MAIN_MENU,
	MULTIPLAYER_MENU,
	MULTIPLAYER_LOBBY,
	IN_GAME,
	GAME_OVER
} GameState;

typedef enum InputFlags
{
	INPUT_LEFT,
	INPUT_RIGHT,
	INPUT_SHOOT
} InputFlags;

typedef enum SoundFlags {
	SND_PLAYER_SHOOT = (1 << 0),
	SND_HIT_ENEMY = (1 << 1),
	SND_FIGHTER_LOSS = (1 << 2),
} SoundFlags;

typedef struct EventPacket {
	PacketType type;
} EventPacket;

typedef struct InputPacket {
	PacketType type;
	InputFlags flags; 
} InputPacket;

typedef struct StatePacket {
	u8    type; // PacketType
	int score;
	u8    entityCount;
	u8    soundFlags;
	struct {
		float x, y;
		u8    state;
		u8	  size;
		Rectangle    sprite;
	} entities[MAX_ENTITIES];
} StatePacket;

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

typedef struct NetworkInfo
{
	SOCKET socket;
	NetworkMode mode;
	struct sockaddr_in clientAddr; 
	int clientConnected;
} NetworkInfo;

typedef struct GameData
{
	Entity entities[MAX_ENTITIES];
	Configuration config;
	NetworkInfo network;
	int entityCount;
	int score;
	int powerupTicks;
	u8 maxPlayerProjectiles;
	u8 currentPlayerProjectiles;
	u8 maxDivingEnemies;
	u8 currentDivingEnemies;
	u8 currentGamestate;
	u8 soundFlags;
} GameData;

void loadFrames(Rectangle* frames, int frameCount, Rectangle baseFrame, int spacing)
{
	for (int i = 0; i < frameCount; i++)
	{
		frames[i] = (Rectangle){ baseFrame.x + i * (baseFrame.width + spacing), baseFrame.y, baseFrame.width, baseFrame.height };
	}
}

void loadAssets(Assets* assets)
{
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

	// Animations
	// Blue enemy movement
	assets->blueEnemyMovement.frames = malloc(sizeof(Rectangle) * 3);
	assert(assets->blueEnemyMovement.frames != NULL);
	loadFrames(assets->blueEnemyMovement.frames, 3, BASE_BLUE_ENEMY_SPRITE, 1);
	assets->blueEnemyMovement = (Animation){ .frames = assets->blueEnemyMovement.frames, .frameCount = 3, .currentFrame = 0, .ticksPerFrame = 50, .lastFrameTick = 0 };

	// Purple enemy movement
	assets->purpleEnemyMovement.frames = malloc(sizeof(Rectangle) * 3);
	assert(assets->purpleEnemyMovement.frames != NULL);
	loadFrames(assets->purpleEnemyMovement.frames, 3, BASE_PURPLE_ENEMY_SPRITE, 1);
	assets->purpleEnemyMovement = (Animation){ .frames = assets->purpleEnemyMovement.frames, .frameCount = 3, .currentFrame = 0, .ticksPerFrame = 50, .lastFrameTick = 0 };


	// Enemy death
	assets->enemyDeath.frames = malloc(sizeof(Rectangle) * 4);
	assert(assets->enemyDeath.frames != NULL);
	loadFrames(assets->enemyDeath.frames, 4, (Rectangle) { 61, 70, 16, 16 }, 1);	
	assets->enemyDeath = (Animation){ .frames = assets->enemyDeath.frames, .frameCount = 4, .currentFrame = 0, .ticksPerFrame = 15, .lastFrameTick = 0 };

	// Player death
	assets->playerDeath.frames = malloc(sizeof(Rectangle) * 4);
	assert(assets->playerDeath.frames != NULL);
	loadFrames(assets->playerDeath.frames, 4, (Rectangle) { 1, 87, 32, 32 }, 1);
	assets->playerDeath = (Animation){ .frames = assets->playerDeath.frames, .frameCount = 4, .currentFrame = 0, .ticksPerFrame = 15, .lastFrameTick = 0 };

}

void initializeGameData(GameData* gameData, Assets* assets)
{
	// Close existing socket if any
	if (gameData->network.socket != INVALID_SOCKET)
	{
		closesocket(gameData->network.socket);
		WSACleanup();
	}

	gameData->currentGamestate = MAIN_MENU;
	gameData->entityCount = 0;
	gameData->score = 0;
	gameData->maxPlayerProjectiles = MAX_PLAYER_PROJECTILES;
	gameData->currentPlayerProjectiles = 0;
	gameData->currentDivingEnemies = 0;
	gameData->maxDivingEnemies = MAX_DIVING_ENEMIES;
	gameData->powerupTicks = 0;
	gameData->network.socket = INVALID_SOCKET;
	gameData->network.clientConnected = 0;
	gameData->soundFlags = 0;
	
}

void startGame(GameData* gameData, Assets* assets)
{
	for (int i = 0; i < MAX_ENTITIES; i++)
	{
		gameData->entities[i] = (Entity){ .type = NONE, .position = {0, 0}, .velocity = {0, 0}, .size = 0 };
	}

	gameData->entities[0] = (Entity){ .type = PLAYER, .state = ALIVE, .position = (Vector2){gameData->network.clientConnected == 1 ? 92.0f : 112.0f, 240}, .size = 16, .baseSprite = BASE_PLAYER_SPRITE };
	gameData->entities[0].data.player.deathAnimation = assets->playerDeath;
	gameData->entityCount++;

	if (gameData->network.clientConnected == 1) 
	{
		gameData->entities[1] = (Entity){ .type = PLAYER, .state = ALIVE, .position = (Vector2){132, 240}, .size = 16, .baseSprite = BASE_PLAYER_SPRITE };
		gameData->entities[1].data.player.deathAnimation = assets->playerDeath;
		gameData->entityCount++;
	}

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			gameData->entities[gameData->entityCount] = (Entity){ .type = ENEMY, .state = ALIVE, .position = (Vector2){32.0f + j * 16.0f, 40.0f + i * 16.0f}, .velocity = (Vector2){0.1f, 0}, .size = 16, .baseSprite = BASE_BLUE_ENEMY_SPRITE, .data.enemy.type = BLUE_ENEMY };
			gameData->entities[gameData->entityCount].data.enemy = (EnemyData){ .type = BLUE_ENEMY, .behaviour = IN_FORMATION, .movementAnimation = assets->blueEnemyMovement, .deathAnimation = assets->enemyDeath };
			gameData->entityCount++;
		}
	}

	for (int j = 0; j < 8; j++)
	{
		gameData->entities[gameData->entityCount] = (Entity){ .type = ENEMY, .state = ALIVE, .position = (Vector2){48.0f + j * 16.0f, 24.0f}, .velocity = (Vector2){0.1f, 0}, .size = 16, .baseSprite = BASE_PURPLE_ENEMY_SPRITE, .data.enemy.type = BLUE_ENEMY };
		gameData->entities[gameData->entityCount].data.enemy = (EnemyData){ .type = PURPLE_ENEMY, .behaviour = IN_FORMATION, .movementAnimation = assets->purpleEnemyMovement, .deathAnimation = assets->enemyDeath };
		gameData->entityCount++;
	}
}

void ClientInitialization(GameData* gameData)
{
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port = htons(2112);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		printf("connect failed: %d\n", WSAGetLastError());
	}
	else {
		// Set non-blocking so recv never stalls the game loop
		u_long mode = 1;
		ioctlsocket(sock, FIONBIO, &mode);
		printf("UDP socket ready\n");
	}
	gameData->network.socket = sock;
}

void ServerInitialization(GameData* gameData)
{
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(2112);
	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		printf("bind failed: %d\n", WSAGetLastError());
	}
	else {
		u_long mode = 1;
		ioctlsocket(sock, FIONBIO, &mode);
		printf("UDP echo server listening on port 2112...\n");
	}
	gameData->network.socket = sock;
	gameData->network.clientConnected = 0;
}

void LobbyHandler(GameData* gameData, Assets* assets)
{
	if (gameData->network.mode == CLIENT) {
		EventPacket pkt = { .type = PKT_HEARTBEAT };
		send(gameData->network.socket, (char*)&pkt, sizeof(pkt), 0);

		if (!gameData->network.clientConnected) {
			EventPacket response;
			int n = recv(gameData->network.socket, (char*)&response, sizeof(response), 0);
			if (n > 0 && response.type == PKT_HEARTBEAT)
				gameData->network.clientConnected = 1;
		}

		EventPacket eventPkt;
		int n = recv(gameData->network.socket, (char*)&eventPkt, sizeof(eventPkt), 0);
		if (n > 0 && eventPkt.type == PKT_START) {
			startGame(gameData, assets);
			gameData->currentGamestate = IN_GAME;
		}

	}
	else if (gameData->network.mode == HOST) {
		EventPacket pkt;
		struct sockaddr_in from;
		int from_len = sizeof(from);
		int n = recvfrom(gameData->network.socket, (char*)&pkt, sizeof(pkt), 0,
			(struct sockaddr*)&from, &from_len);
		if (n > 0 && pkt.type == PKT_HEARTBEAT && !gameData->network.clientConnected) {
			gameData->network.clientAddr = from;
			gameData->network.clientConnected = 1;
			printf("client connected!\n");

			EventPacket ack = { .type = PKT_HEARTBEAT };
			sendto(gameData->network.socket, (char*)&ack, sizeof(ack), 0,
				(struct sockaddr*)&from, sizeof(from));
		}
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
		gameData->soundFlags |= SND_PLAYER_SHOOT;
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

void GameOverScreen(GameData* gameData)
{
	BeginDrawing();
	DrawText("GAME OVER", gameData->config.screenWidth / 2 - MeasureText("GAME OVER", 60) / 2, gameData->config.screenHeight / 2 - 30, 60, WHITE);
	EndDrawing();
	WaitTime(5.0);
	return;
}

void NetworkHandler(GameData* gameData, Assets* assets)
{
	if (gameData->network.socket == INVALID_SOCKET) return;

	if (gameData->network.mode == HOST)
	{
		// Receive client input, apply to player 2
		InputPacket input;
		struct sockaddr_in from;
		int from_len = sizeof(from);
		int n = recvfrom(gameData->network.socket, (char*)&input, sizeof(input), 0,
			(struct sockaddr*)&from, &from_len);
		if (n > 0 && input.type == PKT_INPUT) {
			Entity* p2 = &gameData->entities[1];
			if (p2->type == PLAYER)
			{
				if (input.flags & (1 << 0)) p2->velocity.x = -1.0f;
				else if (input.flags & (1 << 1)) p2->velocity.x = 1.0f;
				else                             p2->velocity.x = 0.0f;
				if (input.flags & (1 << 2)) ShootProjectile(gameData, p2, assets);
			}
		}

		if (n > 0 && input.type == PKT_GAMEOVER) {
			GameOverScreen(gameData);
			initializeGameData(gameData, assets);
		}
	}
	else if (gameData->network.mode == CLIENT)
	{
		// Send local input to host
		InputPacket input = { .type = PKT_INPUT, .flags = 0 };
		if (IsKeyDown(KEY_LEFT))        input.flags |= (1 << 0);
		if (IsKeyDown(KEY_RIGHT))        input.flags |= (1 << 1);
		if (IsKeyPressed(KEY_LEFT_CONTROL)) input.flags |= (1 << 2);
		send(gameData->network.socket, (char*)&input, sizeof(input), 0);

		// Receive state updates from host
		StatePacket statePacket;
		int n = recv(gameData->network.socket, (char*)&statePacket, sizeof(statePacket), 0);
		if (n > 0 && statePacket.type == PKT_STATE) {
			gameData->score = statePacket.score;
			gameData->entityCount = statePacket.entityCount;
			for (int i = 0; i < statePacket.entityCount; i++) {
				gameData->entities[i].position.x = statePacket.entities[i].x;
				gameData->entities[i].position.y = statePacket.entities[i].y;
				gameData->entities[i].state = statePacket.entities[i].state;
				gameData->entities[i].size = statePacket.entities[i].size;
				// animation frame for Draw()
				gameData->entities[i].baseSprite = statePacket.entities[i].sprite;
			}
			if (statePacket.soundFlags & SND_PLAYER_SHOOT) PlaySound(assets->playerShoot);
			if (statePacket.soundFlags & SND_HIT_ENEMY)    PlaySound(assets->hitEnemy);
			if (statePacket.soundFlags & SND_FIGHTER_LOSS)  PlaySound(assets->fighterLoss);
		}

		if (n > 0 && statePacket.type == PKT_GAMEOVER)
		{
			GameOverScreen(gameData);
			initializeGameData(gameData, assets);
		}
	}
}

void BroadcastState(GameData* gameData)
{
	if (!gameData->network.clientConnected) return;

	StatePacket pkt = { .type = PKT_STATE };

	pkt.score = gameData->score;
	pkt.entityCount = gameData->entityCount;

	for (int i = 0; i < gameData->entityCount; i++) {
		pkt.entities[i].x = gameData->entities[i].position.x;
		pkt.entities[i].y = gameData->entities[i].position.y;
		pkt.entities[i].state = gameData->entities[i].state;
		pkt.entities[i].size = gameData->entities[i].size;
		
		Rectangle sprite;
		switch (gameData->entities[i].type) {
		case PLAYER:   
			if (gameData->entities[i].state == ALIVE)
				sprite = gameData->entities[i].baseSprite;
			else
				sprite = gameData->entities[i].data.player.deathAnimation.frames[gameData->entities[i].data.player.deathAnimation.currentFrame];
			break;
		case PROJECTILE: sprite = gameData->entities[i].baseSprite; break;
		case ENEMY:
			if (gameData->entities[i].state == ALIVE)
				sprite = gameData->entities[i].data.enemy.movementAnimation.frames[gameData->entities[i].data.enemy.movementAnimation.currentFrame];
			else
				sprite = gameData->entities[i].data.enemy.deathAnimation.frames[gameData->entities[i].data.enemy.deathAnimation.currentFrame];
			break;
		}
		pkt.entities[i].sprite = sprite;
		
	}

	pkt.soundFlags = gameData->soundFlags;
	gameData->soundFlags = 0;

	sendto(gameData->network.socket, (char*)&pkt, sizeof(pkt), 0,
		(struct sockaddr*)&gameData->network.clientAddr, sizeof(gameData->network.clientAddr));
}

void Update(GameData* gameData, int currentTick, Assets* assets)
{
	

	if (IsSoundPlaying(assets->battleTheme) == false)
	{
		PlaySound(assets->battleTheme);
		SetSoundVolume(assets->battleTheme, 0.5f);
	}
	
	if (gameData->network.mode != CLIENT)
	{
		if (gameData->powerupTicks > 0)
		{
			gameData->maxPlayerProjectiles = MAX_POWERUP_PROJECTILES;
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
						if (currentTick % ENEMY_TICKS_PER_DIRECTION == 0)
						{
							gameData->entities[i].velocity.x *= -1.0f;
						}

						if (gameData->currentDivingEnemies < gameData->maxDivingEnemies)
						{
							int divingChance = rand() % 10000;

							if (divingChance < DIVING_CHANCE)
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
						float t = (currentTick - gameData->entities[i].data.enemy.diveStartTick) * (TICK_DURATION * 2);
						if (gameData->entities[i].data.enemy.type == BLUE_ENEMY)
						{
							gameData->entities[i].velocity = (Vector2){ cosf(t) * 1.5f, sinf(t * 0.5f) * 0.5f };
							if (shootChance < BLUE_ENEMY_SHOOT_CHANCE)
							{
								ShootProjectile(gameData, &gameData->entities[i], assets);
							}
						}
						else if (gameData->entities[i].data.enemy.type == PURPLE_ENEMY)
						{
							gameData->entities[i].velocity = (Vector2){ cosf(t) * 3.5f, sinf(t * 0.7f) * 0.7f };
							if (shootChance < PURPLE_ENEMY_SHOOT_CHANCE)
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

			if (gameData->entities[i].type == PROJECTILE && gameData->entities[i].data.projectile.type == PLAYER_BULLET)
			{
				for (int j = 0; j < gameData->entityCount; j++)
				{
					if (gameData->entities[j].type == ENEMY && gameData->entities[j].state == ALIVE)
					{
						if (CheckCollisionCircles(gameData->entities[i].position, gameData->entities[i].size * SPRITE_TO_HITBOX_SCALE, gameData->entities[j].position, gameData->entities[j].size * SPRITE_TO_HITBOX_SCALE))
						{
							if (gameData->entities[j].data.enemy.behaviour == DIVING)
							{
								gameData->currentDivingEnemies--;
							}
							if (gameData->entities[j].data.enemy.type == PURPLE_ENEMY)
							{
								gameData->score += PURPLE_ENEMY_SCORE;
								gameData->powerupTicks += POWERUP_DURATION;
							}
							else
							{
								gameData->score += BLUE_ENEMY_SCORE;
							}
							if (j == gameData->entityCount - 1) j = i;
							KillEntity(gameData, i);
							gameData->entities[j].state = DYING;
							PlaySound(assets->hitEnemy);
							gameData->soundFlags |= SND_HIT_ENEMY;
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
							if (CheckCollisionCircles(gameData->entities[i].position, gameData->entities[i].size * SPRITE_TO_HITBOX_SCALE, gameData->entities[j].position, gameData->entities[j].size * SPRITE_TO_HITBOX_SCALE))
							{
								PlaySound(assets->fighterLoss);
								gameData->soundFlags |= SND_FIGHTER_LOSS;
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
						int anyPlayerAlive = 0;
						for (int j = 0; j < gameData->entityCount; j++)
						{
							if (j != i && gameData->entities[j].type == PLAYER && gameData->entities[j].state == ALIVE)
							{
								anyPlayerAlive = 1;
								break;
							}
						}
						if (!anyPlayerAlive)
						{
							if (gameData->network.mode == HOST)
							{
								EventPacket gameOverPkt = { .type = PKT_GAMEOVER };
								sendto(gameData->network.socket, (char*)&gameOverPkt, sizeof(gameOverPkt), 0,
									(struct sockaddr*)&gameData->network.clientAddr, sizeof(gameData->network.clientAddr));
							}

							GameOverScreen(gameData);
							initializeGameData(gameData, assets);
							
						}
						else
						{
							KillEntity(gameData, i); // remove dead player but keep going
						}
					}
				}
			}
		}
	}

	
}

void Draw(RenderTexture2D target, GameData* gameData, Assets* assets)
{
	BeginTextureMode(target);
	ClearBackground(BLACK);

	if (gameData->currentGamestate == MAIN_MENU)
	{
		Rectangle singlePlayerButton = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2 - 75, 150, 50 };
		Rectangle multiplayerButton = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2, 150, 50 };

		DrawRectangleRec(singlePlayerButton, WHITE);
		DrawRectangleRec(multiplayerButton, WHITE);

		DrawText("SINGLEPLAYER", GAME_WIDTH / 2 - MeasureText("SINGLEPLAYER", 16) / 2, GAME_HEIGHT / 2 - 60, 16, BLUE);
		DrawText("MULTIPLAYER", GAME_WIDTH / 2 - MeasureText("MULTIPLAYER", 16) / 2, GAME_HEIGHT / 2 + 15, 16, BLUE);
		
	}

	if (gameData->currentGamestate == MULTIPLAYER_MENU)
	{
		Rectangle hostButton = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2 - 75, 150, 50 };
		Rectangle joinButton = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2, 150, 50 };

		DrawRectangleRec(hostButton, WHITE);
		DrawRectangleRec(joinButton, WHITE);

		DrawText("HOST", GAME_WIDTH / 2 - MeasureText("HOST", 16) / 2, GAME_HEIGHT / 2 - 60, 16, BLUE);
		DrawText("JOIN", GAME_WIDTH / 2 - MeasureText("JOIN", 16) / 2, GAME_HEIGHT / 2 + 15, 16, BLUE);
	}

	if (gameData->currentGamestate == MULTIPLAYER_LOBBY)
	{
		Rectangle playerOneStatus = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2 - 75, 150, 25 };
		Rectangle playerTwoStatus = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2 - 25, 150, 25 };
		
		DrawRectangleRec(playerOneStatus, (gameData->network.mode == HOST || (gameData->network.mode == CLIENT && gameData->network.clientConnected) )? GREEN : WHITE);
		DrawRectangleRec(playerTwoStatus, gameData->network.clientConnected ? GREEN : RED);
		

		DrawText("PLAYER 1", GAME_WIDTH / 2 - MeasureText("PLAYER 1", 16) / 2, GAME_HEIGHT / 2 - 70, 16, BLUE);
		DrawText("PLAYER 2", GAME_WIDTH / 2 - MeasureText("PLAYER 2", 16) / 2, GAME_HEIGHT / 2 - 20, 16, BLUE);
		

		if (gameData->network.mode == HOST)
		{
			Rectangle startGame = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2 + 25, 150, 30 };
			DrawRectangleRec(startGame, gameData->network.clientConnected ? WHITE : GRAY);
			DrawText("START", GAME_WIDTH / 2 - MeasureText("START", 20) / 2, GAME_HEIGHT / 2 + 30, 20, BLACK);
		}
	}

	if (gameData->currentGamestate == IN_GAME)
	{
		if (gameData->network.mode != CLIENT)
		{
			for (int i = 0; i < gameData->entityCount; i++)
			{
				Rectangle spritePosition = (Rectangle){ (float)(int)(gameData->entities[i].position.x - (gameData->entities[i].size / 2.0f)), (float)(int)(gameData->entities[i].position.y - (gameData->entities[i].size / 2.0f)), (float)gameData->entities[i].size, (float)gameData->entities[i].size };
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
		}
		else
		{
			for (int i = 0; i < gameData->entityCount; i++)
			{
				Rectangle spritePosition = (Rectangle){ (float)(int)(gameData->entities[i].position.x - (gameData->entities[i].size / 2.0f)), (float)(int)(gameData->entities[i].position.y - (gameData->entities[i].size / 2.0f)), (float)gameData->entities[i].size, (float)gameData->entities[i].size };
				DrawTexturePro(assets->spriteSheet, gameData->entities[i].baseSprite, spritePosition, (Vector2) { 0, 0 }, 0.0f, WHITE);
			}
		}
		
		char scoreText[20];
		sprintf(scoreText, "SCORE: %i", gameData->score);
		DrawText(scoreText, 8, 5, 8, WHITE);
	}
	
	EndTextureMode();

	BeginDrawing();
	ClearBackground(BLACK);
	DrawTexturePro(target.texture, 
		(Rectangle) {0, 0, GAME_WIDTH, -GAME_HEIGHT}, 
		(Rectangle) {0, 0, (float)gameData->config.screenWidth, (float)gameData->config.screenHeight},
		(Vector2) {0, 0}, 
		0.0f,
		WHITE
	);
	EndDrawing();
}

void HandleInput(GameData* gameData, Assets* assets)
{
	if (gameData->currentGamestate == MAIN_MENU)
	{
		Vector2 mousePosition = GetMousePosition();
		Vector2 gameMousePosition = (Vector2){ mousePosition.x / GAME_SCALE_FACTOR, mousePosition.y / GAME_SCALE_FACTOR };
		Rectangle singlePlayerButton = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2 - 75, 150, 50 };
		Rectangle multiplayerButton = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2, 150, 50 };
		if (CheckCollisionPointRec(gameMousePosition, singlePlayerButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
		{
			gameData->currentGamestate = IN_GAME;
			startGame(gameData, assets);
		}
		else if (CheckCollisionPointRec(gameMousePosition, multiplayerButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
		{
			gameData->currentGamestate = MULTIPLAYER_MENU;
		}
	}

	else if (gameData->currentGamestate == MULTIPLAYER_MENU)
	{
		Vector2 mousePosition = GetMousePosition();
		Vector2 gameMousePosition = (Vector2){ mousePosition.x / GAME_SCALE_FACTOR, mousePosition.y / GAME_SCALE_FACTOR };
		Rectangle hostButton = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2 - 75, 150, 50 };
		Rectangle joinButton = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2, 150, 50 };
		if (CheckCollisionPointRec(gameMousePosition, hostButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
		{
			ServerInitialization(gameData);
			gameData->currentGamestate = MULTIPLAYER_LOBBY;
			gameData->network.mode = HOST;
		}
		else if (CheckCollisionPointRec(gameMousePosition, joinButton) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
		{
			ClientInitialization(gameData);
			gameData->currentGamestate = MULTIPLAYER_LOBBY;
			gameData->network.mode = CLIENT;
		}
	}

	else if (gameData->currentGamestate == MULTIPLAYER_LOBBY)
	{
		if (gameData->network.mode == HOST && gameData->network.clientConnected)
		{
			Vector2 mousePosition = GetMousePosition();
			Vector2 gameMousePosition = (Vector2){ mousePosition.x / GAME_SCALE_FACTOR, mousePosition.y / GAME_SCALE_FACTOR };
			Rectangle start = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2 + 25, 150, 30 };
			if (CheckCollisionPointRec(gameMousePosition, start) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
			{
				startGame(gameData, assets);
				gameData->currentGamestate = IN_GAME;
				EventPacket pkt = { .type = PKT_START };
				sendto(gameData->network.socket, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*)&gameData->network.clientAddr, sizeof(gameData->network.clientAddr));
			}
		}
	}

	else if (gameData->currentGamestate == IN_GAME)
	{
		if (gameData->entities[0].type == PLAYER)
		{
			if (IsKeyDown(KEY_LEFT)) gameData->entities[0].velocity = (Vector2){ -1.0f, 0 };
			else if (IsKeyDown(KEY_RIGHT)) gameData->entities[0].velocity = (Vector2){ 1.0f, 0 };
			else gameData->entities[0].velocity = (Vector2){ 0, 0 };
			if (IsKeyPressed(KEY_LEFT_CONTROL)) ShootProjectile(gameData, &gameData->entities[0], assets);
		}
	}
}

void CleanUp(GameData* gameData, Assets* assets)
{
	if (gameData->network.socket != INVALID_SOCKET)
	{
		closesocket(gameData->network.socket);
		WSACleanup();
	}
	free(gameData);
	free(assets);
}

int main(void)
{
	srand((unsigned int)time(NULL));
	int currentTick = ENEMY_TICKS_PER_DIRECTION / 2;
	double lastTick = GetTime();
	GameData* gameData = malloc(sizeof(GameData));
	gameData->config = (Configuration){ .screenWidth = (int)(GAME_WIDTH * GAME_SCALE_FACTOR), .screenHeight = (int)(GAME_HEIGHT * GAME_SCALE_FACTOR) };
	InitWindow(gameData->config.screenWidth, gameData->config.screenHeight, "Galaxian Clone");
	InitAudioDevice();
	SetTargetFPS(60);               

	RenderTexture2D target = LoadRenderTexture(GAME_WIDTH, GAME_HEIGHT);
	SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

	Assets* assets = malloc(sizeof(Assets));
	assert(gameData != NULL && assets != NULL);

	loadAssets(assets);

	initializeGameData(gameData, assets);

	while (!WindowShouldClose())    
	{
	
		HandleInput(gameData, assets);

		if (gameData->currentGamestate == MULTIPLAYER_LOBBY)
		{
			LobbyHandler(gameData, assets);
		}
		if (gameData->currentGamestate == IN_GAME)
		{
			NetworkHandler(gameData, assets);
			if (GetTime() - lastTick >= TICK_DURATION)
			{
				currentTick++;
				lastTick = GetTime();
				Update(gameData, currentTick, assets);
				if (gameData->network.mode == HOST) BroadcastState(gameData);
			}
		}
		Draw(target, gameData, assets);
	}
	CloseWindow();
	CleanUp(gameData, assets);
}