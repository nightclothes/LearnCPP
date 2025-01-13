// Dear ImGui: standalone example application for DirectX 11

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#define _USE_MATH_DEFINES
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <vector>
#include <string>
#include "system_monitor.hpp"

// Data
// Direct3D 11 设备指针，用于创建和管理Direct3D资源
static ID3D11Device*            g_pd3dDevice = nullptr;
// Direct3D 11 设备上下文指针，用于执行渲染命令
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
// DXGI 交换链指针，用于管理前后缓冲区的交换
static IDXGISwapChain*          g_pSwapChain = nullptr;
// 标识交换链是否被遮挡，用于处理窗口最小化等情况
static bool                     g_SwapChainOccluded = false;
// 窗口调整后的宽度和高度，用于处理窗口大小变化
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
// 主渲染目标视图指针，用于指定渲染目标
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 在文件开头添加枚举类型
enum class MenuPage {
    Dashboard,
    DataVisualization,
    SystemMonitor,
    Settings
};

// 添加全局变量
static MenuPage current_page = MenuPage::Dashboard;
static std::vector<float> cpu_history(100, 0.0f);
static std::vector<float> memory_history(100, 0.0f);
static std::vector<float> network_history(100, 0.0f);

// 在文件开头添加新的全局变量
static const ImVec4 THEME_COLOR_MAIN = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
static const ImVec4 THEME_COLOR_DARK = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
static const ImVec4 THEME_COLOR_LIGHT = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
static const ImVec4 THEME_COLOR_ACCENT = ImVec4(0.28f, 0.56f, 1.00f, 0.50f);

// 在文件开头添加
static const char* const MENU_ICONS[] = { "📊", "📈", "🔍", "⚙" };
static const char* const MENU_ITEMS[] = { "仪表盘", "数据可视化", "系统监控", "设置" };

// 添加全局变量
static SystemMonitor g_SystemMonitor;
static SystemMonitor::SystemInfo g_SystemInfo;
static std::vector<SystemMonitor::ProcessInfo> g_ProcessList;
static std::chrono::steady_clock::time_point g_LastUpdateTime;

// 在文件开头添加
struct ScrollingBuffer {
    static const int MaxSize = 100;
    float Values[MaxSize];
    int Offset;
    
    ScrollingBuffer() {
        Offset = 0;
        memset(Values, 0, sizeof(Values));
    }

    void AddValue(float value) {
        Values[Offset] = value;
        Offset = (Offset + 1) % MaxSize;
    }

    void Draw(const char* label, float scale_min, float scale_max) {
        ImGui::PlotLines(label, 
            [](void* data, int idx) {
                ScrollingBuffer* self = (ScrollingBuffer*)data;
                int pos = (self->Offset + idx) % MaxSize;
                return self->Values[pos];
            },
            this, MaxSize, 0, nullptr, scale_min, scale_max, ImVec2(-1, 150));
    }
};

// 在文件开头添加
struct PerformanceData {
    ScrollingBuffer cpuHistory;
    ScrollingBuffer memHistory;
    std::vector<float> diskReadSpeeds;
    std::vector<float> diskWriteSpeeds;
    std::chrono::steady_clock::time_point lastUpdate;
    
    void Update(const SystemMonitor::SystemInfo& info) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastUpdate).count() > 1000) {
            cpuHistory.AddValue(info.cpuUsage / 100.0f);
            memHistory.AddValue(info.memoryUsage / 100.0f);
            lastUpdate = now;
        }
    }
};

static PerformanceData g_PerformanceData;

// 在ShowExampleAppMenu函数中更新系统信息
void UpdateSystemInfo() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - g_LastUpdateTime).count() > 1000) {
        g_SystemInfo = g_SystemMonitor.GetSystemInfo();
        g_ProcessList = g_SystemMonitor.GetProcessList();
        g_LastUpdateTime = now;
    }
}

