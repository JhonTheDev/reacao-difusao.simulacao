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

typedef enum AppScreen {
    SCREEN_MENU = 0,
    SCREEN_SIMULATION
} AppScreen;

typedef struct ResolutionOption {
    const char *label;
    int width;
    int height;
} ResolutionOption;

typedef struct FpsOption {
    const char *label;
    int fps;
    int stepsPerFrame;
} FpsOption;

static const ResolutionOption resolutionOptions[] = {
    {"800x600", 800, 600},
    {"1024x768", 1024, 768},
    {"1280x960", 1280, 960},
    {"1440x1080", 1440, 1080}
};

static const FpsOption fpsOptions[] = {
    {"10 FPS", 10, 24},
    {"30 FPS", 30, 8},
    {"60 FPS", 60, 4}
};

static const char *presetNames[] = {
    "Preset 1 - manchas",
    "Preset 2 - orgânico",
    "Preset 3 - vermes",
    "Preset 4 - compacto"
};

static AppScreen currentScreen = SCREEN_MENU;
static bool view3D = false;
static bool paused = false;

static int selectedPreset = 1;
static int selectedResolution = 1;
static int selectedFps = 2;
static int stepsPerFrame = 4;

static float feed = 0.037f;
static float kill = 0.060f;

static float A[GRID_W * GRID_H];
static float B[GRID_W * GRID_H];
static float nextA[GRID_W * GRID_H];
static float nextB[GRID_W * GRID_H];

static int idx(int x, int y) {
    return y * GRID_W + x;
}

static float clamp01(float v) {
    if (v < 0.0f) {
        return 0.0f;
    }

    if (v > 1.0f) {
        return 1.0f;
    }

    return v;
}

static int getCellSize(void) {
    int cellWidth = GetScreenWidth() / GRID_W;
    int cellHeight = GetScreenHeight() / GRID_H;
    int cellSize = cellWidth < cellHeight ? cellWidth : cellHeight;

    if (cellSize < 1) {
        cellSize = 1;
    }

    return cellSize;
}

static void getGridOffset(int cellSize, int *offsetX, int *offsetY) {
    int gridPixelWidth = GRID_W * cellSize;
    int gridPixelHeight = GRID_H * cellSize;

    *offsetX = (GetScreenWidth() - gridPixelWidth) / 2;
    *offsetY = (GetScreenHeight() - gridPixelHeight) / 2;
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

static int worldToGridX(int screenX, int cellSize, int offsetX) {
    return (screenX - offsetX) / cellSize;
}

static int worldToGridY(int screenY, int cellSize, int offsetY) {
    return (screenY - offsetY) / cellSize;
}

static bool isInsideGrid(int x, int y) {
    return x >= 0 && x < GRID_W && y >= 0 && y < GRID_H;
}

static void paintCell(int cx, int cy, int radius, float aValue, float bValue) {
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            if (!isInsideGrid(x, y)) {
                continue;
            }

            int i = idx(x, y);
            A[i] = aValue;
            B[i] = bValue;
            nextA[i] = aValue;
            nextB[i] = bValue;
        }
    }
}

static void applyPreset(int preset) {
    selectedPreset = preset;

    switch (preset) {
        case 1:
            feed = 0.054f;
            kill = 0.062f;
            break;
        case 2:
            feed = 0.037f;
            kill = 0.060f;
            break;
        case 3:
            feed = 0.029f;
            kill = 0.057f;
            break;
        case 4:
            feed = 0.022f;
            kill = 0.051f;
            break;
    }
}

static void applyFpsChoice(int index) {
    if (index < 0 || index >= (int)(sizeof(fpsOptions) / sizeof(fpsOptions[0]))) {
        return;
    }

    selectedFps = index;
    SetTargetFPS(fpsOptions[index].fps);
    stepsPerFrame = fpsOptions[index].stepsPerFrame;
}

static void applyResolutionChoice(int index) {
    if (index < 0 || index >= (int)(sizeof(resolutionOptions) / sizeof(resolutionOptions[0]))) {
        return;
    }

    selectedResolution = index;
    SetWindowSize(resolutionOptions[index].width, resolutionOptions[index].height);
}

static void initGrid(void) {
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            int i = idx(x, y);
            A[i] = 1.0f;
            B[i] = 0.0f;
            nextA[i] = 1.0f;
            nextB[i] = 0.0f;
        }
    }

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
    int cellSize = getCellSize();
    int offsetX = 0;
    int offsetY = 0;
    getGridOffset(cellSize, &offsetX, &offsetY);

    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            float b = B[idx(x, y)];

            unsigned char r = (unsigned char)(20 + b * 225.0f);
            unsigned char g = (unsigned char)(18 + b * 170.0f);
            unsigned char bl = (unsigned char)(22 + b * 70.0f);

            DrawRectangle(offsetX + x * cellSize, offsetY + y * cellSize, cellSize, cellSize, (Color){r, g, bl, 255});
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

