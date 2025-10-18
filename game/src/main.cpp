#include <raylib.h>
#include <raymath.h>
#include <cassert>
#include <array>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdio>

constexpr float SCREEN_SIZE = 800;
constexpr int TILE_COUNT = 20;
constexpr float TILE_SIZE = SCREEN_SIZE / TILE_COUNT;

enum TileType : int
{
    GRASS,      // Marks unoccupied space, can be overwritten 
    DIRT,       // Marks the path, cannot be overwritten
    WAYPOINT,   // Marks where the path turns, cannot be overwritten
    TURRET,
    COUNT
};

struct Cell
{
    int row;
    int col;
};

constexpr std::array<Cell, 4> DIRECTIONS{ Cell{ -1, 0 }, Cell{ 1, 0 }, Cell{ 0, -1 }, Cell{ 0, 1 } };

inline bool InBounds(Cell cell, int rows = TILE_COUNT, int cols = TILE_COUNT)
{
    return cell.col >= 0 && cell.col < cols && cell.row >= 0 && cell.row < rows;
}

void DrawTile(int row, int col, Color color)
{
    DrawRectangle(col * TILE_SIZE, row * TILE_SIZE, TILE_SIZE, TILE_SIZE, color);
}


void DrawTile(int row, int col, int type)
{
    Color colors[4] =
    {
        LIME,
        BEIGE,
        SKYBLUE,
        YELLOW
    };
    //assert(type >= 0 && type < COUNT);
    DrawTile(row, col, colors[type]);
}

Vector2 TileCenter(int row, int col)
{
    float x = col * TILE_SIZE + TILE_SIZE * 0.5f;
    float y = row * TILE_SIZE + TILE_SIZE * 0.5f;
    return { x, y };
}

// Returns a collection of adjacent cells that match the search value.
std::vector<Cell> FloodFill(Cell start, int tiles[TILE_COUNT][TILE_COUNT], TileType searchValue)
{
    // "open" = "places we want to search", "closed" = "places we've already searched".
    std::vector<Cell> result;
    std::vector<Cell> open;
    bool closed[TILE_COUNT][TILE_COUNT];
    for (int row = 0; row < TILE_COUNT; row++)
    {
        for (int col = 0; col < TILE_COUNT; col++)
        {
            // We don't want to search zero-tiles, so add them to closed!
            closed[row][col] = tiles[row][col] == 0;
        }
    }

    // Add the starting cell to the exploration queue & search till there's nothing left!
    open.push_back(start);
    while (!open.empty())
    {
        // Remove from queue and prevent revisiting
        Cell cell = open.back();
        open.pop_back();
        closed[cell.row][cell.col] = true;

        // Add to result if explored cell has the desired value
        if (tiles[cell.row][cell.col] == searchValue)
            result.push_back(cell);

        // Search neighbours
        for (Cell dir : DIRECTIONS)
        {
            Cell adj = { cell.row + dir.row, cell.col + dir.col };
            if (InBounds(adj) && !closed[adj.row][adj.col] && tiles[adj.row][adj.col] != 0)
                open.push_back(adj);
        }
    }

    return result;
}

struct Enemy
{
    Vector2 position;
    float speed;
    float health;
    int curr;
    int next;
    bool active;
};

struct Turret
{
    Vector2 position;
    float range;
    float damage;
    float fireRate;
    float lastShotTime;
};

struct Bullet
{
    Vector2 position;
    Vector2 velocity;
    float speed;
    float damage;
    bool active;
};

enum GameState
{
    BUILD,
    RUNNING,
    GAMEOVER,
    WIN
};

struct Level
{
    int enemyCount;
    float enemySpeed;
    float enemyHealth;
};

