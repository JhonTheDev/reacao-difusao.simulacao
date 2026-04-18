#include "raylib.h"
#include <stdbool.h>
#include <stdio.h>

#define GRID_W 160
#define GRID_H 120
#define CELL_SIZE 5

#define DIFF_A 1.0f
#define DIFF_B 0.5f
#define DT 1.0f

#define DRAW_STEP_3D 2
#define WORLD_SCALE 0.08f

static bool view3D = false;

static float feed = 0.037f;
static float kill = 0.060f;
static int stepsPerFrame = 8;

static float A[GRID_W * GRID_H];
static float B[GRID_W * GRID_H];
static float nextA[GRID_W * GRID_H];
static float nextB[GRID_W * GRID_H];

static void applyPreset(int preset) {
    switch (preset) {
        case 1: // mais manchas
            feed = 0.054f;
            kill = 0.062f;
            break;
        case 2: // mais estruturas organicas
            feed = 0.037f;
            kill = 0.060f;
            break;
        case 3: // formas tipo verme
            feed = 0.029f;
            kill = 0.057f;
            break;
        case 4: //mais compactos
            feed = 0.022f;
            kill = 0.051f;
            break;
    }
}

 // Mapreando X e Y pra um vetor linear
static int idx(int x, int y) {
    return y * GRID_W + x;
}

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static float laplacian(const float *grid, int x, int y) {
    float center = grid[idx(x, y)];
    float sum = 0.0f;

    sum += grid[idx(x - 1, y)] * 0.20f;
    sum += grid[idx(x + 1, y)] * 0.20f;
    sum += grid[idx(x, y - 1)] * 0.20f;
    sum += grid[idx(x, y + 1)] * 0.20f;

    sum += grid[idx(x - 1, y - 1)] * 0.05f;
    sum += grid[idx(x + 1, y - 1)] * 0.05f;
    sum += grid[idx(x - 1, y + 1)] * 0.05f;
    sum += grid[idx(x + 1, y + 1)] * 0.05f;

    sum -= center;
    return sum;
}

static void initGrid(void) {
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            int i = idx(x, y); //Reagente
            A[i] = 1.0f;
            B[i] = 0.0f;
            nextA[i] = 1.0f;
            nextB[i] = 0.0f;
        }
    }

    // Semente central para disparar o padrão
    int cx = GRID_W / 2;
    int cy = GRID_H / 2;
    for (int y = cy - 8; y <= cy + 8; y++) {
        for (int x = cx - 8; x <= cx + 8; x++) {
            int i = idx(x, y);
            A[i] = 0.0f;
            B[i] = 1.0f;
        }
    }
}

static void stepGrayScott(void) {
    for (int y = 1; y < GRID_H - 1; y++) {
        for (int x = 1; x < GRID_W - 1; x++) {
            int i = idx(x, y);

            float a = A[i];
            float b = B[i];
            float lapA = laplacian(A, x, y);
            float lapB = laplacian(B, x, y);
            float reaction = a * b * b;

            float aNew = a + (DIFF_A * lapA - reaction + feed * (1.0f - a)) * DT;
            float bNew = b + (DIFF_B * lapB + reaction - (kill + feed) * b) * DT;

            nextA[i] = clamp01(aNew);
            nextB[i] = clamp01(bNew);
        }
    }

    // bordas simples: mantem estado atual
    for (int x = 0; x < GRID_W; x++) {
        nextA[idx(x, 0)] = A[idx(x, 0)];
        nextB[idx(x, 0)] = B[idx(x, 0)];
        nextA[idx(x, GRID_H - 1)] = A[idx(x, GRID_H - 1)];
        nextB[idx(x, GRID_H - 1)] = B[idx(x, GRID_H - 1)];
    }
    for (int y = 0; y < GRID_H; y++) {
        nextA[idx(0, y)] = A[idx(0, y)];
        nextB[idx(0, y)] = B[idx(0, y)];
        nextA[idx(GRID_W - 1, y)] = A[idx(GRID_W - 1, y)];
        nextB[idx(GRID_W - 1, y)] = B[idx(GRID_W - 1, y)];
    }

    for (int i = 0; i < GRID_W * GRID_H; i++) {
        A[i] = nextA[i];
        B[i] = nextB[i];
    }
}