static bool drawButton(Rectangle rect, const char *text, bool active) {
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, rect);

    Color background = active ? (Color){200, 160, 90, 255} : (hover ? (Color){55, 60, 72, 255} : (Color){28, 32, 40, 255});
    Color border = active ? (Color){245, 226, 192, 255} : (hover ? (Color){200, 160, 90, 255} : (Color){85, 90, 100, 255});
    Color textColor = active ? (Color){20, 18, 16, 255} : RAYWHITE;

    DrawRectangleRounded(rect, 0.18f, 8, background);
    DrawRectangleRoundedLines(rect, 0.18f, 8, border);

    int fontSize = 18;
    int textWidth = MeasureText(text, fontSize);
    int textX = (int)(rect.x + (rect.width - textWidth) * 0.5f);
    int textY = (int)(rect.y + (rect.height - fontSize) * 0.5f - 1);

    DrawText(text, textX, textY, fontSize, textColor);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void drawPanel(Rectangle rect, const char *title) {
    DrawRectangleRounded(rect, 0.12f, 8, (Color){10, 12, 16, 220});
    DrawRectangleRoundedLines(rect, 0.12f, 8, (Color){200, 160, 90, 180});
    DrawText(title, (int)rect.x + 18, (int)rect.y + 14, 22, (Color){245, 226, 192, 255});
}

static void startSimulation(void) {
    initGrid();
    paused = false;
    currentScreen = SCREEN_SIMULATION;
}

static void drawMenu(void) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    ClearBackground((Color){8, 10, 14, 255});

    DrawText("Reaction-Diffusion Sandbox", 32, 24, 30, (Color){245, 226, 192, 255});
    DrawText("Escolha presets, FPS, resolucao e o modo de visualizacao antes de iniciar.", 32, 58, 18, (Color){200, 200, 210, 255});

    Rectangle presetPanel = { 28, 98, (float)(sw - 56) * 0.32f, 300 };
    Rectangle fpsPanel = { 28 + presetPanel.width + 16, 98, (float)(sw - 56) * 0.28f, 300 };
    Rectangle resPanel = { fpsPanel.x + fpsPanel.width + 16, 98, sw - (fpsPanel.x + fpsPanel.width + 44), 300 };

    drawPanel(presetPanel, "Presets");
    drawPanel(fpsPanel, "FPS / modo");
    drawPanel(resPanel, "Resolucao");

    float presetX = presetPanel.x + 16;
    float presetY = presetPanel.y + 56;
    float presetW = presetPanel.width - 32;
    float presetH = 44;
    float presetGap = 10;

    for (int i = 0; i < 4; i++) {
        Rectangle button = { presetX, presetY + i * (presetH + presetGap), presetW, presetH };
        if (drawButton(button, presetNames[i], selectedPreset == i + 1)) {
            applyPreset(i + 1);
        }
    }

    float fpsX = fpsPanel.x + 16;
    float fpsY = fpsPanel.y + 56;
    float fpsW = fpsPanel.width - 32;
    float fpsH = 44;
    float fpsGap = 10;

    for (int i = 0; i < 3; i++) {
        Rectangle button = { fpsX, fpsY + i * (fpsH + fpsGap), fpsW, fpsH };
        if (drawButton(button, fpsOptions[i].label, selectedFps == i)) {
            applyFpsChoice(i);
        }
    }

    Rectangle modeButton = { fpsX, fpsPanel.y + 224, fpsW, 44 };
    if (drawButton(modeButton, view3D ? "Modo 3D ativo" : "Modo 2D ativo", view3D)) {
        view3D = !view3D;
    }

    float resX = resPanel.x + 16;
    float resY = resPanel.y + 56;
    float resW = resPanel.width - 32;
    float resH = 44;
    float resGap = 10;

    for (int i = 0; i < 4; i++) {
        Rectangle button = { resX, resY + i * (resH + resGap), resW, resH };
        if (drawButton(button, resolutionOptions[i].label, selectedResolution == i)) {
            applyResolutionChoice(i);
        }
    }

    Rectangle startButton = { 32, sh - 86, sw - 64, 54 };
    if (drawButton(startButton, "Iniciar sandbox", true)) {
        startSimulation();
    }

    char summary[256];
    snprintf(summary, sizeof(summary), "Preset: %d | Feed: %.3f | Kill: %.3f | FPS: %d | Res: %s | Modo: %s",
        selectedPreset,
        feed,
        kill,
        fpsOptions[selectedFps].fps,
        resolutionOptions[selectedResolution].label,
        view3D ? "3D" : "2D");
    DrawText(summary, 32, sh - 128, 18, (Color){230, 210, 180, 255});
}