// 在文件中添加主题函数
void ApplyBlueTheme()
{
    ImGui::StyleColorsDark();
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.15f, 0.94f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.08f, 0.08f, 0.20f, 0.94f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.20f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.12f, 0.12f, 0.30f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.15f, 0.15f, 0.40f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.18f, 0.18f, 0.50f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.08f, 0.20f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.78f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.88f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.98f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
}

void ApplyGreenTheme()
{
    ImGui::StyleColorsDark();
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.15f, 0.06f, 0.94f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.08f, 0.20f, 0.08f, 0.94f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.20f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.50f, 0.43f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.12f, 0.30f, 0.12f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.15f, 0.40f, 0.15f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.18f, 0.50f, 0.18f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.12f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.20f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.35f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.78f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.88f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.98f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.26f, 0.98f, 0.26f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.88f, 0.24f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.98f, 0.26f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.26f, 0.98f, 0.26f, 0.40f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.26f, 0.98f, 0.26f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.06f, 0.98f, 0.06f, 1.00f);
}

// 在文件中添加设置保存函数
void SaveSettings(bool notifications, bool dark_mode, float refresh_rate, 
                 int process_limit, bool show_system_processes, 
                 const char* log_path, int theme)
{
    // 这里可以实现设置的保存逻辑，比如写入配置文件
    // 暂时只打印设置信息
    printf("Settings saved:\n");
    printf("Notifications: %d\n", notifications);
    printf("Dark Mode: %d\n", dark_mode);
    printf("Refresh Rate: %.1f\n", refresh_rate);
    printf("Process Limit: %d\n", process_limit);
    printf("Show System Processes: %d\n", show_system_processes);
    printf("Log Path: %s\n", log_path);
    printf("Theme: %d\n", theme);
}

void DrawPieChart(const char* label, float used, float total, const ImVec2& size) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float radius = size.x * 0.5f;
    ImVec2 center = ImVec2(pos.x + radius, pos.y + radius);
    
    // 绘制背景圆
    draw_list->AddCircleFilled(center, radius, IM_COL32(50, 50, 50, 255));
    
    // 绘制使用量扇形
    float angle = (used / total) * 2.0f * (float)M_PI;
    int segments = 50;
    draw_list->PathClear();
    draw_list->PathLineTo(center);
    for (int i = 0; i <= segments; i++) {
        float a = (i / (float)segments) * angle;
        draw_list->PathLineTo(ImVec2(
            center.x + cosf(a - (float)M_PI/2) * radius,
            center.y + sinf(a - (float)M_PI/2) * radius
        ));
    }
    draw_list->PathFillConvex(IM_COL32(0, 191, 255, 255));
    
    // 显示百分比
    char overlay[32];
    sprintf(overlay, "%.1f%%", (used/total) * 100);
    auto textSize = ImGui::CalcTextSize(overlay);
    draw_list->AddText(
        ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
        IM_COL32(255, 255, 255, 255),
        overlay
    );
    
    ImGui::Dummy(size);
}

struct AppSettings {
    bool notifications = true;
    bool dark_mode = true;
    float refresh_rate = 1.0f;
    int process_limit = 50;
    bool show_system_processes = true;
    std::string log_path = "system_monitor.log";
    int theme = 0;
    
    void Save(const char* filename = "settings.ini") {
        FILE* f = fopen(filename, "w");
        if (f) {
            fprintf(f, "notifications=%d\n", notifications);
            fprintf(f, "dark_mode=%d\n", dark_mode);
            fprintf(f, "refresh_rate=%.1f\n", refresh_rate);
            fprintf(f, "process_limit=%d\n", process_limit);
            fprintf(f, "show_system_processes=%d\n", show_system_processes);
            fprintf(f, "log_path=%s\n", log_path.c_str());
            fprintf(f, "theme=%d\n", theme);
            fclose(f);
        }
    }
    
