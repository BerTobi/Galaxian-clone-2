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

// Gameplay definitions

#define BLUE_ENEMY_SCORE 20
#define PURPLE_ENEMY_SCORE 40
#define DIVING_CHANCE 2
#define BLUE_ENEMY_SHOOT_CHANCE 20
#define PURPLE_ENEMY_SHOOT_CHANCE 30
#define ENEMY_TICKS_PER_DIRECTION 400

#define MAX_DIVING_ENEMIES 5
#define MAX_PLAYER_PROJECTILES 1
#define MAX_POWERUP_PROJECTILES 2

#define POWERUP_DURATION 600

#define PLAYER_BULLET_SPEED -2.0f
#define ENEMY_BULLET_SPEED 2.0f

#define ROWS_OF_BLUE_ENEMIES 3
#define COLUMNS_OF_BLUE_ENEMIES 10
#define COLUMNS_OF_PURPLE_ENEMIES 8
#define BLUE_ENEMY_STARTING_Y 40.0f
#define PURPLE_ENEMY_STARTING_Y 24.0f
#define ENEMY_SPACING 16.0f
#define ENEMY_SIZE 16.0f


// Config definitions

#define MAX_ENTITIES 100
#define NONE -1
#define TICK_DURATION 0.01f //In seconds
#define DISCONNECT_TIMEOUT 5.0 //In seconds
#define ENTITIES_PER_PACKET 20
#define MAX_PENDING_EVENTS 16
#define MAX_STATE_PACKETS 10

#define BASE_PLAYER_SPRITE (Rectangle){1, 70, 16, 16}
#define BASE_BULLET_SPRITE (Rectangle){200, 97, 1, 2}
#define BASE_BLUE_ENEMY_SPRITE (Rectangle){1, 34, 16, 16}
#define BASE_PURPLE_ENEMY_SPRITE (Rectangle){1, 17, 16, 16}
#define BASE_ENEMY_DEATH_SPRITE (Rectangle) { 61, 70, 16, 16 }
#define BASE_PLAYER_DEATH_SPRITE (Rectangle) { 1, 87, 32, 32 }

#define SPRITE_TO_HITBOX_SCALE 0.4f
#define GAME_WIDTH 224
#define GAME_HEIGHT 256
#define GAME_SCALE_FACTOR 4.0f

typedef enum NetworkMode
{
	HOST,
	CLIENT
} NetworkMode;

typedef enum NetcodeMode
{
	BLOCKING,
	NON_BLOCKING
} NetcodeMode;

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
	IP_INPUT,
	MULTIPLAYER_LOBBY,
	IN_GAME,
	GAME_OVER
} GameState;

typedef enum InputFlags
{
	INPUT_LEFT = (1 << 0),
	INPUT_RIGHT = (1 << 1),
	INPUT_SHOOT = (1 << 2)
} InputFlags;

typedef enum SoundFlags {
	SND_PLAYER_SHOOT = (1 << 0),
	SND_HIT_ENEMY = (1 << 1),
	SND_FIGHTER_LOSS = (1 << 2),
} SoundFlags;

typedef struct EventPacket {
	u8 type;							// 1 byte
} EventPacket;							// Total: 1 byte

typedef struct InputPacket {
	u8 type;							// 1 byte
	InputFlags flags;					// 4 bytes
	int lastReceivedEventID;			// 4 bytes
} InputPacket;							// Total: 9 bytes

typedef struct StatePacket {
	u8    type;							// 1 byte
	u8    entityCount;					// 1 byte
	u8	  soundFlags;					// 1 byte
	u8    totalPackets;					// 1 byte
	int score;							// 4 bytes
	int entityIndex;					// 4 bytes	
	int eventID;						// 4 bytes
	int tick;							// 4 bytes
	struct {
		float x, y;						// 8 bytes
		u8    state;					// 1 byte
		u8	  size;						// 1 byte
		Rectangle    sprite;			// 16 bytes
	} entities[ENTITIES_PER_PACKET];	// 26 bytes * 20 entities = 520 bytes
	double timestamp;					// 8 bytes
} StatePacket;							// Total: 574 bytes

typedef struct PendingEvent {
	u8 soundFlags;
	int id;
	int retries;
} PendingEvent;

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
	Sound gameOver;
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
	Animation deathAnimation;
	Animation movementAnimation;
	int diveStartTick;
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
	EntityData data;
	Rectangle baseSprite;
	int size;
} Entity;

