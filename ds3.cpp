#include "stdafx.h"
#include "version.h"
#include "helper.hpp"

#ifdef _DEBUG
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#else
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <safetyhook.hpp>
#include <inipp/inipp.h>
#include <filesystem>
#include <fstream>

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

std::shared_ptr<spdlog::logger> logger;
std::string sLogFile = "DS3_ARFix.log";
std::filesystem::path ExePath;
std::string sExeName;

inipp::Ini<char> ini;
std::string sConfigFile = "DS3_ARFix.ini";
std::filesystem::path FixPath;

bool bCustomResolution = true;
uint32_t iTargetResX = 1024;
uint32_t iTargetResY = 768;

bool bEnableARFix = true;
float fTargetARValue = 1.3333333f;

bool bEnableCameraFOV = true;
float fCameraFOVValue = 60.0f;

template<typename T>
T GetConfigAndLog(inipp::Ini<char>& ini, const std::string& section, const std::string& key, T defaultValue) {
    T value = defaultValue;
    inipp::get_value(ini.sections[section], key, value);
    SPDLOG_INFO("Config Parse: [{}]: {} = {}", section, key, value);
    return value;
}

void Logging() {
    WCHAR dllPath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(thisModule, dllPath, MAX_PATH);
    FixPath = dllPath;
    FixPath = FixPath.remove_filename();

    WCHAR gameExePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(exeModule, gameExePath, MAX_PATH);
    ExePath = gameExePath;
    sExeName = ExePath.filename().string();
    ExePath = ExePath.remove_filename();

    try {
        std::ofstream file(ExePath / sLogFile, std::ios::trunc);
        if (file.is_open()) file.close();
        logger = std::make_shared<spdlog::logger>("DS3_ARFix", std::make_shared<spdlog::sinks::rotating_file_sink_st>((ExePath / sLogFile).string(), 10 * 1024 * 1024, 1));
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
        spdlog::flush_on(spdlog::level::debug);
        spdlog::set_level(spdlog::level::debug);
        SPDLOG_INFO("Dark Souls III Ultimate AR & Camera FOV Fix Initialized");
    }
    catch (...) {}
}

void Configuration() {
    std::ifstream iniFile(FixPath / sConfigFile);
    if (!iniFile) return;
    ini.parse(iniFile);
    ini.strip_trailing_comments();

    bCustomResolution = GetConfigAndLog(ini, "Resolution", "CustomResolution", true);
    iTargetResX = GetConfigAndLog(ini, "Resolution", "Width", 1024);
    iTargetResY = GetConfigAndLog(ini, "Resolution", "Height", 768);

    bEnableARFix = GetConfigAndLog(ini, "AspectRatio", "EnableARFix", true);
    fTargetARValue = static_cast<float>(iTargetResX) / static_cast<float>(iTargetResY);
    SPDLOG_INFO("Calculated Target Aspect Ratio (UI): {:.4f}", fTargetARValue);

    bEnableCameraFOV = GetConfigAndLog(ini, "Camera", "EnableCustomFOV", true);
    fCameraFOVValue = GetConfigAndLog(ini, "Camera", "FOVValue", 60.0f);
}

void PatchResolution() {
    if (!bCustomResolution) return;

    uint8_t* resAddr = Memory::PatternScan(exeModule, "20 03 00 00 C2 01 00 00"); 
    if (resAddr) {
        DWORD oldProtect;
        if (VirtualProtect(resAddr, sizeof(uint32_t) * 2, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            *reinterpret_cast<uint32_t*>(resAddr) = iTargetResX;
            *reinterpret_cast<uint32_t*>(resAddr + 4) = iTargetResY;
            VirtualProtect(resAddr, sizeof(uint32_t) * 2, oldProtect, &oldProtect);
            SPDLOG_INFO("Successfully replaced sacrificial resolution with {}x{}", iTargetResX, iTargetResY);
        }
    } else {
        SPDLOG_WARN("Could not find sacrificial resolution in memory.");
    }
}

void PatchAspectRatio() {
    if (!bEnableARFix) return;

    std::uint8_t* arScanResult = Memory::PatternScan(exeModule,
        "F3 0F 10 35 ?? ?? ?? ?? F3 48 0F 2A CE 44 0F 29 44 24 60");

    if (arScanResult) {
        static SafetyHookMid ARMidHook{};
        ARMidHook = safetyhook::create_mid(arScanResult, [](SafetyHookContext& ctx) {
            ctx.xmm6.f32[0] = fTargetARValue;
        });
        SPDLOG_INFO("Hooked Aspect Ratio reader at 0x{:X}", reinterpret_cast<uintptr_t>(arScanResult));
    }
}

void PatchCameraFOV() {
    if (!bEnableCameraFOV) return;

    std::uint8_t* fovScanResult = Memory::PatternScan(exeModule, "F3 0F 10 01 44 0F 2F E0 76 ?? 48 8D");

    if (fovScanResult) {
        static SafetyHookMid CameraFOVMidHook{};
        CameraFOVMidHook = safetyhook::create_mid(fovScanResult, [](SafetyHookContext& ctx) {
            if (ctx.rcx) {
                float* pFov = reinterpret_cast<float*>(ctx.rcx);
                if (*pFov > 35.0f && *pFov != fCameraFOVValue) {
                    *pFov = fCameraFOVValue;
                }
            }
        });
        SPDLOG_INFO("Hooked Camera FOV at 0x{:X}", reinterpret_cast<uintptr_t>(fovScanResult));
    } else {
        std::uint8_t* fovLoose = Memory::PatternScan(exeModule, "F3 0F 10 01 44 0F 2F E0");
        if (fovLoose) {
            static SafetyHookMid CameraFOVMidHookLoose{};
            CameraFOVMidHookLoose = safetyhook::create_mid(fovLoose, [](SafetyHookContext& ctx) {
                if (ctx.rcx) {
                    float* pFov = reinterpret_cast<float*>(ctx.rcx);
                    if (*pFov > 35.0f && *pFov != fCameraFOVValue) {
                        *pFov = fCameraFOVValue;
                    }
                }
            });
            SPDLOG_INFO("Hooked Camera FOV (loose) at 0x{:X}", reinterpret_cast<uintptr_t>(fovLoose));
        } else {
            SPDLOG_WARN("Could not find Camera FOV pattern.");
        }
    }
}

DWORD __stdcall Main(void*) {
    Logging();
    Configuration();

    Sleep(500); 
    PatchResolution();
    PatchAspectRatio();
    PatchCameraFOV();

    return 1;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        thisModule = hModule;
        if (HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0)) {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST);
            CloseHandle(mainHandle);
        }
    }
    return TRUE;
}