    void Load(const char* filename = "settings.ini") {
        FILE* f = fopen(filename, "r");
        if (f) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), f)) {
                char key[64], value[192];
                if (sscanf(buffer, "%[^=]=%[^\n]", key, value) == 2) {
                    if (strcmp(key, "notifications") == 0) notifications = atoi(value);
                    else if (strcmp(key, "dark_mode") == 0) dark_mode = atoi(value);
                    else if (strcmp(key, "refresh_rate") == 0) refresh_rate = atof(value);
                    else if (strcmp(key, "process_limit") == 0) process_limit = atoi(value);
                    else if (strcmp(key, "show_system_processes") == 0) show_system_processes = atoi(value);
                    else if (strcmp(key, "log_path") == 0) log_path = value;
                    else if (strcmp(key, "theme") == 0) theme = atoi(value);
                }
            }
            fclose(f);
        }
    }
};

static AppSettings g_Settings;

// 替换 ShowExampleAppMenu 函数
void ShowExampleAppMenu()
{
    // 保存当前样式状态
    const auto style_backup = ImGui::GetStyle();
    
    // 设置全局样式
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));
    
    // 设置颜色
    const int color_count = 4;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, THEME_COLOR_DARK);
    ImGui::PushStyleColor(ImGuiCol_Button, THEME_COLOR_LIGHT);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, THEME_COLOR_MAIN);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, THEME_COLOR_ACCENT);

    try {
        // 左侧菜单面板
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(250, ImGui::GetIO().DisplaySize.y));
        
        if (ImGui::Begin("##MainMenu", nullptr, 
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus))
        {
            // Logo和标题区域
            {
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
                ImGui::PushStyleColor(ImGuiCol_Text, THEME_COLOR_MAIN);
                ImGui::Text("系统监控面板");
                ImGui::PopStyleColor();
                ImGui::PopFont();
                
                ImGui::Text("v1.0.0");
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
                ImGui::Separator();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
            }

            // 菜单项样式
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 12));
            ImGui::PushStyleColor(ImGuiCol_Header, THEME_COLOR_LIGHT);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, THEME_COLOR_MAIN);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, THEME_COLOR_ACCENT);

            // 菜单项
            for (int i = 0; i < 4; i++) {
                ImGui::PushID(i);
                bool selected = current_page == static_cast<MenuPage>(i);
                
                if (selected) {
                    ImGui::PushStyleColor(ImGuiCol_Text, THEME_COLOR_MAIN);
                }
                
                if (ImGui::Selectable(MENU_ITEMS[i], selected, 0, ImVec2(0, 45))) {
                    current_page = static_cast<MenuPage>(i);
                }
                
                // 在选项左侧绘制图标
                if (selected) {
                    ImGui::SameLine(5);
                    ImGui::Text(MENU_ICONS[i]);
                    ImGui::PopStyleColor();
                    
                    // 绘制选中指示器
                    auto pos = ImGui::GetItemRectMin();
                    auto size = ImGui::GetItemRectSize();
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        ImVec2(pos.x - 5, pos.y), 
                        ImVec2(pos.x - 2, pos.y + size.y),
                        ImGui::ColorConvertFloat4ToU32(THEME_COLOR_MAIN)
                    );
                }
                
                ImGui::PopID();
            }

            // 底部信息
            ImGui::SetCursorPos(ImVec2(15, ImGui::GetIO().DisplaySize.y - 70));
            ImGui::Text("系统状态: 正常运行");
            ImGui::Text("更新时间: %s", "2024-01-01");

            ImGui::PopStyleVar(); // FramePadding
            ImGui::PopStyleColor(3); // Header colors
        }
        ImGui::End();

        // 主内容区域
        ImGui::SetNextWindowPos(ImVec2(250, 0));
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - 250, ImGui::GetIO().DisplaySize.y));
        
        if (ImGui::Begin("##MainContent", nullptr, 
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus))
        {
            // 内容区域的标题栏
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            ImGui::Text("%s %s", MENU_ICONS[static_cast<int>(current_page)], 
                MENU_ITEMS[static_cast<int>(current_page)]);
            ImGui::PopFont();
            ImGui::Separator();
            ImGui::Spacing();

            // 根据当前选择的页面显示不同内容
            switch (current_page)
            {
                case MenuPage::Dashboard:
                {
                    UpdateSystemInfo();
                    
                    ImGui::Text("系统概览");
                    ImGui::Separator();
                    ImGui::Spacing();

                    // CPU和内存使用率
                    {
                        ImGui::BeginChild("Performance", ImVec2(0, 150), true);
                        float cpuUsage = g_SystemInfo.cpuUsage / 100.0f;
                        float memUsage = g_SystemInfo.memoryUsage / 100.0f;
                        
                        // CPU使用率圆形进度条
                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        ImVec2 center = ImGui::GetCursorScreenPos();
                        center.x += 60;
                        center.y += 60;
                        
                        draw_list->AddCircle(center, 50, IM_COL32(100, 100, 100, 255), 32, 4);
                        draw_list->AddCircleFilled(center, 48, IM_COL32(30, 30, 30, 255), 32);
                        
                        // 绘制进度弧
                        int segments = 32;
                        float angle = cpuUsage * 2 * 3.14159f;
                        for (int i = 0; i < segments; i++) {
                            float a1 = (float)i / segments * angle;
                            float a2 = (float)(i + 1) / segments * angle;
                            draw_list->AddLine(
                                ImVec2(center.x + cosf(a1) * 48, center.y + sinf(a1) * 48),
                                ImVec2(center.x + cosf(a2) * 48, center.y + sinf(a2) * 48),
                                IM_COL32(0, 191, 255, 255), 4
                            );
                        }
                        
                        // CPU使用率文本
                        char cpuText[32];
                        sprintf(cpuText, "%.1f%%", g_SystemInfo.cpuUsage);
                        auto textSize = ImGui::CalcTextSize(cpuText);
                        draw_list->AddText(
                            ImVec2(center.x - textSize.x/2, center.y - textSize.y/2),
                            IM_COL32(255, 255, 255, 255), cpuText
                        );
                        
                        // 内存使用率条
                        ImGui::SameLine(150);
                        ImGui::BeginGroup();
                        ImGui::Text("内存使用");
                        ImGui::ProgressBar(memUsage, ImVec2(200, 20));
                        ImGui::Text("%.1f%%", g_SystemInfo.memoryUsage);
                        ImGui::EndGroup();
                        
                        ImGui::EndChild();
                    }

                    // 系统信息卡片
                    ImGui::Columns(3, "SystemInfo", false);
                    
                    // 运行时间卡片
                    ImGui::BeginChild("Uptime", ImVec2(0, 100), true);
                    ImGui::Text("系统运行时间");
                    float hours = g_SystemInfo.systemUptime;
                    int days = (int)(hours / 24);
                    hours = fmod(hours, 24);
                    ImGui::Text("%d 天 %.1f 小时", days, hours);
                    ImGui::EndChild();
                    ImGui::NextColumn();

                    // CPU温度卡片
                    ImGui::BeginChild("Temperature", ImVec2(0, 100), true);
                    ImGui::Text("CPU温度");
                    ImGui::Text("%.1f °C", g_SystemInfo.cpuTemperature);
                    if (g_SystemInfo.cpuTemperature > 80)
                        ImGui::TextColored(ImVec4(1, 0, 0, 1), "警告：温度过高！");
                    ImGui::EndChild();
                    ImGui::NextColumn();

                    // 磁盘使用卡片
                    ImGui::BeginChild("DiskUsage", ImVec2(0, 100), true);
                    ImGui::Text("磁盘使用率");
                    ImGui::Text("%.1f%%", g_SystemInfo.diskUsage);
                    ImGui::ProgressBar(g_SystemInfo.diskUsage / 100.0f);
                    ImGui::EndChild();
                    
                    ImGui::Columns(1);
                    break;
                }
                case MenuPage::DataVisualization:
                {
                    UpdateSystemInfo();
                    static ScrollingBuffer cpuData, memData;
                    
                    // 更新数据
                    cpuData.AddValue(g_SystemInfo.cpuUsage / 100.0f);
                    memData.AddValue(g_SystemInfo.memoryUsage / 100.0f);

                    // CPU和内存使用率历史图表
                    ImGui::BeginChild("性能历史", ImVec2(0, 400), true);
                    
                    ImGui::Text("CPU使用率历史");
                    cpuData.Draw("##CPU", 0.0f, 1.0f);
                    
                    ImGui::Spacing();
                    ImGui::Text("内存使用率历史");
                    memData.Draw("##Memory", 0.0f, 1.0f);
                    
                    ImGui::EndChild();

                    // 磁盘使用情况
                    ImGui::Text("磁盘使用情况");
                    auto diskInfos = g_SystemMonitor.GetDiskInfo();
                    
                    ImGui::Columns(3, "DiskInfo", false);
                    for (const auto& disk : diskInfos) {
                        ImGui::BeginGroup();
                        ImGui::Text("%s", disk.driveLetter.c_str());
                        DrawPieChart(disk.driveLetter.c_str(), 
                                    disk.usedSpace,
                                    disk.totalSpace,
                                    ImVec2(150, 150));
                        
                        ImGui::Text("总容量: %.1f GB", disk.totalSpace);
                        ImGui::Text("已用: %.1f GB", disk.usedSpace);
                        ImGui::Text("可用: %.1f GB", disk.freeSpace);
                        ImGui::Text("读取速度: %.1f MB/s", disk.readSpeed);
                        ImGui::Text("写入速度: %.1f MB/s", disk.writeSpeed);
                        ImGui::EndGroup();
                        
                        ImGui::NextColumn();
                    }
                    ImGui::Columns(1);
                    break;
                }
                case MenuPage::SystemMonitor:
                {
                    UpdateSystemInfo();
                    
                    // 进程列表
                    static ImGuiTableFlags flags = 
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | 
                        ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | 
                        ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | 
                        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                        ImGuiTableFlags_ScrollY;
                        
                    if (ImGui::BeginTable("进程列表", 5, flags)) {
                        ImGui::TableSetupScrollFreeze(0, 1); // 顶部行固定
                        ImGui::TableSetupColumn("进程名", ImGuiTableColumnFlags_DefaultSort);
                        ImGui::TableSetupColumn("PID");
                        ImGui::TableSetupColumn("CPU使用率 %");
                        ImGui::TableSetupColumn("内存使用");
                        ImGui::TableSetupColumn("状态");
                        ImGui::TableHeadersRow();

                        // 进程列表排序
                        if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs()) {
                            if (sorts_specs->SpecsDirty) {
                                std::sort(g_ProcessList.begin(), g_ProcessList.end(),
                                    [sorts_specs](const SystemMonitor::ProcessInfo& a, const SystemMonitor::ProcessInfo& b) {
                                        for (int n = 0; n < sorts_specs->SpecsCount; n++) {
                                            const ImGuiTableColumnSortSpecs* sort_spec = &sorts_specs->Specs[n];
                                            int delta = 0;
                                            switch (sort_spec->ColumnIndex) {
                                                case 0: delta = a.name.compare(b.name); break;
                                                case 1: delta = a.pid - b.pid; break;
                                                case 2: delta = a.cpuUsage - b.cpuUsage; break;
                                                case 3: delta = a.memoryUsage - b.memoryUsage; break;
                                                case 4: delta = a.status.compare(b.status); break;
                                            }
                                            if (delta > 0)
                                                return sort_spec->SortDirection == ImGuiSortDirection_Ascending;
                                            if (delta < 0)
                                                return sort_spec->SortDirection == ImGuiSortDirection_Descending;
                                        }
                                        return false;
                                    });
                                sorts_specs->SpecsDirty = false;
                            }
                        }

                        // 显示进程信息
                        for (const auto& process : g_ProcessList) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", process.name.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%u", process.pid);
                            ImGui::TableNextColumn();
                            ImGui::Text("%.1f", process.cpuUsage);
                            ImGui::TableNextColumn();
                            ImGui::Text("%zu MB", process.memoryUsage);
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", process.status.c_str());
                        }
                        ImGui::EndTable();
                    }

                    // 网络监控
                    auto networkInfos = g_SystemMonitor.GetNetworkInfo();
                    if (ImGui::BeginTable("网络监控", 4, ImGuiTableFlags_Borders)) {
                        ImGui::TableSetupColumn("适配器");
                        ImGui::TableSetupColumn("上传速度");
                        ImGui::TableSetupColumn("下载速度");
                        ImGui::TableSetupColumn("总流量");
                        ImGui::TableHeadersRow();

                        for (const auto& net : networkInfos) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", net.adapterName.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%s/s", g_SystemMonitor.FormatBytes(net.uploadSpeed).c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%s/s", g_SystemMonitor.FormatBytes(net.downloadSpeed).c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("↑%s ↓%s", 
                                g_SystemMonitor.FormatBytes(net.bytesSent).c_str(),
                                g_SystemMonitor.FormatBytes(net.bytesReceived).c_str());
                        }
                        ImGui::EndTable();
                    }
                    break;
                }
                case MenuPage::Settings:
                {
                    static bool enable_notifications = true;
                    static bool dark_mode = true;
                    static float refresh_rate = 1.0f;
                    static int process_limit = 50;
                    static bool show_system_processes = true;
                    static char log_path[256] = "system_monitor.log";
                    static int selected_theme = 0;
                    const char* themes[] = { "深色主题", "浅色主题", "蓝色主题", "绿色主题" };

                    ImGui::Text("基本设置");
                    ImGui::Separator();
                    ImGui::Spacing();

                    if (ImGui::Checkbox("启用通知", &enable_notifications)) {
                        // 处理通知设置变更
                    }

                    if (ImGui::Checkbox("深色模式", &dark_mode)) {
                        // 切换主题
                        if (dark_mode)
                            ImGui::StyleColorsDark();
                        else
                            ImGui::StyleColorsLight();
                    }

                    ImGui::SliderFloat("刷新频率 (秒)", &refresh_rate, 0.1f, 5.0f, "%.1f");
                    ImGui::SliderInt("进程显示数量限制", &process_limit, 10, 200);
                    ImGui::Checkbox("显示系统进程", &show_system_processes);

                    ImGui::Spacing();
                    ImGui::Text("主题设置");
                    ImGui::Separator();
                    ImGui::Spacing();

                    if (ImGui::Combo("选择主题", &selected_theme, themes, IM_ARRAYSIZE(themes))) {
                        // 应用选择的主题
                        switch (selected_theme) {
                            case 0: ImGui::StyleColorsDark(); break;
                            case 1: ImGui::StyleColorsLight(); break;
                            case 2: ApplyBlueTheme(); break;
                            case 3: ApplyGreenTheme(); break;
                        }
                    }

                    ImGui::Spacing();
                    ImGui::Text("日志设置");
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::InputText("日志文件路径", log_path, sizeof(log_path));

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    if (ImGui::Button("保存设置", ImVec2(120, 30))) {
                        // 保存所有设置
                        SaveSettings(enable_notifications, dark_mode, refresh_rate, 
                                    process_limit, show_system_processes, log_path, selected_theme);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("重置设置", ImVec2(120, 30))) {
                        // 重置为默认设置
                        enable_notifications = true;
                        dark_mode = true;
                        refresh_rate = 1.0f;
                        process_limit = 50;
                        show_system_processes = true;
                        strcpy(log_path, "system_monitor.log");
                        selected_theme = 0;
                        ImGui::StyleColorsDark();
                    }

                    // 显示关于信息
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Text("关于");
                    ImGui::Text("系统监控工具 v1.0.0");
                    ImGui::Text("作者: Your Name");
                    ImGui::Text("构建时间: %s %s", __DATE__, __TIME__);
                    break;
                }
            }
        }
        ImGui::End();
    }
    catch (...) {
        // 确保样式被正确恢复
        ImGui::PopStyleColor(color_count);
        ImGui::PopStyleVar(2);
        ImGui::GetStyle() = style_backup;
        throw;
    }

    // 恢复样式
    ImGui::PopStyleColor(color_count);
    ImGui::PopStyleVar(2);
}

