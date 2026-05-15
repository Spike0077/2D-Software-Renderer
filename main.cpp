#define _USE_MATH_DEFINES
/**
 * 项目名称：2D软件渲染器 - 旋转渐变三角形Demo
 * 作者：Spike
 * 完成时间：2026年5月15日
 * 技术栈：C++11, SDL2
 * 核心功能：
 * 1. 基于扫描线算法的通用任意三角形填充
 * 2. Gouraud着色（逐像素颜色插值）
 * 3. 与帧率无关的实时动画系统
 * 4. 鼠标跟随交互
 * 5. 呼吸缩放和动态颜色渐变效果
 * 
 * 坐标系：SDL标准坐标系（原点在左上角，y轴向下递增）
 */
#include <SDL2/SDL.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <ctime>
using namespace std;

const int WIDTH = 800;
const int HEIGHT = 600;

float centerX = WIDTH / 2.0f;
float centerY = HEIGHT / 2.0f;

/**
 * 通用线性插值函数
 * @tparam T 插值类型（支持int, float, unsigned char等）
 * @param a 起始值
 * @param b 结束值
 * @param t 插值比例（0~1）
 * @return 插值结果
 */
template<typename T>
T lerp(T a, T b, float t) {
    return static_cast<T>(a + (b - a) * t);
}

/**
 * 生成随时间变化的动态颜色分量（0~255）
 * @param time 当前时间（秒）
 * @param offset 相位偏移（弧度），用于产生不同颜色的变化节奏
 * @return 颜色分量值（0~255）
 */
unsigned char dynamicColor(float time, float offset) {
    float value = 127.5f + 127.5f * sin(time * 2.0f + offset);
    return (unsigned char)value;
}

/**
 * 顶点旋转函数（SDL坐标系专属，顺时针为正）
 * @param x 输入输出：顶点x坐标
 * @param y 输入输出：顶点y坐标
 * @param cx 旋转中心x坐标
 * @param cy 旋转中心y坐标
 * @param angle 旋转角度（弧度）
 */
void rotatePoint(float& x, float& y, float cx, float cy, float angle) {
    float dx = x - cx;
    float dy = y - cy;
    
    float cosA = cos(angle);
    float sinA = sin(angle);
    
    float newDx = dx * cosA - dy * sinA;
    float newDy = dx * sinA + dy * cosA;
    
    x = cx + newDx;
    y = cy + newDy;
}

/**
 * 绘制单个像素点
 * @param renderer SDL渲染器指针
 * @param x 像素x坐标
 * @param y 像素y坐标
 * @param r 红色分量（0~255）
 * @param g 绿色分量（0~255）
 * @param b 蓝色分量（0~255）
 * @param a 透明度分量（0~255，默认255不透明）
 */
void drawPoint(SDL_Renderer* renderer, int x, int y, int r, int g, int b, int a = 255) {
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderDrawPoint(renderer, x, y);
}

/**
 * 使用Bresenham算法绘制任意斜率的直线
 * @param renderer SDL渲染器指针
 * @param x0 起点x坐标
 * @param y0 起点y坐标
 * @param x1 终点x坐标
 * @param y1 终点y坐标
 * @param r 红色分量
 * @param g 绿色分量
 * @param b 蓝色分量
 */
void drawLine(SDL_Renderer* renderer, int x0, int y0, int x1, int y1, int r, int g, int b) {
    int stepX = (x0 < x1) ? 1 : -1;
    int stepY = (y0 < y1) ? 1 : -1;
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int error = dx - dy;
    
    while (true) {
        drawPoint(renderer, x0, y0, r, g, b);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int error2 = error * 2;
        if (error2 > -dy) {
            error -= dy;
            x0 += stepX;
        }
        if (error2 < dx) {
            error += dx;
            y0 += stepY;
        }
    }
}

/**
 * 绘制水平直线（用于三角形填充，性能优化）
 * @param renderer SDL渲染器指针
 * @param y 直线y坐标
 * @param x0 起点x坐标
 * @param x1 终点x坐标
 * @param r 红色分量
 * @param g 绿色分量
 * @param b 蓝色分量
 */
void drawHorizontalLine(SDL_Renderer* renderer, int y, int x0, int x1, int r, int g, int b) {
    if (x0 > x1) {
        swap(x0, x1);
    }
    for (int x = x0; x <= x1; x++) {
        drawPoint(renderer, x, y, r, g, b);
    }
}

/**
 * 绘制带Gouraud着色的平顶渐变三角形
 * @param renderer SDL渲染器指针
 * @param x0,y0,r0,g0,b0 第一个顶点坐标和颜色
 * @param x1,y1,r1,g1,b1 第二个顶点坐标和颜色（与第一个顶点同高）
 * @param x2,y2,r2,g2,b2 第三个顶点坐标和颜色（底部顶点）
 */