static void drawSimulationHud(void) {
    DrawRectangle(10, 10, 690, 110, (Color){10, 12, 16, 210});
    DrawRectangleLines(10, 10, 690, 110, (Color){200, 160, 90, 180});

    DrawText(paused ? "Status: PAUSADO" : "Status: RODANDO", 20, 18, 20, RAYWHITE);

    char line1[160];
    snprintf(line1, sizeof(line1), "Feed: %.3f  Kill: %.3f  Steps/frame: %d  FPS alvo: %d", feed, kill, stepsPerFrame, fpsOptions[selectedFps].fps);
    DrawText(line1, 20, 44, 20, (Color){230, 210, 180, 255});

    char line2[220];
    snprintf(line2, sizeof(line2), "Preset: %d  Res: %s  Modo: %s", selectedPreset, resolutionOptions[selectedResolution].label, view3D ? "3D" : "2D");
    DrawText(line2, 20, 70, 18, (Color){190, 190, 200, 255});

    DrawText("Botões: Menu | Pause | Reset | Presets | FPS | Resolucoes | 2D/3D", 20, 94, 16, (Color){190, 190, 200, 255});

    Rectangle menuButton = { 720, 18, 130, 34 };
    Rectangle pauseButton = { 720, 60, 130, 34 };
    Rectangle resetButton = { 720, 102, 130, 34 };
    Rectangle viewButton = { 720, 144, 130, 34 };

    if (drawButton(menuButton, "Menu", false)) {
        currentScreen = SCREEN_MENU;
        paused = true;
    }

    if (drawButton(pauseButton, paused ? "Continuar" : "Pausar", false)) {
        paused = !paused;
    }

    if (drawButton(resetButton, "Reset", false)) {
        initGrid();
    }

    if (drawButton(viewButton, view3D ? "Ir 2D" : "Ir 3D", false)) {
        view3D = !view3D;
    }
}

static void handleSimulationHotkeys(void) {
    if (IsKeyPressed(KEY_SPACE)) {
        paused = !paused;
    }

    if (IsKeyPressed(KEY_R)) {
        initGrid();
    }

    if (IsKeyPressed(KEY_ONE)) {
        applyPreset(1);
    }

    if (IsKeyPressed(KEY_TWO)) {
        applyPreset(2);
    }

    if (IsKeyPressed(KEY_THREE)) {
        applyPreset(3);
    }

    if (IsKeyPressed(KEY_FOUR)) {
        applyPreset(4);
    }

    if (IsKeyPressed(KEY_V)) {
        view3D = !view3D;
    }

    if (IsKeyPressed(KEY_UP)) {
        feed += 0.001f;
    }

    if (IsKeyPressed(KEY_DOWN)) {
        feed -= 0.001f;
    }

    if (IsKeyPressed(KEY_RIGHT)) {
        kill += 0.001f;
    }

    if (IsKeyPressed(KEY_LEFT)) {
        kill -= 0.001f;
    }

    if (IsKeyPressed(KEY_Z) && stepsPerFrame > 1) {
        stepsPerFrame--;
    }

    if (IsKeyPressed(KEY_X) && stepsPerFrame < 32) {
        stepsPerFrame++;
    }

    if (IsKeyPressed(KEY_F11)) {
        ToggleFullscreen();
    }

    feed = clamp01(feed);
    kill = clamp01(kill);
}

static void handleMousePainting(void) {
    if (view3D || currentScreen != SCREEN_SIMULATION) {
        return;
    }

    int cellSize = getCellSize();
    int offsetX = 0;
    int offsetY = 0;
    getGridOffset(cellSize, &offsetX, &offsetY);

    Vector2 mouse = GetMousePosition();
    int gx = worldToGridX((int)mouse.x, cellSize, offsetX);
    int gy = worldToGridY((int)mouse.y, cellSize, offsetY);

    if (!isInsideGrid(gx, gy)) {
        return;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        paintCell(gx, gy, 2, 0.0f, 1.0f);
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        paintCell(gx, gy, 2, 1.0f, 0.0f);
    }
}

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(resolutionOptions[selectedResolution].width, resolutionOptions[selectedResolution].height, "Reacao-Difusao Gray-Scott");
    SetTargetFPS(fpsOptions[selectedFps].fps);
    applyPreset(selectedPreset);
    stepsPerFrame = fpsOptions[selectedFps].stepsPerFrame;
    initGrid();

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 10.0f, 12.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose()) {
        feed = clamp01(feed);
        kill = clamp01(kill);

        if (currentScreen == SCREEN_SIMULATION) {
            handleSimulationHotkeys();
            handleMousePainting();

            if (!paused) {
                for (int i = 0; i < stepsPerFrame; i++) {
                    stepGrayScott();
                }
            }
        }

        BeginDrawing();

        if (currentScreen == SCREEN_MENU) {
            drawMenu();
        } else {
            ClearBackground((Color){8, 10, 14, 255});

            if (view3D) {
                UpdateCamera(&camera, CAMERA_ORBITAL);
                drawGrid3D(&camera);
            } else {
                drawGrid();
            }

            drawSimulationHud();
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