typedef struct NetworkInfo
{
	SOCKET socket;
	NetworkMode mode;
	struct sockaddr_in clientAddr; 
	int clientConnected;
	double lastPacketTime;
	char ip[16];
	int ipLength;
	PendingEvent pendingEvents[MAX_PENDING_EVENTS];
	int pendingCount;
	int nextEventID;
	int lastPlayedEventID;
	StatePacket bufferedPartialStates[MAX_STATE_PACKETS];
	int bufferedCount;
	int bufferedTick;
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
	u8 gameOver;
} GameData;

// Function declarations

void loadAnimation(Animation* animation, int frameCount, Rectangle baseFrame, int spacing, float ticksPerFrame);
void loadAssets(Assets* assets);
void InitializeGameData(GameData* gameData, Assets* assets);
void startGame(GameData* gameData, Assets* assets);
void SoundEffect(GameData* gameData, Assets* assets, SoundFlags flag);
void ShootProjectile(GameData* gameData, Entity* shooter, Assets* assets);
void KillEntity(GameData* gameData, int index);
void GameOverScreen(GameData* gameData, Assets* assets);
void ClientInitialization(GameData* gameData, const char* ip);
void ServerInitialization(GameData* gameData);
void LobbyHandler(GameData* gameData, Assets* assets);
void NetworkHandler(GameData* gameData, Assets* assets);
void BroadcastState(GameData* gameData);

static inline void UpdateEnemy(GameData* gameData, Assets* assets, int id, int currentTick);
static inline void UpdateProjectile(GameData* gameData, Assets* assets, int id);
static inline void UpdatePlayer(GameData* gameData, Assets* assets, int id, int currentTick);

void Update(GameData* gameData, int currentTick, Assets* assets);
void Draw(RenderTexture2D target, GameData* gameData, Assets* assets);
void HandleInput(GameData* gameData, Assets* assets);
void CleanUp(GameData* gameData, Assets* assets);
void EnqueueSoundEvent(GameData* gameData, u8 soundFlags);

// DFS Ordering