void drawFlatTopTriangleColored(SDL_Renderer* renderer,
                                int x0, int y0, unsigned char r0, unsigned char g0, unsigned char b0,
                                int x1, int y1, unsigned char r1, unsigned char g1, unsigned char b1,
                                int x2, int y2, unsigned char r2, unsigned char g2, unsigned char b2) {
    float invHeight = 1.0f / (y2 - y0);
    
    for (int y = y0; y <= y2; y++) {
        float t = (y - y0) * invHeight;
        int xLeft = x0 + (int)(t * (x2 - x0));
        int xRight = x1 + (int)(t * (x2 - x1));
        
        if (xLeft > xRight) swap(xLeft, xRight);

        unsigned char rLeft = lerp(r0, r2, t);
        unsigned char gLeft = lerp(g0, g2, t);
        unsigned char bLeft = lerp(b0, b2, t);
        unsigned char rRight = lerp(r1, r2, t);
        unsigned char gRight = lerp(g1, g2, t);
        unsigned char bRight = lerp(b1, b2, t);

        int lineWidth = xRight - xLeft;
        if (lineWidth == 0) {
            drawPoint(renderer, xLeft, y, rLeft, gLeft, bLeft);
            continue;
        }
        float invLineWidth = 1.0f / lineWidth;
        for (int x = xLeft; x <= xRight; x++) {
            float tLine = (x - xLeft) * invLineWidth;
            unsigned char r = lerp(rLeft, rRight, tLine);
            unsigned char g = lerp(gLeft, gRight, tLine);
            unsigned char b = lerp(bLeft, bRight, tLine);
            drawPoint(renderer, x, y, r, g, b);
        }
    }
}

/**
 * 绘制带Gouraud着色的平底渐变三角形
 * @param renderer SDL渲染器指针
 * @param x0,y0,r0,g0,b0 顶部顶点坐标和颜色
 * @param x1,y1,r1,g1,b1 第一个底部顶点坐标和颜色
 * @param x2,y2,r2,g2,b2 第二个底部顶点坐标和颜色（与第一个底部顶点同高）
 */
void drawFlatBottomTriangleColored(SDL_Renderer* renderer,
                                   int x0, int y0, unsigned char r0, unsigned char g0, unsigned char b0,
                                   int x1, int y1, unsigned char r1, unsigned char g1, unsigned char b1,
                                   int x2, int y2, unsigned char r2, unsigned char g2, unsigned char b2) {
    float invHeight = 1.0f / (y1 - y0);
    
    for (int y = y0; y <= y1; y++) {
        float t = (y - y0) * invHeight;
        int xLeft = x0 + (int)(t * (x1 - x0));
        int xRight = x0 + (int)(t * (x2 - x0));
        
        if (xLeft > xRight) swap(xLeft, xRight);

        unsigned char rLeft = lerp(r0, r1, t);
        unsigned char gLeft = lerp(g0, g1, t);
        unsigned char bLeft = lerp(b0, b1, t);
        unsigned char rRight = lerp(r0, r2, t);
        unsigned char gRight = lerp(g0, g2, t);
        unsigned char bRight = lerp(b0, b2, t);

        int lineWidth = xRight - xLeft;
        if (lineWidth == 0) {
            drawPoint(renderer, xLeft, y, rLeft, gLeft, bLeft);
            continue;
        }
        float invLineWidth = 1.0f / lineWidth;
        for (int x = xLeft; x <= xRight; x++) {
            float tLine = (x - xLeft) * invLineWidth;
            unsigned char r = lerp(rLeft, rRight, tLine);
            unsigned char g = lerp(gLeft, gRight, tLine);
            unsigned char b = lerp(bLeft, bRight, tLine);
            drawPoint(renderer, x, y, r, g, b);
        }
    }
}

/**
 * 通用带Gouraud着色的任意三角形填充函数
 * 自动将任意三角形拆分为平顶+平底三角形，支持所有顶点顺序和边界情况
 * @param renderer SDL渲染器指针
 * @param x0,y0,r0,g0,b0 第一个顶点坐标和颜色
 * @param x1,y1,r1,g1,b1 第二个顶点坐标和颜色
 * @param x2,y2,r2,g2,b2 第三个顶点坐标和颜色
 */
