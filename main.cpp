#include <Windows.h>
#include <vector>
#include <queue>
#include <cmath>
#include <iostream>
#include <cstdio>

HINSTANCE g_hInst = nullptr;
static bool g_Running = true;

struct Point {
    int x, y;
};

int min_width = 0;
int max_width = 5000;
int min_height = 0;
int max_height = 5000;

int health_bar_offset = 55;

void CreateConsole()
{
    AllocConsole();
    SetConsoleTitle(L"Aimbot Log");
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    std::cout << "[INFO] Console created.\n";
}

unsigned char* CaptureScreen(int& width, int& height)
{
    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);

    HDC hScreenDC = GetDC(nullptr);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    SelectObject(hMemDC, hBitmap);

    BitBlt(hMemDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    int stride = width * 3;
    unsigned char* pixels = new unsigned char[stride * height];

    if (GetDIBits(hMemDC, hBitmap, 0, height, pixels, &bmi, DIB_RGB_COLORS) == 0) {
        std::cout << "[ERROR] GetDIBits failed.\n";
        delete[] pixels;
        pixels = nullptr;
    }

    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreenDC);

    return pixels;
}

bool IsEnemyPixel(unsigned char R, unsigned char G, unsigned char B)
{
    int targetR = 242;
    int targetG = 73;
    int targetB = 73;

    int tolerance = 0;

    int dR = abs((int)R - targetR);
    int dG = abs((int)G - targetG);
    int dB = abs((int)B - targetB);

    return (dR <= tolerance && dG <= tolerance && dB <= tolerance);
}

void BFSFindCluster(const std::vector<unsigned char>& mask, int width, int height, int startX, int startY,
    std::vector<bool>& visited, int& minX, int& maxX, int& minY, int& maxY)
{
    std::queue<Point> q;
    q.push({ startX, startY });
    visited[startY * width + startX] = true;

    minX = maxX = startX;
    minY = maxY = startY;

    int dirs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };

    while (!q.empty()) {
        Point p = q.front();
        q.pop();

        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;

        for (auto& d : dirs) {
            int nx = p.x + d[0];
            int ny = p.y + d[1];
            if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                continue;
            int idx = ny * width + nx;
            if (!visited[idx] && mask[idx] == 1) {
                visited[idx] = true;
                q.push({ nx, ny });
            }
        }
    }
}

void DetachDLL()
{
    std::cout << "[INFO] Detaching DLL...\n";
    g_Running = false;
    Sleep(100);
    FreeLibraryAndExitThread(g_hInst, 0);
}

void AimbotLoop()
{
    std::cout << "[INFO] Aimbot loop started.\n";

    while (g_Running)
    {
        if (GetAsyncKeyState(VK_DELETE) & 0x8000) {
            DetachDLL();
            break;
        }

        if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000)
        {
            std::cout << "[DEBUG] XBUTTON1 pressed, capturing screen...\n";
            int screenWidth, screenHeight;
            unsigned char* pixels = CaptureScreen(screenWidth, screenHeight);
            if (!pixels)
            {
                std::cout << "[WARN] Failed to capture screen.\n";
                Sleep(10);
                continue;
            }

            int stride = screenWidth * 3;
            std::vector<unsigned char> mask(screenWidth * screenHeight, 0);

            int centerX = screenWidth / 2;
            int centerY = screenHeight / 2;

            std::cout << "[DEBUG] Sample pixels around center:\n";
            for (int i = -5; i <= 5; i++) {
                int dbgX = centerX + i;
                int dbgY = centerY;
                if (dbgX < 0 || dbgX >= screenWidth) continue;
                unsigned char Bp = pixels[dbgY * stride + dbgX * 3 + 0];
                unsigned char Gp = pixels[dbgY * stride + dbgX * 3 + 1];
                unsigned char Rp = pixels[dbgY * stride + dbgX * 3 + 2];
                std::cout << "Pixel(" << dbgX << "," << dbgY << "): R=" << (int)Rp << " G=" << (int)Gp << " B=" << (int)Bp << "\n";
            }

            for (int y = 0; y < screenHeight; y++) {
                for (int x = 0; x < screenWidth; x++) {
                    unsigned char B = pixels[y * stride + x * 3 + 0];
                    unsigned char G = pixels[y * stride + x * 3 + 1];
                    unsigned char R = pixels[y * stride + x * 3 + 2];

                    if (IsEnemyPixel(R, G, B)) {
                        mask[y * screenWidth + x] = 1;
                    }
                }
            }

            int maskCount = 0;
            for (auto m : mask) {
                if (m == 1) maskCount++;
            }
            std::cout << "[DEBUG] maskCount = " << maskCount << "\n";

            std::vector<bool> visited(screenWidth * screenHeight, false);

            double minDist = DBL_MAX;
            POINT bestTarget = { -1, -1 };
            int clusterCount = 0;


            if (maskCount > 0) {
                for (int y = 0; y < screenHeight; y++) {
                    for (int x = 0; x < screenWidth; x++) {
                        int idx = y * screenWidth + x;
                        if (mask[idx] == 1 && !visited[idx]) {
                            int minX, maxX, minY, maxY;
                            BFSFindCluster(mask, screenWidth, screenHeight, x, y, visited, minX, maxX, minY, maxY);

                            int w = (maxX - minX + 1);
                            int h = (maxY - minY + 1);

                            if (w < min_width || w > max_width || h < min_height || h > max_height) {
                                continue;
                            }

                            clusterCount++;
                            int clusterCenterX = (minX + maxX) / 2;
                            int clusterCenterY = (minY + maxY) / 2;

                            int targetX = clusterCenterX;
                            int targetY = clusterCenterY + health_bar_offset;

                            double dist = sqrt((double)(clusterCenterX - centerX) * (clusterCenterX - centerX) +
                                (double)(clusterCenterY - centerY) * (clusterCenterY - centerY));

                            if (dist < minDist) {
                                minDist = dist;
                                bestTarget.x = targetX;
                                bestTarget.y = targetY;
                            }
                        }
                    }
                }
            }

            delete[] pixels;

            std::cout << "[DEBUG] Found " << clusterCount << " clusters.\n";
            if (bestTarget.x == -1) {
                std::cout << "No valid target found.\n";
            }
            else {
                std::cout << "Best target at (" << bestTarget.x << ", " << bestTarget.y << ")\n";
                SetCursorPos(bestTarget.x, bestTarget.y);
            }
        }

    }
    std::cout << "[INFO] Aimbot loop ended.\n";
}

DWORD WINAPI MainThread(LPVOID)
{
    CreateConsole();
    std::cout << "[INFO] DLL injected and main thread started.\n";
    AimbotLoop();
    std::cout << "[INFO] Main thread exiting.\n";
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInst = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        std::cout << "[INFO] DLL detached.\n";
        break;
    }
    return TRUE;
}