int main()
{
    InitWindow(SCREEN_SIZE, SCREEN_SIZE, "Tower Defense - 3 Levels");
    SetTargetFPS(60);
   
    // Audio setup
    InitAudioDevice();
    Sound sTurretPlace = LoadSound("Turret_Place.wav");
    Sound sTurretBreak = LoadSound("Turret_Break.wav");
    Sound sTurretFire = LoadSound("Turret_Fire.wav");
    Sound sEnemyHit = LoadSound("Enemy_Hit.wav");
    Sound sEnemyDeath = LoadSound("Enemy_Death.wav");

    int currentLevel = 1;
    const int maxLevels = 3;
    GameState state = BUILD;

    std::vector<Level> levels = {
        { 10, 100, 100 },   // Level 1
        { 20, 120, 150 },   // Level 2
        { 30, 150, 200 }    // Level 3
    };

    int tiles[TILE_COUNT][TILE_COUNT] = {  //col:0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19    row:
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0 }, // 0
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 1
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 2
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 3
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 4
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 5
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 6
            { 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0 }, // 7
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 8
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 9
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 10
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 11
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 12
            { 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0 }, // 13
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, // 14
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, // 15
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, // 16
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0 }, // 17
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 18
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }  // 19 };
    };

    std::vector<Cell> waypoints = FloodFill({ 0, 12 }, tiles, WAYPOINT);


    std::vector<Enemy> enemies;
    std::vector<Turret> turrets;
    std::vector<Bullet> bullets;

    float spawnTimer = 0.0f;
    int enemiesSpawned = 0;
    bool levelComplete = false;
    bool playerLost = false;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        Vector2 mouse = GetMousePosition();

        if (state == BUILD)
        {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                int col = mouse.x / TILE_SIZE;
                int row = mouse.y / TILE_SIZE;
                if (InBounds({ row, col }) && tiles[row][col] == GRASS)
                {
                    Turret t;
                    t.position = TileCenter(row, col);
                    t.range = 150.0f;
                    t.damage = 20.0f;
                    t.fireRate = 1.5f;
                    t.lastShotTime = 0.0f;
                    turrets.push_back(t);
                    tiles[row][col] = TURRET;
					PlaySound(sTurretPlace);
                }
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            {
                int col = mouse.x / TILE_SIZE;
                int row = mouse.y / TILE_SIZE;
                if (InBounds({ row, col }) && tiles[row][col] == TURRET)
                {
                    tiles[row][col] = GRASS;
                    Vector2 tileCenter = TileCenter(row, col);
					PlaySound(sTurretBreak);

                    for (int i = 0; i < turrets.size(); i++)
                    {
                        if (Vector2Distance(turrets[i].position, tileCenter) < 1.0f)
                        {
                            turrets.erase(turrets.begin() + i);
          
                            break;
                        }
                    }

                }
            }

            if (turrets.size() >= 5)
            {
                enemies.clear();
                enemiesSpawned = 0;
                spawnTimer = 0.0f;
                state = RUNNING;
            }
        }

        else if (state == RUNNING)
        {
            spawnTimer += dt;
            Level& lvl = levels[currentLevel - 1];

            if (spawnTimer >= 1.0f && enemiesSpawned < lvl.enemyCount && waypoints.size() >= 2)
            {
                Enemy e;
                e.position = TileCenter(waypoints[0].row, waypoints[0].col);
                e.speed = lvl.enemySpeed;
                e.health = lvl.enemyHealth;
                e.curr = 0;
                e.next = 1;
                e.active = true;
                enemies.push_back(e);
                enemiesSpawned++;
                spawnTimer = 0.0f;
            }

            for (Enemy& e : enemies)
            {
                if (!e.active) continue;
                Vector2 from = TileCenter(waypoints[e.curr].row, waypoints[e.curr].col);
                Vector2 to = TileCenter(waypoints[e.next].row, waypoints[e.next].col);
                Vector2 dir = Vector2Normalize(to - from);
                e.position += dir * e.speed * dt;

                float minDist = e.speed / 60.0f * 1.1f;
                if (CheckCollisionPointCircle(e.position, to, minDist))
                {
                    e.position = to;
                    e.curr++;
                    e.next++;
                    if (e.next >= waypoints.size())
                    {
                        e.active = false;
                        playerLost = true;
                    }
                }
            }

            // Turrets shoot
            for (Turret& t : turrets)
            {
                t.lastShotTime += dt;
                float closest = t.range;
                Enemy* target = nullptr;

                for (Enemy& e : enemies)
                {
                    if (!e.active) continue;
                    float dist = Vector2Distance(t.position, e.position);
                    if (dist < closest)
                    {
                        closest = dist;
                        target = &e;
                    }
                }

                if (target && t.lastShotTime >= 1.0f / t.fireRate)
                {
                    Bullet b;
                    b.position = t.position;
                    Vector2 dir = Vector2Normalize(target->position - t.position);
                    b.velocity = dir;
                    b.speed = 300.0f;
                    b.damage = t.damage;
                    b.active = true;
                    bullets.push_back(b);
					PlaySound(sTurretFire); 

                    t.lastShotTime = 0.0f;
                }
            }

            for (Bullet& b : bullets)
            {
                if (!b.active) continue;
                b.position += b.velocity * b.speed * dt;

                for (Enemy& e : enemies)
                {
                    if (!e.active) continue;
                    if (CheckCollisionCircles(b.position, 5, e.position, 10))
                    {
                        e.health -= b.damage;
                        b.active = false;
						PlaySound(sEnemyHit);
                        break;
                    }
                }
            }

            for (int i = 0; i < bullets.size(); )
            {
                if (!bullets[i].active)
                    bullets.erase(bullets.begin() + i);
                else
                    i++;
            }

            for (Enemy& e : enemies)
                if (e.health <= 0) e.active = false;

            bool anyActive = false;
            for (Enemy& e : enemies)
            {
                if (e.active)
                {
                    anyActive = true;
                    break;
                }
            }
            if (!anyActive && enemiesSpawned >= lvl.enemyCount)
            {
                levelComplete = true;
            }

            if (playerLost)
                state = GAMEOVER;
            else if (levelComplete)
            {
                currentLevel++;
                if (currentLevel > maxLevels)
                    state = WIN;
                else
                {
                    // Next level setup
                    turrets.clear();
                    for (int r = 0; r < TILE_COUNT; r++)
                        for (int c = 0; c < TILE_COUNT; c++)
                            if (tiles[r][c] == TURRET) tiles[r][c] = GRASS;
                    levelComplete = false;
                    state = BUILD;
                }
            }
        }

        else if (state == GAMEOVER)
        {
            if (IsKeyPressed(KEY_R))
            {
                enemies.clear();
                turrets.clear();
                bullets.clear();
                playerLost = false;
                for (int r = 0; r < TILE_COUNT; r++)
                    for (int c = 0; c < TILE_COUNT; c++)
                        if (tiles[r][c] == TURRET) tiles[r][c] = GRASS;
                state = BUILD;
            }
            if (IsKeyPressed(KEY_ENTER))
            {
                currentLevel = 1;
                enemies.clear();
                turrets.clear();
                bullets.clear();
                playerLost = false;
                for (int r = 0; r < TILE_COUNT; r++)
                    for (int c = 0; c < TILE_COUNT; c++)
                        if (tiles[r][c] == TURRET) tiles[r][c] = GRASS;
                state = BUILD;
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);

        for (int row = 0; row < TILE_COUNT; row++)
            for (int col = 0; col < TILE_COUNT; col++)
                DrawTile(row, col, tiles[row][col]);

        for (Bullet& b : bullets)
            if (b.active)
                DrawCircleV(b.position, 5, RED);
        for (Enemy& e : enemies)
            if (e.active) DrawCircleV(e.position, 10, GOLD);

        for (Turret& t : turrets)
        {
            DrawCircleV(t.position, 12, BLUE);
            DrawCircleLines(t.position.x, t.position.y, t.range, Fade(BLUE, 0.2f));
        }

        DrawText(TextFormat("Level: %i", currentLevel), 10, 10, 20, WHITE);

        if (state == BUILD)
            DrawText("BUILD MODE: Place 5 turrets to start. Left-click = Place, Right-click = Remove", 10, 40, 18, YELLOW);
        else if (state == GAMEOVER)
            DrawText("GAME OVER! Press R to Replay or ENTER to Restart", 200, 400, 24, RED);
        else if (state == WIN)
            DrawText("YOU WIN! All 3 levels completed!", 250, 400, 28, GREEN);

        EndDrawing();
    }

    UnloadSound(sTurretPlace);
    UnloadSound(sTurretBreak);
    UnloadSound(sTurretFire);
    UnloadSound(sEnemyHit);
    UnloadSound(sEnemyDeath);

    CloseWindow();
    return 0;
}
