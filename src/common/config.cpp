// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fstream>
#include <string>
#include <fmt/core.h>
#include <toml.hpp>
#include "config.h"

namespace Config {

static bool isNeo = false;
static bool isFullscreen = false;
static u32 screenWidth = 1280;
static u32 screenHeight = 720;
static s32 gpuId = -1; // Vulkan physical device index. Set to negative for auto select
static std::string logFilter;
static std::string logType = "sync";
static bool isDebugDump = false;
static bool isLibc = true;
static bool isShowSplash = true;
static bool isNullGpu = false;
static bool shouldDumpShaders = false;
static bool shouldDumpPM4 = false;
static bool vkValidation = false;
static bool vkValidationSync = false;
// Gui
std::string settings_install_dir = "";
u32 main_window_geometry_x = 400;
u32 main_window_geometry_y = 400;
u32 main_window_geometry_w = 1280;
u32 main_window_geometry_h = 720;
u32 mw_themes = 0;
u32 m_icon_size = 36;
u32 m_icon_size_grid = 69;
u32 m_slider_pos = 0;
u32 m_slider_pos_grid = 0;
u32 m_table_mode = 0;
u32 m_window_size_W = 1280;
u32 m_window_size_H = 720;
std::vector<std::string> m_pkg_viewer;
std::vector<std::string> m_elf_viewer;
std::vector<std::string> m_recent_files;

bool isLleLibc() {
    return isLibc;
}

bool isNeoMode() {
    return isNeo;
}

bool isFullscreenMode() {
    return isFullscreen;
}

u32 getScreenWidth() {
    return screenWidth;
}

u32 getScreenHeight() {
    return screenHeight;
}

s32 getGpuId() {
    return gpuId;
}

std::string getLogFilter() {
    return logFilter;
}

std::string getLogType() {
    return logType;
}

bool debugDump() {
    return isDebugDump;
}

bool showSplash() {
    return isShowSplash;
}

bool nullGpu() {
    return isNullGpu;
}

bool dumpShaders() {
    return shouldDumpShaders;
}

bool dumpPM4() {
    return shouldDumpPM4;
}

bool vkValidationEnabled() {
    return vkValidation;
}

bool vkValidationSyncEnabled() {
    return vkValidationSync;
}

void setMainWindowGeometry(u32 x, u32 y, u32 w, u32 h) {
    main_window_geometry_x = x;
    main_window_geometry_y = y;
    main_window_geometry_w = w;
    main_window_geometry_h = h;
}
void setGameInstallDir(const std::string& dir) {
    settings_install_dir = dir;
}
void setMainWindowTheme(u32 theme) {
    mw_themes = theme;
}
void setIconSize(u32 size) {
    m_icon_size = size;
}
void setIconSizeGrid(u32 size) {
    m_icon_size_grid = size;
}
void setSliderPositon(u32 pos) {
    m_slider_pos = pos;
}
void setSliderPositonGrid(u32 pos) {
    m_slider_pos_grid = pos;
}
void setTableMode(u32 mode) {
    m_table_mode = mode;
}
void setMainWindowWidth(u32 width) {
    m_window_size_W = width;
}
void setMainWindowHeight(u32 height) {
    m_window_size_H = height;
}
void setPkgViewer(std::vector<std::string> pkgList) {
    m_pkg_viewer.resize(pkgList.size());
    m_pkg_viewer = pkgList;
}
void setElfViewer(std::vector<std::string> elfList) {
    m_elf_viewer.resize(elfList.size());
    m_elf_viewer = elfList;
}
void setRecentFiles(std::vector<std::string> recentFiles) {
    m_recent_files.resize(recentFiles.size());
    m_recent_files = recentFiles;
}

u32 getMainWindowGeometryX() {
    return main_window_geometry_x;
}
u32 getMainWindowGeometryY() {
    return main_window_geometry_y;
}
u32 getMainWindowGeometryW() {
    return main_window_geometry_w;
}
u32 getMainWindowGeometryH() {
    return main_window_geometry_h;
}
std::string getGameInstallDir() {
    return settings_install_dir;
}
u32 getMainWindowTheme() {
    return mw_themes;
}
u32 getIconSize() {
    return m_icon_size;
}
u32 getIconSizeGrid() {
    return m_icon_size_grid;
}
u32 getSliderPositon() {
    return m_slider_pos;
}
u32 getSliderPositonGrid() {
    return m_slider_pos_grid;
}
u32 getTableMode() {
    return m_table_mode;
}
u32 getMainWindowWidth() {
    return m_window_size_W;
}
u32 getMainWindowHeight() {
    return m_window_size_H;
}
std::vector<std::string> getPkgViewer() {
    return m_pkg_viewer;
}
std::vector<std::string> getElfViewer() {
    return m_elf_viewer;
}
std::vector<std::string> getRecentFiles() {
    return m_recent_files;
}

void load(const std::filesystem::path& path) {
    // If the configuration file does not exist, create it and return
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        save(path);
        return;
    }