static void drawGrid(void) {
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            float b = B[idx(x, y)];

            // paleta amber/dark
            unsigned char r = (unsigned char)(20 + b * 225.0f);
            unsigned char g = (unsigned char)(18 + b * 170.0f);
            unsigned char bl = (unsigned char)(22 + b * 70.0f);

            DrawRectangle(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE, (Color){r, g, bl, 255});
        }
    }
}

static void drawGrid3D(Camera3D *camera) {
    BeginMode3D(*camera);

    for (int y = 0; y < GRID_H; y += DRAW_STEP_3D) {
        for (int x = 0; x < GRID_W; x += DRAW_STEP_3D) {
            float b = B[idx(x, y)];

            float size = WORLD_SCALE * DRAW_STEP_3D * 0.92f;
            float h = 0.04f + b * 1.8f;

            float wx = (x - GRID_W * 0.5f) * WORLD_SCALE;
            float wz = (y - GRID_H * 0.5f) * WORLD_SCALE;

            Vector3 pos = { wx, h * 0.5f, wz };
            Vector3 cubeSize = { size, h, size };

            unsigned char r = (unsigned char)(25 + b * 220.0f);
            unsigned char g = (unsigned char)(18 + b * 170.0f);
            unsigned char bl = (unsigned char)(28 + b * 90.0f);

            Color c = (Color){ r, g, bl, 255 };

            DrawCubeV(pos, cubeSize, c);
        }
    }

    DrawGrid(30, 0.5f);
    EndMode3D();
}

 // Inputs/Controles
static void handleInput(bool *paused) {
    if (IsKeyPressed(KEY_SPACE)) *paused = !*paused;
    if (IsKeyPressed(KEY_R)) initGrid();

    if (IsKeyPressed(KEY_UP)) feed += 0.001f;
    if (IsKeyPressed(KEY_DOWN)) feed -= 0.001f;

    if (IsKeyPressed(KEY_RIGHT)) kill += 0.001f;
    if (IsKeyPressed(KEY_LEFT)) kill -= 0.001f;

    if (IsKeyPressed(KEY_Z) && stepsPerFrame > 1) stepsPerFrame--;
    if (IsKeyPressed(KEY_X) && stepsPerFrame < 32) stepsPerFrame++;

    if (IsKeyPressed(KEY_ONE)) applyPreset(1);
    if (IsKeyPressed(KEY_TWO)) applyPreset(2);
    if (IsKeyPressed(KEY_THREE)) applyPreset(3);
    if (IsKeyPressed(KEY_FOUR)) applyPreset(4);

    if (IsKeyPressed(KEY_V)) view3D = !view3D;
    if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

    feed = clamp01(feed);
    kill = clamp01(kill);
}

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(GRID_W * CELL_SIZE, GRID_H * CELL_SIZE, "Reacao-Difusao Gray-Scott");
    SetTargetFPS(60);

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 10.0f, 12.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    bool paused = false;
    initGrid();

    while (!WindowShouldClose()) {
        handleInput(&paused);

        if (!paused) {
            for (int i = 0; i < stepsPerFrame; i++) {
                stepGrayScott();
            }
        }

        BeginDrawing();
        ClearBackground((Color){8, 10, 14, 255});

        if (view3D) {
            drawGrid3D(&camera);
        } else {
            drawGrid();
        }

        DrawRectangle(10, 10, 530, 92, (Color){10, 12, 16, 210});
        DrawRectangleLines(10, 10, 530, 92, (Color){200, 160, 90, 180});

        DrawText(paused ? "Status: PAUSADO" : "Status: RODANDO", 20, 18, 20, RAYWHITE);

        char line1[140];
        snprintf(line1, sizeof(line1), "Feed: %.3f  Kill: %.3f  Steps/frame: %d", feed, kill, stepsPerFrame);
        DrawText(line1, 20, 44, 20, (Color){230, 210, 180, 255});

        DrawText("V alterna 2D/3D | F11 tela cheia | SPACE pause | R reset", 20, 72, 16, (Color){190, 190, 200, 255});

        if (view3D) {
            UpdateCamera(&camera, CAMERA_ORBITAL);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}