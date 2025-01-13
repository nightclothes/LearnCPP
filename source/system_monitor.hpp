#pragma once
#include <windows.h>
#include <pdh.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <map>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "iphlpapi.lib")

class SystemMonitor {
public:
    SystemMonitor() {
        Initialize();
    }

    ~SystemMonitor() {
        if (cpuQuery) {
            PdhCloseQuery(cpuQuery);
        }
    }

    struct SystemInfo {
        double cpuUsage;
        double memoryUsage;
        double diskUsage;
        std::vector<double> cpuHistory;
        std::vector<double> memoryHistory;
        ULONGLONG networkReceived;
        ULONGLONG networkSent;
        double diskReadSpeed;
        double diskWriteSpeed;
        double systemUptime;
        double cpuTemperature;
    };

    struct ProcessInfo {
        std::string name;
        double cpuUsage;
        SIZE_T memoryUsage;
        std::string status;
        DWORD pid;
    };

    struct NetworkInfo {
        std::string adapterName;
        ULONG64 bytesReceived;
        ULONG64 bytesSent;
        double uploadSpeed;    // bytes per second
        double downloadSpeed;  // bytes per second
    };

    struct DiskInfo {
        std::string driveLetter;
        double totalSpace;     // GB
        double usedSpace;      // GB
        double freeSpace;      // GB
        double readSpeed;      // MB/s
        double writeSpeed;     // MB/s
    };

    SystemInfo GetSystemInfo() {
        SystemInfo info;
        
        // CPU使用率
        PDH_FMT_COUNTERVALUE counterVal;
        PdhCollectQueryData(cpuQuery);
        PdhGetFormattedCounterValue(cpuCounter, PDH_FMT_DOUBLE, NULL, &counterVal);
        info.cpuUsage = counterVal.doubleValue;

        // 内存使用率
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        info.memoryUsage = memInfo.dwMemoryLoad;

        // 磁盘使用率
        ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
        GetDiskFreeSpaceExA("C:\\", &freeBytesAvailable, &totalBytes, &totalFreeBytes);
        info.diskUsage = (1.0 - (double)totalFreeBytes.QuadPart / totalBytes.QuadPart) * 100.0;

        // 更新历史数据
        UpdateHistoryData(info.cpuUsage, info.memoryUsage);
        info.cpuHistory = cpuHistory;
        info.memoryHistory = memoryHistory;

        // 系统运行时间
        info.systemUptime = GetSystemUptime();

        // CPU温度 (需要WMI查询，这里用模拟数据)
        info.cpuTemperature = 45.0 + (rand() % 20);

        return info;
    }

    std::vector<ProcessInfo> GetProcessList() {
        std::vector<ProcessInfo> processes;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W processEntry = { sizeof(PROCESSENTRY32W) };
            
            if (Process32FirstW(snapshot, &processEntry)) {
                do {
                    ProcessInfo info;
                    info.name = WideToUtf8(processEntry.szExeFile);
                    info.pid = processEntry.th32ProcessID;
                    
                    // 获取进程内存使用
                    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, info.pid);
                    if (processHandle != NULL) {
                        PROCESS_MEMORY_COUNTERS pmc;
                        if (GetProcessMemoryInfo(processHandle, &pmc, sizeof(pmc))) {
                            info.memoryUsage = pmc.WorkingSetSize / 1024 / 1024; // Convert to MB
                        }
                        CloseHandle(processHandle);
                    }
                    
                    info.cpuUsage = GetProcessCpuUsage(info.pid);
                    info.status = "运行中";
                    
                    processes.push_back(info);
                } while (Process32NextW(snapshot, &processEntry));
            }
            CloseHandle(snapshot);
        }
        return processes;
    }

    std::vector<NetworkInfo> GetNetworkInfo() {
        std::vector<NetworkInfo> networkInfos;
        
        // 获取网络适配器信息
        ULONG size = 0;
        GetAdaptersInfo(NULL, &size);
        std::vector<IP_ADAPTER_INFO> adapters(size / sizeof(IP_ADAPTER_INFO) + 1);
        
        if (GetAdaptersInfo(&adapters[0], &size) == ERROR_SUCCESS) {
            PIP_ADAPTER_INFO pAdapter = &adapters[0];
            while (pAdapter) {
                NetworkInfo info;
                info.adapterName = pAdapter->Description;
                
                // 获取网络流量
                MIB_IFROW ifRow;
                ifRow.dwIndex = pAdapter->Index;
                if (GetIfEntry(&ifRow) == NO_ERROR) {
                    info.bytesReceived = ifRow.dwInOctets;
                    info.bytesSent = ifRow.dwOutOctets;
                    
                    // 计算速度（与上次获取的差值）
                    static std::map<DWORD, std::pair<ULONG64, ULONG64>> lastBytes;
                    static std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();
                    
                    auto now = std::chrono::steady_clock::now();
                    double seconds = std::chrono::duration<double>(now - lastTime).count();
                    
                    if (lastBytes.find(pAdapter->Index) != lastBytes.end()) {
                        info.downloadSpeed = (info.bytesReceived - lastBytes[pAdapter->Index].first) / seconds;
                        info.uploadSpeed = (info.bytesSent - lastBytes[pAdapter->Index].second) / seconds;
                    }
                    
                    lastBytes[pAdapter->Index] = {info.bytesReceived, info.bytesSent};
                    lastTime = now;
                }
                
                networkInfos.push_back(info);
                pAdapter = pAdapter->Next;
            }
        }
        
        return networkInfos;
    }

    std::vector<DiskInfo> GetDiskInfo() {
        std::vector<DiskInfo> diskInfos;
        DWORD drives = GetLogicalDrives();
        char driveLetter = 'A';
        
        while (drives) {
            if (drives & 1) {
                std::string root = std::string(1, driveLetter) + ":\\";
                ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
                
                if (GetDiskFreeSpaceExA(root.c_str(), &freeBytesAvailable, 
                    &totalBytes, &totalFreeBytes)) {
                    DiskInfo info;
                    info.driveLetter = root;
                    info.totalSpace = totalBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
                    info.freeSpace = totalFreeBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
                    info.usedSpace = info.totalSpace - info.freeSpace;
                    
                    // 获取磁盘读写速度（模拟数据）
                    static std::map<char, std::pair<double, double>> diskSpeeds;
                    if (diskSpeeds.find(driveLetter) == diskSpeeds.end()) {
                        diskSpeeds[driveLetter] = {0.0, 0.0};
                    }
                    
                    // 模拟随机波动的读写速度
                    diskSpeeds[driveLetter].first += (rand() % 100 - 50) / 10.0;
                    diskSpeeds[driveLetter].second += (rand() % 100 - 50) / 10.0;
                    
                    info.readSpeed = std::max(0.0, diskSpeeds[driveLetter].first);
                    info.writeSpeed = std::max(0.0, diskSpeeds[driveLetter].second);
                    
                    diskInfos.push_back(info);
                }
            }
            drives >>= 1;
            driveLetter++;
        }
        
        return diskInfos;
    }

    std::string FormatBytes(double bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;
        
        while (bytes >= 1024 && unitIndex < 4) {
            bytes /= 1024;
            unitIndex++;
        }
        
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << bytes << " " << units[unitIndex];
        return ss.str();
    }