void drawFilledTriangleColored(SDL_Renderer* renderer,
                               int x0, int y0, unsigned char r0, unsigned char g0, unsigned char b0,
                               int x1, int y1, unsigned char r1, unsigned char g1, unsigned char b1,
                               int x2, int y2, unsigned char r2, unsigned char g2, unsigned char b2) {

    // 按y坐标从小到大排序顶点
    if (y0 > y1) { swap(x0, x1); swap(y0, y1); swap(r0, r1); swap(g0, g1); swap(b0, b1); }
    if (y0 > y2) { swap(x0, x2); swap(y0, y2); swap(r0, r2); swap(g0, g2); swap(b0, b2); }
    if (y1 > y2) { swap(x1, x2); swap(y1, y2); swap(r1, r2); swap(g1, g2); swap(b1, b2); }

    if (y0 == y2) return; // 退化三角形，不绘制

    // 平顶三角形（两个顶点在顶部）
    if (y0 == y1) {
        drawFlatTopTriangleColored(renderer,
            x0, y0, r0, g0, b0,
            x1, y1, r1, g1, b1,
            x2, y2, r2, g2, b2);
        return;
    }

    // 平底三角形（两个顶点在底部）
    if (y1 == y2) {
        drawFlatBottomTriangleColored(renderer,
            x0, y0, r0, g0, b0,
            x1, y1, r1, g1, b1,
            x2, y2, r2, g2, b2);
        return;
    }

    // 一般三角形：拆分为平底+平顶三角形
    int xSplit = x0 + (int)((float)(y1 - y0) * (x2 - x0) / (y2 - y0));
    float tSplit = (float)(y1 - y0) / (y2 - y0);
    unsigned char rSplit = lerp(r0, r2, tSplit);
    unsigned char gSplit = lerp(g0, g2, tSplit);
    unsigned char bSplit = lerp(b0, b2, tSplit);

    drawFlatBottomTriangleColored(renderer,
        x0, y0, r0, g0, b0,        
        xSplit, y1, rSplit, gSplit, bSplit, 
        x1, y1, r1, g1, b1);       

    drawFlatTopTriangleColored(renderer,
        xSplit, y1, rSplit, gSplit, bSplit, 
        x1, y1, r1, g1, b1,       
        x2, y2, r2, g2, b2);      
}