void loadAssets(Assets* assets)
{
	assets->playerShoot = LoadSound("res/Shoot.mp3");
	assets->fighterLoss = LoadSound("res/Fighter Loss.mp3");
	assets->hitEnemy = LoadSound("res/Hit Enemy.mp3");
	assets->battleTheme = LoadSound("res/Battle Theme.mp3");
	assets->gameOver = LoadSound("res/Game Over.mp3");

	Image spritesheetImage = LoadImage("res/Spritesheet.png");
	ImageFormat(&spritesheetImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
	ImageColorReplace(&spritesheetImage, BLACK, (Color) { 0, 0, 0, 0 });
	assets->spriteSheet = LoadTextureFromImage(spritesheetImage);
	UnloadImage(spritesheetImage);
	SetTextureFilter(assets->spriteSheet, TEXTURE_FILTER_POINT);

	// Animations
	loadAnimation(&assets->blueEnemyMovement, 3, BASE_BLUE_ENEMY_SPRITE, 1, 50);
	loadAnimation(&assets->purpleEnemyMovement, 3, BASE_PURPLE_ENEMY_SPRITE, 1, 50);
	loadAnimation(&assets->enemyDeath, 4, BASE_ENEMY_DEATH_SPRITE, 1, 15);
	loadAnimation(&assets->playerDeath, 4, BASE_PLAYER_DEATH_SPRITE, 1, 15);

}

void loadAnimation(Animation* animation, int frameCount, Rectangle baseFrame, int spacing, float ticksPerFrame)
{
	animation->frames = malloc(sizeof(Rectangle) * frameCount);
	assert(animation->frames != NULL);
	for (int i = 0; i < frameCount; i++)
	{
		animation->frames[i] = (Rectangle){ baseFrame.x + i * (baseFrame.width + spacing), baseFrame.y, baseFrame.width, baseFrame.height };
	}
	*animation = (Animation){ .frames = animation->frames, .frameCount = frameCount, .currentFrame = 0, .ticksPerFrame = ticksPerFrame, .lastFrameTick = 0 };;
}

void InitializeGameData(GameData* gameData, Assets* assets)
{
	// Close existing socket if any
	if (gameData->network.socket != INVALID_SOCKET)
	{
		closesocket(gameData->network.socket);
		WSACleanup();
	}

	if (IsSoundPlaying(assets->battleTheme)) StopSound(assets->battleTheme);

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
	gameData->network.lastPacketTime = 0;
	gameData->network.ip[0] = '\0';
	gameData->network.ipLength = 0;
	gameData->gameOver = 0;
	gameData->network.bufferedCount = 0;
	gameData->network.bufferedTick = -1;
	gameData->network.lastPlayedEventID = -1;

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
			gameData->currentGamestate = IP_INPUT;
			gameData->network.mode = CLIENT;
		}
	}

	else if (gameData->currentGamestate == IP_INPUT)
	{
		// Type characters
		int c;
		while ((c = GetCharPressed()) != 0) {
			if ((c >= '0' && c <= '9' || c == '.') && gameData->network.ipLength < 15) {
				gameData->network.ip[gameData->network.ipLength++] = (char)c;
				gameData->network.ip[gameData->network.ipLength] = '\0';
			}
		}
		// Backspace
		if (IsKeyPressed(KEY_BACKSPACE) && gameData->network.ipLength > 0) {
			gameData->network.ip[--gameData->network.ipLength] = '\0';
		}
		// Confirm
		if (IsKeyPressed(KEY_ENTER) && gameData->network.ipLength > 0) {
			ClientInitialization(gameData, gameData->network.ip);
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

void ClientInitialization(GameData* gameData, const char* ip)
{
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port = htons(2112);
	inet_pton(AF_INET, ip, &addr.sin_addr);

	if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) printf("connect failed: %d\n", WSAGetLastError());
	else
	{
		u_long mode = NON_BLOCKING;
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
	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) printf("bind failed: %d\n", WSAGetLastError());
	else
	{
		u_long mode = NON_BLOCKING;
		ioctlsocket(sock, FIONBIO, &mode);
		printf("UDP echo server listening on port 2112...\n");
	}
	gameData->network.socket = sock;
	gameData->network.clientConnected = 0;
}

void ShootProjectile(GameData* gameData, Entity* shooter, Assets* assets)
{
	ProjectileType type = NONE;
	Vector2 velocity = { 0, 0 };
	Vector2 spawnPos = { shooter->position.x, shooter->position.y };
	if (shooter->type == PLAYER && gameData->currentPlayerProjectiles < gameData->maxPlayerProjectiles)
	{
		type = PLAYER_BULLET;
		velocity = (Vector2){ 0, PLAYER_BULLET_SPEED };
		gameData->currentPlayerProjectiles++;
		SoundEffect(gameData, assets, SND_PLAYER_SHOOT);
		spawnPos = (Vector2){ shooter->position.x, shooter->position.y - 10.0f };
	}
	else if (shooter->type == ENEMY)
	{
		type = ENEMY_BULLET;
		velocity = (Vector2){ 0, ENEMY_BULLET_SPEED };
	}
	else
		return;

	gameData->entities[gameData->entityCount] = (Entity){ .type = PROJECTILE, .state = ALIVE, .position = spawnPos, .velocity = velocity, .size = 2, .baseSprite = BASE_BULLET_SPRITE, .data.projectile.type = type };
	gameData->entityCount++;
}

void LobbyHandler(GameData* gameData, Assets* assets)
{
	EventPacket sent;
	EventPacket received;
	switch (gameData->network.mode)
	{
	case CLIENT:
	{
		int n = recv(gameData->network.socket, (char*)&received, sizeof(received), 0);
		if (n > 0)
		{
			switch (received.type)
			{
			case PKT_START:
				startGame(gameData, assets);
				gameData->currentGamestate = IN_GAME;
			case PKT_HEARTBEAT:
				gameData->network.clientConnected = 1;
			}
		}

		sent = (EventPacket){ .type = PKT_HEARTBEAT };
		send(gameData->network.socket, (char*)&sent, sizeof(sent), 0);
		break;
	}
	case HOST:
	{
		struct sockaddr_in from;
		int from_len = sizeof(from);
		int n = recvfrom(gameData->network.socket, (char*)&received, sizeof(received), 0, (struct sockaddr*)&from, &from_len);
		if (n > 0 && received.type == PKT_HEARTBEAT && !gameData->network.clientConnected)
		{
			gameData->network.clientAddr = from;
			gameData->network.clientConnected = 1;
			printf("client connected!\n");

			sent = (EventPacket){ .type = PKT_HEARTBEAT };
			sendto(gameData->network.socket, (char*)&sent, sizeof(sent), 0, (struct sockaddr*)&from, sizeof(from));
		}
		break;
	}
	}
}

void startGame(GameData* gameData, Assets* assets)
{
	gameData->network.lastPacketTime = GetTime();

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

	for (int i = 0; i < ROWS_OF_BLUE_ENEMIES; i++)
	{
		for (int j = 0; j < COLUMNS_OF_BLUE_ENEMIES; j++)
		{
			gameData->entities[gameData->entityCount] = (Entity){ .type = ENEMY, .state = ALIVE, .position = (Vector2){GAME_WIDTH / 2.0f - COLUMNS_OF_BLUE_ENEMIES * ENEMY_SPACING / 2.0f + j * ENEMY_SPACING, BLUE_ENEMY_STARTING_Y + i * ENEMY_SPACING}, .velocity = (Vector2){0.1f, 0}, .size = ENEMY_SIZE, .baseSprite = BASE_BLUE_ENEMY_SPRITE };
			gameData->entities[gameData->entityCount].data.enemy = (EnemyData){ .type = BLUE_ENEMY, .behaviour = IN_FORMATION, .movementAnimation = assets->blueEnemyMovement, .deathAnimation = assets->enemyDeath };
			gameData->entityCount++;
		}
	}

	for (int j = 0; j < COLUMNS_OF_PURPLE_ENEMIES; j++)
	{
		gameData->entities[gameData->entityCount] = (Entity){ .type = ENEMY, .state = ALIVE, .position = (Vector2){GAME_WIDTH / 2.0f - COLUMNS_OF_PURPLE_ENEMIES * ENEMY_SPACING / 2.0f + j * ENEMY_SPACING, PURPLE_ENEMY_STARTING_Y}, .velocity = (Vector2){0.1f, 0}, .size = ENEMY_SIZE, .baseSprite = BASE_PURPLE_ENEMY_SPRITE };
		gameData->entities[gameData->entityCount].data.enemy = (EnemyData){ .type = PURPLE_ENEMY, .behaviour = IN_FORMATION, .movementAnimation = assets->purpleEnemyMovement, .deathAnimation = assets->enemyDeath };
		gameData->entityCount++;
	}
}

void Update(GameData* gameData, int currentTick, Assets* assets)
{
	gameData->maxPlayerProjectiles = gameData->powerupTicks > 0 ? MAX_POWERUP_PROJECTILES : 1;
	if (gameData->powerupTicks > 0) gameData->powerupTicks--;

	for (int i = 0; i < gameData->entityCount; i++)
	{
		gameData->entities[i].position.x += gameData->entities[i].velocity.x;
		gameData->entities[i].position.y += gameData->entities[i].velocity.y;

		switch (gameData->entities[i].type)
		{
		case ENEMY:	UpdateEnemy(gameData, assets, i, currentTick);		break;
		case PROJECTILE: UpdateProjectile(gameData, assets, i);			break;
		case PLAYER: UpdatePlayer(gameData, assets, i, currentTick);	break;
		}
	}
}

static inline void UpdateEnemy(GameData* gameData, Assets* assets, int id, int currentTick)
{
	if (gameData->entities[id].state == DYING)
	{
		if (currentTick - gameData->entities[id].data.enemy.deathAnimation.lastFrameTick >= gameData->entities[id].data.enemy.deathAnimation.ticksPerFrame)
		{
			gameData->entities[id].data.enemy.deathAnimation.currentFrame = (gameData->entities[id].data.enemy.deathAnimation.currentFrame + 1);
			gameData->entities[id].data.enemy.deathAnimation.lastFrameTick = currentTick;
		}
		if (gameData->entities[id].data.enemy.deathAnimation.currentFrame >= gameData->entities[id].data.enemy.deathAnimation.frameCount)
			KillEntity(gameData, id);
		return;
	}

	if (gameData->entities[id].data.enemy.behaviour == IN_FORMATION)
	{
		if (currentTick % ENEMY_TICKS_PER_DIRECTION == 0)
		{
			gameData->entities[id].velocity.x *= -1.0f;
		}

		if (gameData->currentDivingEnemies < gameData->maxDivingEnemies)
		{
			int divingChance = rand() % 10000;

			if (divingChance < DIVING_CHANCE)
			{
				gameData->entities[id].data.enemy.behaviour = DIVING;
				gameData->entities[id].data.enemy.diveStartTick = currentTick;
				gameData->currentDivingEnemies++;
			}
		}
	}

	if (gameData->entities[id].data.enemy.behaviour == DIVING)
	{
		int shootChance = rand() % 10000;
		float t = (currentTick - gameData->entities[id].data.enemy.diveStartTick) * (TICK_DURATION * 2);
		if (gameData->entities[id].data.enemy.type == BLUE_ENEMY)
		{
			gameData->entities[id].velocity = (Vector2){ cosf(t) * 1.5f, sinf(t * 0.5f) * 0.5f };
			if (shootChance < BLUE_ENEMY_SHOOT_CHANCE)
			{
				ShootProjectile(gameData, &gameData->entities[id], assets);
			}
		}
		else if (gameData->entities[id].data.enemy.type == PURPLE_ENEMY)
		{
			gameData->entities[id].velocity = (Vector2){ cosf(t) * 3.5f, sinf(t * 0.7f) * 0.7f };
			if (shootChance < PURPLE_ENEMY_SHOOT_CHANCE)
			{
				ShootProjectile(gameData, &gameData->entities[id], assets);
			}
		}
	}

	if (currentTick - gameData->entities[id].data.enemy.movementAnimation.lastFrameTick >= gameData->entities[id].data.enemy.movementAnimation.ticksPerFrame)
	{
		gameData->entities[id].data.enemy.movementAnimation.currentFrame = (gameData->entities[id].data.enemy.movementAnimation.currentFrame + 1) % gameData->entities[id].data.enemy.movementAnimation.frameCount;
		gameData->entities[id].data.enemy.movementAnimation.lastFrameTick = currentTick;
	}

}

static inline void UpdateProjectile(GameData* gameData, Assets* assets, int id)
{
	if (gameData->entities[id].position.y < 0 || gameData->entities[id].position.y > 256)
	{
		if (gameData->entities[id].data.projectile.type == PLAYER_BULLET)
		{
			gameData->currentPlayerProjectiles--;
		}
		KillEntity(gameData, id);
		return;
	}

	if (gameData->entities[id].data.projectile.type == PLAYER_BULLET)
	{
		for (int j = 0; j < gameData->entityCount; j++)
		{
			if (gameData->entities[j].type == ENEMY && gameData->entities[j].state == ALIVE)
			{
				if (CheckCollisionCircles(gameData->entities[id].position, gameData->entities[id].size * SPRITE_TO_HITBOX_SCALE, gameData->entities[j].position, gameData->entities[j].size * SPRITE_TO_HITBOX_SCALE))
				{
					if (gameData->entities[j].data.enemy.behaviour == DIVING)
					{
						gameData->currentDivingEnemies--;
					}

					switch (gameData->entities[j].data.enemy.type)
					{
					case PURPLE_ENEMY:
						gameData->score += PURPLE_ENEMY_SCORE;
						gameData->powerupTicks += POWERUP_DURATION;
						break;
					case BLUE_ENEMY:
						gameData->score += BLUE_ENEMY_SCORE;
						break;
					}
					if (j == gameData->entityCount - 1) j = id;
					KillEntity(gameData, id);
					gameData->entities[j].state = DYING;
					SoundEffect(gameData, assets, SND_HIT_ENEMY);
					gameData->currentPlayerProjectiles--;
					return;
				}
			}
		}
	}
}

static inline void UpdatePlayer(GameData* gameData, Assets* assets, int id, int currentTick)
{
	if (gameData->entities[id].state == DYING)
	{
		if (currentTick - gameData->entities[id].data.player.deathAnimation.lastFrameTick >= gameData->entities[id].data.player.deathAnimation.ticksPerFrame)
		{
			gameData->entities[id].data.player.deathAnimation.currentFrame = (gameData->entities[id].data.player.deathAnimation.currentFrame + 1);
			gameData->entities[id].data.player.deathAnimation.lastFrameTick = currentTick;
		}
		if (gameData->entities[id].data.player.deathAnimation.currentFrame >= gameData->entities[id].data.player.deathAnimation.frameCount)
		{
			int anyPlayerAlive = 0;
			for (int j = 0; j < gameData->entityCount; j++)
			{
				if (j != id && gameData->entities[j].type == PLAYER && gameData->entities[j].state == ALIVE)
				{
					anyPlayerAlive = 1;
					break;
				}
			}
			if (!anyPlayerAlive)
			{
				gameData->gameOver = 1;

				GameOverScreen(gameData, assets);
				InitializeGameData(gameData, assets);
				return;
			}
			else
			{
				KillEntity(gameData, id);
			}
		}
		return;
	}

	for (int j = 0; j < gameData->entityCount; j++)
	{
		if ((gameData->entities[j].type == PROJECTILE && gameData->entities[j].data.projectile.type == ENEMY_BULLET) || (gameData->entities[j].type == ENEMY && gameData->entities[j].state == ALIVE))
		{
			if (CheckCollisionCircles(gameData->entities[id].position, gameData->entities[id].size * SPRITE_TO_HITBOX_SCALE, gameData->entities[j].position, gameData->entities[j].size * SPRITE_TO_HITBOX_SCALE))
			{
				printf("player %d hit, position %.1f %.1f\n", id, gameData->entities[id].position.x, gameData->entities[id].position.y);
				SoundEffect(gameData, assets, SND_FIGHTER_LOSS);
				gameData->entities[id].state = DYING;
				return;
			}
		}
	}

}

void SoundEffect(GameData* gameData, Assets* assets, SoundFlags flag)
{
	switch (flag)
	{
	case SND_PLAYER_SHOOT:
		PlaySound(assets->playerShoot);
		EnqueueSoundEvent(gameData, SND_PLAYER_SHOOT);
		break;
	case SND_FIGHTER_LOSS:
		PlaySound(assets->fighterLoss);
		EnqueueSoundEvent(gameData, SND_FIGHTER_LOSS);
		break;
	case SND_HIT_ENEMY:
		PlaySound(assets->hitEnemy);
		EnqueueSoundEvent(gameData, SND_HIT_ENEMY);
		break;
	}

}

void EnqueueSoundEvent(GameData* gameData, u8 soundFlags)
{
	if (gameData->network.pendingCount >= MAX_PENDING_EVENTS) return;
	PendingEvent* event = &gameData->network.pendingEvents[gameData->network.pendingCount++];
	event->soundFlags = soundFlags;
	event->id = gameData->network.nextEventID++;
	event->retries = 0;
}

void KillEntity(GameData* gameData, int index)
{

	gameData->entities[index] = gameData->entities[gameData->entityCount - 1];
	gameData->entityCount--;

}

void BroadcastState(GameData* gameData, int currentTick)
{
	if (!gameData->network.clientConnected) return;

	if (gameData->gameOver != 0)
	{
		EventPacket gameOverPkt = { .type = PKT_GAMEOVER };
		sendto(gameData->network.socket, (char*)&gameOverPkt, sizeof(gameOverPkt), 0,
			(struct sockaddr*)&gameData->network.clientAddr, sizeof(gameData->network.clientAddr));
	}

	StatePacket pkt = { .type = PKT_STATE };

	pkt.score = gameData->score;
	pkt.entityCount = gameData->entityCount;
	pkt.timestamp = GetTime();
	int totalPackets = gameData->entityCount / ENTITIES_PER_PACKET + 1;
	pkt.totalPackets = totalPackets;
	pkt.tick = currentTick;

	for (int j = 0; j < gameData->entityCount / ENTITIES_PER_PACKET + 1; j++)
	{
		pkt.entityIndex = j * ENTITIES_PER_PACKET;
		for (int i = 0; i < ENTITIES_PER_PACKET; i++)
		{
			if (pkt.entityIndex + i >= gameData->entityCount) break;
			pkt.entities[i].x = gameData->entities[pkt.entityIndex + i].position.x;
			pkt.entities[i].y = gameData->entities[pkt.entityIndex + i].position.y;
			pkt.entities[i].state = gameData->entities[pkt.entityIndex + i].state;
			pkt.entities[i].size = gameData->entities[pkt.entityIndex + i].size;

			Rectangle sprite;
			switch (gameData->entities[pkt.entityIndex + i].type) {
			case PLAYER:
				if (gameData->entities[pkt.entityIndex + i].state == ALIVE)
					sprite = gameData->entities[pkt.entityIndex + i].baseSprite;
				else
					sprite = gameData->entities[pkt.entityIndex + i].data.player.deathAnimation.frames[gameData->entities[pkt.entityIndex + i].data.player.deathAnimation.currentFrame];
				break;
			case PROJECTILE: sprite = gameData->entities[pkt.entityIndex + i].baseSprite; break;
			case ENEMY:
				if (gameData->entities[pkt.entityIndex + i].state == ALIVE)
					sprite = gameData->entities[pkt.entityIndex + i].data.enemy.movementAnimation.frames[gameData->entities[pkt.entityIndex + i].data.enemy.movementAnimation.currentFrame];
				else
					sprite = gameData->entities[pkt.entityIndex + i].data.enemy.deathAnimation.frames[gameData->entities[pkt.entityIndex + i].data.enemy.deathAnimation.currentFrame];
				break;
			default:
				sprite = gameData->entities[pkt.entityIndex + i].baseSprite;
				break;
			}
			pkt.entities[i].sprite = sprite;

		}

		if (gameData->network.pendingCount > 0)
		{
			PendingEvent* event = &gameData->network.pendingEvents[0];
			pkt.soundFlags = event->soundFlags;
			pkt.eventID = event->id;
			event->retries++;
			if (event->retries > 10) memmove(&gameData->network.pendingEvents, &gameData->network.pendingEvents + 1, (--gameData->network.pendingCount) * sizeof(PendingEvent));
		}
		else
		{
			pkt.soundFlags = 0;
			pkt.eventID = -1;
		}

		sendto(gameData->network.socket, (char*)&pkt, sizeof(pkt), 0,
			(struct sockaddr*)&gameData->network.clientAddr, sizeof(gameData->network.clientAddr));
	}

	
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
		if (n > 0 && input.type == PKT_INPUT)
		{
			gameData->network.lastPacketTime = GetTime();
			Entity* p2 = &gameData->entities[1];
			if (p2->type == PLAYER)
			{
				if (input.flags & (1 << 0)) p2->velocity.x = -1.0f;
				else if (input.flags & (1 << 1)) p2->velocity.x = 1.0f;
				else                             p2->velocity.x = 0.0f;
				if (input.flags & (1 << 2)) ShootProjectile(gameData, p2, assets);
			}
		}

		if (n > 0 && input.type == PKT_HEARTBEAT)
		{
			if (!gameData->network.clientConnected)
			{
				gameData->network.clientAddr = from;
				gameData->network.clientConnected = 1;
				gameData->network.lastPacketTime = GetTime();
				printf("client reconnected!\n");
				EventPacket pkt = { .type = PKT_START };
				sendto(gameData->network.socket, (char*)&pkt, sizeof(pkt), 0,
					(struct sockaddr*)&from, sizeof(from));
			}
		}

		if (n > 0 && input.type == PKT_GAMEOVER)
		{
			GameOverScreen(gameData, assets);
			InitializeGameData(gameData, assets);
		}

		if (gameData->network.clientConnected && GetTime() - gameData->network.lastPacketTime > DISCONNECT_TIMEOUT)
		{
			printf("Client timed out\n");
			gameData->network.clientConnected = 0;
		}
	}
	else if (gameData->network.mode == CLIENT)
	{
		// Send local input to host
		InputPacket input = { .type = PKT_INPUT, .flags = 0 };
		if (IsKeyDown(KEY_LEFT))        input.flags |= (1 << 0);
		if (IsKeyDown(KEY_RIGHT))        input.flags |= (1 << 1);
		if (IsKeyPressed(KEY_LEFT_CONTROL)) input.flags |= (1 << 2);

		// Receive state updates from host
		StatePacket statePacket;
		int n;
		while ((n = recv(gameData->network.socket, (char*)&statePacket, sizeof(statePacket), 0)) > 0)
		{
			if (statePacket.tick != gameData->network.bufferedTick) {
				gameData->network.bufferedCount = 0;
				gameData->network.bufferedTick = statePacket.tick;
			}

			if (gameData->network.bufferedCount < MAX_STATE_PACKETS) gameData->network.bufferedPartialStates[gameData->network.bufferedCount++] = statePacket;

			if (gameData->network.bufferedCount == statePacket.totalPackets)
			{
				for (int packet = 0; packet < gameData->network.bufferedCount; packet++)
				{
					StatePacket* statePacket = &gameData->network.bufferedPartialStates[packet];
					gameData->score = statePacket->score;
					gameData->entityCount = statePacket->entityCount;
					gameData->network.lastPacketTime = GetTime();
					for (int i = 0; i < ENTITIES_PER_PACKET; i++) {
						int id = i + statePacket->entityIndex;
						if (id >= statePacket->entityCount) break;
						gameData->entities[id].position.x = statePacket->entities[i].x;
						gameData->entities[id].position.y = statePacket->entities[i].y;
						gameData->entities[id].state = statePacket->entities[i].state;
						gameData->entities[id].size = statePacket->entities[i].size;
						gameData->entities[id].baseSprite = statePacket->entities[i].sprite;
					}
					// Sound only from the packet that carries the event
					if (statePacket->eventID >= 0 && statePacket->eventID > gameData->network.lastPlayedEventID) {
						gameData->network.lastPlayedEventID = statePacket->eventID;
						if (statePacket->soundFlags & SND_PLAYER_SHOOT) PlaySound(assets->playerShoot);
						if (statePacket->soundFlags & SND_HIT_ENEMY)    PlaySound(assets->hitEnemy);
						if (statePacket->soundFlags & SND_FIGHTER_LOSS)  PlaySound(assets->fighterLoss);
					}
				}
				// Ack the event
				input.lastReceivedEventID = statePacket.eventID;
			}

			if (statePacket.type == PKT_GAMEOVER)
			{
				GameOverScreen(gameData, assets);
				InitializeGameData(gameData, assets);
			}

			if (gameData->network.clientConnected && GetTime() - gameData->network.lastPacketTime > DISCONNECT_TIMEOUT)
			{
				printf("Host timed out\n");
				gameData->network.clientConnected = 0;
				InitializeGameData(gameData, assets);
			}
		}
		send(gameData->network.socket, (char*)&input, sizeof(input), 0);
	}
}