private:
    PDH_HQUERY cpuQuery;
    PDH_HCOUNTER cpuCounter;
    std::vector<double> cpuHistory;
    std::vector<double> memoryHistory;
    static const size_t HISTORY_SIZE = 100;

    void Initialize() {
        PdhOpenQueryA(NULL, 0, &cpuQuery);
        PdhAddCounterA(cpuQuery, "\\Processor(_Total)\\% Processor Time", 0, &cpuCounter);
        PdhCollectQueryData(cpuQuery);

        cpuHistory.resize(HISTORY_SIZE, 0.0);
        memoryHistory.resize(HISTORY_SIZE, 0.0);
    }

    void UpdateHistoryData(double cpu, double memory) {
        cpuHistory.erase(cpuHistory.begin());
        cpuHistory.push_back(cpu);
        memoryHistory.erase(memoryHistory.begin());
        memoryHistory.push_back(memory);
    }

    double GetSystemUptime() {
        return GetTickCount64() / 1000.0 / 3600.0; // Convert to hours
    }

    double GetProcessCpuUsage(DWORD pid) {
        // 简化的CPU使用率计算，实际应该使用更复杂的计算方法
        static std::map<DWORD, FILETIME> lastCPUTimes;
        
        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (process == NULL) return 0.0;

        FILETIME createTime, exitTime, kernelTime, userTime;
        if (!GetProcessTimes(process, &createTime, &exitTime, &kernelTime, &userTime)) {
            CloseHandle(process);
            return 0.0;
        }

        ULARGE_INTEGER kernelTimeValue, userTimeValue;
        kernelTimeValue.LowPart = kernelTime.dwLowDateTime;
        kernelTimeValue.HighPart = kernelTime.dwHighDateTime;
        userTimeValue.LowPart = userTime.dwLowDateTime;
        userTimeValue.HighPart = userTime.dwHighDateTime;

        CloseHandle(process);

        if (lastCPUTimes.find(pid) == lastCPUTimes.end()) {
            lastCPUTimes[pid] = kernelTime;
            return 0.0;
        }

        ULARGE_INTEGER lastTime;
        lastTime.LowPart = lastCPUTimes[pid].dwLowDateTime;
        lastTime.HighPart = lastCPUTimes[pid].dwHighDateTime;

        ULARGE_INTEGER systemTime;
        GetSystemTimeAsFileTime((FILETIME*)&systemTime);

        double cpuUsage = ((kernelTimeValue.QuadPart - lastTime.QuadPart) / 
                          static_cast<double>(systemTime.QuadPart)) * 100.0;

        lastCPUTimes[pid] = kernelTime;
        return cpuUsage;
    }

    std::string WideToUtf8(const wchar_t* str) {
        int size = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, str, -1, &result[0], size, nullptr, nullptr);
        return result;
    }
}; 