    toml::value data;

    try {
        data = toml::parse(path);
    } catch (std::exception& ex) {
        fmt::print("Got exception trying to load config file. Exception: {}\n", ex.what());
        return;
    }

    if (data.contains("General")) {
        auto generalResult = toml::expect<toml::value>(data.at("General"));
        if (generalResult.is_ok()) {
            auto general = generalResult.unwrap();

            isNeo = toml::find_or<toml::boolean>(general, "isPS4Pro", false);
            isFullscreen = toml::find_or<toml::boolean>(general, "Fullscreen", true);
            logFilter = toml::find_or<toml::string>(general, "logFilter", "");
            logType = toml::find_or<toml::string>(general, "logType", "sync");
            isShowSplash = toml::find_or<toml::boolean>(general, "showSplash", true);
        }
    }
    if (data.contains("GPU")) {
        auto gpuResult = toml::expect<toml::value>(data.at("GPU"));
        if (gpuResult.is_ok()) {
            auto gpu = gpuResult.unwrap();

            screenWidth = toml::find_or<toml::integer>(gpu, "screenWidth", screenWidth);
            screenHeight = toml::find_or<toml::integer>(gpu, "screenHeight", screenHeight);
            gpuId = toml::find_or<toml::integer>(gpu, "gpuId", 0);
            isNullGpu = toml::find_or<toml::boolean>(gpu, "nullGpu", false);
            shouldDumpShaders = toml::find_or<toml::boolean>(gpu, "dumpShaders", false);
            shouldDumpPM4 = toml::find_or<toml::boolean>(gpu, "dumpPM4", false);
        }
    }
    if (data.contains("Vulkan")) {
        const auto vkResult = toml::expect<toml::value>(data.at("Vulkan"));
        if (vkResult.is_ok()) {
            auto vk = vkResult.unwrap();

            vkValidation = toml::find_or<toml::boolean>(vk, "validation", true);
            vkValidationSync = toml::find_or<toml::boolean>(vk, "validation_sync", true);
        }
    }
    if (data.contains("Debug")) {
        auto debugResult = toml::expect<toml::value>(data.at("Debug"));
        if (debugResult.is_ok()) {
            auto debug = debugResult.unwrap();

            isDebugDump = toml::find_or<toml::boolean>(debug, "DebugDump", false);
        }
    }
    if (data.contains("LLE")) {
        auto lleResult = toml::expect<toml::value>(data.at("LLE"));
        if (lleResult.is_ok()) {
            auto lle = lleResult.unwrap();

            isLibc = toml::find_or<toml::boolean>(lle, "libc", true);
        }
    }
    if (data.contains("GUI")) {
        auto guiResult = toml::expect<toml::value>(data.at("GUI"));
        if (guiResult.is_ok()) {
            auto gui = guiResult.unwrap();

            m_icon_size = toml::find_or<toml::integer>(gui, "iconSize", 0);
            m_icon_size_grid = toml::find_or<toml::integer>(gui, "iconSizeGrid", 0);
            m_slider_pos = toml::find_or<toml::integer>(gui, "sliderPos", 0);
            m_slider_pos_grid = toml::find_or<toml::integer>(gui, "sliderPosGrid", 0);
            mw_themes = toml::find_or<toml::integer>(gui, "theme", 0);
            m_window_size_W = toml::find_or<toml::integer>(gui, "mw_width", 0);
            m_window_size_H = toml::find_or<toml::integer>(gui, "mw_height", 0);
            settings_install_dir = toml::find_or<toml::string>(gui, "installDir", "");
            main_window_geometry_x = toml::find_or<toml::integer>(gui, "geometry_x", 0);
            main_window_geometry_y = toml::find_or<toml::integer>(gui, "geometry_y", 0);
            main_window_geometry_w = toml::find_or<toml::integer>(gui, "geometry_w", 0);
            main_window_geometry_h = toml::find_or<toml::integer>(gui, "geometry_h", 0);
            m_pkg_viewer = toml::find_or<std::vector<std::string>>(gui, "pkgDirs", {});
            m_elf_viewer = toml::find_or<std::vector<std::string>>(gui, "elfDirs", {});
            m_recent_files = toml::find_or<std::vector<std::string>>(gui, "recentFiles", {});
            m_table_mode = toml::find_or<toml::integer>(gui, "gameTableMode", 0);
        }
    }
}
void save(const std::filesystem::path& path) {
    toml::basic_value<toml::preserve_comments> data;

    std::error_code error;
    if (std::filesystem::exists(path, error)) {
        try {
            data = toml::parse<toml::preserve_comments>(path);
        } catch (const std::exception& ex) {
            fmt::print("Exception trying to parse config file. Exception: {}\n", ex.what());
            return;
        }
    } else {
        if (error) {
            fmt::print("Filesystem error accessing {} (error: {})\n", path.string(),
                       error.message().c_str());
        }
        fmt::print("Saving new configuration file {}\n", path.string());
    }

    data["General"]["isPS4Pro"] = isNeo;
    data["General"]["Fullscreen"] = isFullscreen;
    data["General"]["logFilter"] = logFilter;
    data["General"]["logType"] = logType;
    data["General"]["showSplash"] = isShowSplash;
    data["GPU"]["gpuId"] = gpuId;
    data["GPU"]["screenWidth"] = screenWidth;
    data["GPU"]["screenHeight"] = screenHeight;
    data["GPU"]["nullGpu"] = isNullGpu;
    data["GPU"]["dumpShaders"] = shouldDumpShaders;
    data["GPU"]["dumpPM4"] = shouldDumpPM4;
    data["Vulkan"]["validation"] = vkValidation;
    data["Vulkan"]["validation_sync"] = vkValidationSync;
    data["Debug"]["DebugDump"] = isDebugDump;
    data["LLE"]["libc"] = isLibc;
    data["GUI"]["theme"] = mw_themes;
    data["GUI"]["iconSize"] = m_icon_size;
    data["GUI"]["sliderPos"] = m_slider_pos;
    data["GUI"]["iconSizeGrid"] = m_icon_size_grid;
    data["GUI"]["sliderPosGrid"] = m_slider_pos_grid;
    data["GUI"]["gameTableMode"] = m_table_mode;
    data["GUI"]["mw_width"] = m_window_size_W;
    data["GUI"]["mw_height"] = m_window_size_H;
    data["GUI"]["installDir"] = settings_install_dir;
    data["GUI"]["geometry_x"] = main_window_geometry_x;
    data["GUI"]["geometry_y"] = main_window_geometry_y;
    data["GUI"]["geometry_w"] = main_window_geometry_w;
    data["GUI"]["geometry_h"] = main_window_geometry_h;
    data["GUI"]["pkgDirs"] = m_pkg_viewer;
    data["GUI"]["elfDirs"] = m_elf_viewer;
    data["GUI"]["recentFiles"] = m_recent_files;

    std::ofstream file(path, std::ios::out);
    file << data;
    file.close();
}
} // namespace Config