void GameOverScreen(GameData* gameData, Assets* assets)
{
	if (IsSoundPlaying(assets->battleTheme)) StopSound(assets->battleTheme);
	BeginDrawing();
	DrawText("GAME OVER", gameData->config.screenWidth / 2 - MeasureText("GAME OVER", 60) / 2, gameData->config.screenHeight / 2 - 30, 60, WHITE);
	EndDrawing();
	PlaySound(assets->gameOver);
	WaitTime(5.0);
	return;
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

	if (gameData->currentGamestate == IP_INPUT)
	{
		Rectangle inputBox = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2 - 12, 150, 25 };
		DrawRectangleRec(inputBox, WHITE);
		DrawText(gameData->network.ip, GAME_WIDTH / 2 - MeasureText(gameData->network.ip, 12) / 2, GAME_HEIGHT / 2 - 6, 12, BLACK);
		DrawText("ENTER IP", GAME_WIDTH / 2 - MeasureText("ENTER IP", 10) / 2, GAME_HEIGHT / 2 - 30, 10, WHITE);
		DrawText("PRESS ENTER TO CONNECT", GAME_WIDTH / 2 - MeasureText("PRESS ENTER TO CONNECT", 8) / 2, GAME_HEIGHT / 2 + 20, 8, WHITE);
	}

	if (gameData->currentGamestate == MULTIPLAYER_LOBBY)
	{
		Rectangle playerOneStatus = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2 - 75, 150, 25 };
		Rectangle playerTwoStatus = (Rectangle){ GAME_WIDTH / 2 - 75, GAME_HEIGHT / 2 - 25, 150, 25 };

		DrawRectangleRec(playerOneStatus, (gameData->network.mode == HOST || (gameData->network.mode == CLIENT && gameData->network.clientConnected)) ? GREEN : WHITE);
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
		if (IsSoundPlaying(assets->battleTheme) == false)
		{
			PlaySound(assets->battleTheme);
			SetSoundVolume(assets->battleTheme, 0.5f);
		}
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
		(Rectangle) {
		0, 0, GAME_WIDTH, -GAME_HEIGHT
	},
		(Rectangle) {
		0, 0, (float)gameData->config.screenWidth, (float)gameData->config.screenHeight
	},
		(Vector2) {
		0, 0
	},
		0.0f,
		WHITE
	);
	EndDrawing();
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

	InitializeGameData(gameData, assets);

	while (!WindowShouldClose())    
	{
		HandleInput(gameData, assets);

		switch (gameData->currentGamestate)
		{
			case MULTIPLAYER_LOBBY:
				LobbyHandler(gameData, assets);
				break;
			case IN_GAME:
				if (GetTime() - lastTick >= TICK_DURATION)
				{
					currentTick++;
					lastTick = GetTime();
					Update(gameData, currentTick, assets);
					if (gameData->network.mode == HOST) BroadcastState(gameData, currentTick);
				}
				NetworkHandler(gameData, assets);
				break;
		}

		Draw(target, gameData, assets);
	}
	CloseWindow();
	CleanUp(gameData, assets);
}