int main(int argc, char* argv[]) {
    system("chcp 65001 > nul");
    int frameCount = 0;
    float lastFPSTime = 0.0f;
    int fps = 0;

    // SDL初始化
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        cout << "SDL初始化失败！错误：" << SDL_GetError() << endl;
        return -1;
    }
    cout << "SDL初始化成功！" << endl;
    
    // 创建窗口
    SDL_Window* window = SDL_CreateWindow(
        "2D软件渲染器 - 旋转渐变三角形",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        cout << "窗口创建失败！错误：" << SDL_GetError() << endl;
        SDL_Quit();
        return -1;
    }
    cout << "窗口创建成功！" << endl;

    // 创建渲染器
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED
    );
    if (!renderer) {
        cout << "渲染器创建失败！错误：" << SDL_GetError() << endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    cout << "渲染器创建成功！" << endl;

    // ---------------------- 三角形初始顶点定义 ----------------------
    // 大三角形（最外层）
    float size = 200.0f;
    float x0_1 = 0, y0_1 = -size;
    float x1_1 = -size * 0.866f, y1_1 = size * 0.5f;
    float x2_1 = size * 0.866f, y2_1 = size * 0.5f;

    // 中三角形（中间层）
    float size2 = 150.0f;
    float x0_2 = 0, y0_2 = -size2;
    float x1_2 = -size2 * 0.866f, y1_2 = size2 * 0.5f;
    float x2_2 = size2 * 0.866f, y2_2 = size2 * 0.5f;

    // 小三角形（最内层）
    float size3 = 100.0f;
    float x0_3 = 0, y0_3 = -size3;
    float x1_3 = -size3 * 0.866f, y1_3 = size3 * 0.5f;
    float x2_3 = size3 * 0.866f, y2_3 = size3 * 0.5f;

    // ---------------------- 实时动画主循环 ----------------------
    bool running = true;
    SDL_Event event;

    while (running) {
        // 1. 处理事件（窗口关闭+鼠标移动）
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            else if (event.type == SDL_MOUSEMOTION) {
                centerX = (float)event.motion.x;
                centerY = (float)event.motion.y;
            }
        }

        // 2. 清空屏幕（黑色背景）
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // 3. 计算当前时间（与帧率无关的动画核心）
        float time = (float)SDL_GetTicks() / 1000.0f;
        
        // 计算呼吸缩放系数：不同幅度和频率，产生层次感
        float scale = 1.0f + 0.2f * sin(time * 2.0f);      // 大三角形：幅度0.2，频率2Hz
        float scale2 = 1.0f + 0.15f * sin(time * 1.5f);   // 中三角形：幅度0.15，频率1.5Hz
        float scale3 = 1.0f + 0.1f * sin(time * 3.0f);    // 小三角形：幅度0.1，频率3Hz

        // 计算旋转角度：不同方向和周期，增强视觉效果
        float angle1 = time * 2.0f * (float)M_PI / 10.0f;  // 大三角形：10秒一圈，顺时针
        float angle2 = -time * 2.0f * (float)M_PI / 15.0f; // 中三角形：15秒一圈，逆时针
        float angle3 = time * 2.0f * (float)M_PI / 5.0f;   // 小三角形：5秒一圈，顺时针

        // ---------------------- 渲染大三角形 ----------------------
        float rx0_1 = x0_1 * scale, ry0_1 = y0_1 * scale;
        float rx1_1 = x1_1 * scale, ry1_1 = y1_1 * scale;
        float rx2_1 = x2_1 * scale, ry2_1 = y2_1 * scale;
        
        rotatePoint(rx0_1, ry0_1, 0, 0, angle1);
        rotatePoint(rx1_1, ry1_1, 0, 0, angle1);
        rotatePoint(rx2_1, ry2_1, 0, 0, angle1);
        
        // 平移到鼠标中心
        rx0_1 += centerX; ry0_1 += centerY;
        rx1_1 += centerX; ry1_1 += centerY;
        rx2_1 += centerX; ry2_1 += centerY;
        
        drawFilledTriangleColored(renderer,
           (int)rx0_1, (int)ry0_1, 
           dynamicColor(time, 0.0f),    // 红通道
           dynamicColor(time, 2.094f),  // 绿通道（偏移120度）
           dynamicColor(time, 4.188f),  // 蓝通道（偏移240度）
           (int)rx1_1, (int)ry1_1, 
           dynamicColor(time, 2.094f),
           dynamicColor(time, 4.188f),
           dynamicColor(time, 0.0f),
           (int)rx2_1, (int)ry2_1, 
           dynamicColor(time, 4.188f),
           dynamicColor(time, 0.0f),
           dynamicColor(time, 2.094f));

        // ---------------------- 渲染中三角形 ----------------------
        float rx0_2 = x0_2 * scale2, ry0_2 = y0_2 * scale2;
        float rx1_2 = x1_2 * scale2, ry1_2 = y1_2 * scale2;
        float rx2_2 = x2_2 * scale2, ry2_2 = y2_2 * scale2;
        
        rotatePoint(rx0_2, ry0_2, 0, 0, angle2);
        rotatePoint(rx1_2, ry1_2, 0, 0, angle2);
        rotatePoint(rx2_2, ry2_2, 0, 0, angle2);
        
        rx0_2 += centerX; ry0_2 += centerY;
        rx1_2 += centerX; ry1_2 += centerY;
        rx2_2 += centerX; ry2_2 += centerY;
        
        drawFilledTriangleColored(renderer,
          (int)rx0_2, (int)ry0_2, 
          dynamicColor(time, 1.0f),
          dynamicColor(time, 1.0f),
          0,
          (int)rx1_2, (int)ry1_2, 
          0,
          dynamicColor(time, 1.0f),
          dynamicColor(time, 1.0f),
          (int)rx2_2, (int)ry2_2, 
          dynamicColor(time, 1.0f),
          0,
          dynamicColor(time, 1.0f));

        // ---------------------- 渲染小三角形 ----------------------
        float rx0_3 = x0_3 * scale3, ry0_3 = y0_3 * scale3;
        float rx1_3 = x1_3 * scale3, ry1_3 = y1_3 * scale3;
        float rx2_3 = x2_3 * scale3, ry2_3 = y2_3 * scale3;
        
        rotatePoint(rx0_3, ry0_3, 0, 0, angle3);
        rotatePoint(rx1_3, ry1_3, 0, 0, angle3);
        rotatePoint(rx2_3, ry2_3, 0, 0, angle3);
        
        rx0_3 += centerX; ry0_3 += centerY;
        rx1_3 += centerX; ry1_3 += centerY;
        rx2_3 += centerX; ry2_3 += centerY;
        
        drawFilledTriangleColored(renderer,
          (int)rx0_3, (int)ry0_3, 
          255,
          dynamicColor(time, 0.0f),
          0,
          (int)rx1_3, (int)ry1_3, 
          255,
          255,
          255,
          (int)rx2_3, (int)ry2_3, 
          128,
          0,
          dynamicColor(time, 0.0f));

        // 4. 显示画面
        SDL_RenderPresent(renderer);

        // 5. FPS计算与输出
        frameCount++;
        if (time - lastFPSTime >= 1.0f) {
            fps = frameCount;
            frameCount = 0;
            lastFPSTime = time;
            cout << "FPS: " << fps << endl;
        }
    }

    // 6. 资源释放与退出
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    cout << "程序正常退出！" << endl;
    return 0;
}