// Main code
int imgui_example()
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX11 Example", WS_OVERLAPPEDWINDOW, 100, 100, 1000, 1000, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      

    // 加载中文字体（确保路径存在，否则使用默认字体）
    ImFont* font = nullptr;
    try {
        font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msyh.ttc", 18.0f, nullptr, 
            io.Fonts->GetGlyphRangesChineseFull());
    } catch (...) {
        font = io.Fonts->AddFontDefault();
    }
    if (font == nullptr) {
        font = io.Fonts->AddFontDefault();
    }

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    
    // 基础样式设置
    style.FrameRounding = 6.0f;
    style.WindowRounding = 8.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(15, 15);
    style.ItemSpacing = ImVec2(8, 8);
    style.ScrollbarSize = 12.0f;

    // 颜色设置
    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg] = THEME_COLOR_DARK;
    colors[ImGuiCol_FrameBg] = THEME_COLOR_LIGHT;
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.27f, 0.32f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = THEME_COLOR_DARK;
    colors[ImGuiCol_CheckMark] = THEME_COLOR_MAIN;
    colors[ImGuiCol_SliderGrab] = THEME_COLOR_MAIN;
    colors[ImGuiCol_SliderGrabActive] = THEME_COLOR_ACCENT;
    colors[ImGuiCol_ScrollbarBg] = THEME_COLOR_DARK;
    colors[ImGuiCol_ScrollbarGrab] = THEME_COLOR_LIGHT;
    colors[ImGuiCol_ScrollbarGrabHovered] = THEME_COLOR_MAIN;
    colors[ImGuiCol_ScrollbarGrabActive] = THEME_COLOR_ACCENT;

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ShowExampleAppMenu();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions
// 创建 Direct3D 设备并初始化交换链
bool CreateDeviceD3D(HWND hWnd)
{
    // 设置交换链描述
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // 如果硬件不可用，尝试高性能的 WARP 软件驱动
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

// 清理 Direct3D 设备及其相关资源
void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

// 创建渲染目标视图
void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

// 清理渲染目标资源，释放g_mainRenderTargetView并置空
void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
// 窗口消息处理函数，处理窗口的各种消息
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // 使用ImGui处理窗口消息，如果ImGui处理了消息则返回true
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    // 根据消息类型进行处理
    switch (msg)
    {
    case WM_SIZE:
        // 如果窗口被最小化，则返回0
        if (wParam == SIZE_MINIMIZED)
            return 0;
        // 获取窗口的新宽度和高度，并存储在全局变量中
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        // 禁用ALT键触发的应用程序菜单
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        // 窗口销毁时发送退出消息
        ::PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        // 如果启用了DPI缩放，则调整窗口大小以适应新的DPI
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {
            //const int dpi = HIWORD(wParam);
            //printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
            const RECT* suggested_rect = (RECT*)lParam;
            ::SetWindowPos(hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    }
    // 默认处理其他消息
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}


