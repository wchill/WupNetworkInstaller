#include <coreinit/mcp.h>
#include <coreinit/energysaver.h>
#include <coreinit/mutex.h>
#include <dirent.h>
#include <cstring>
#include <string>
#include <sys/unistd.h>
#include <malloc.h>
#include "Installer.h"

static void mcpInstallCallback(MCPError err, void *rawData)
{
    auto *data = (Installer *)rawData;
    data->lastErr = err;
    data->processing = false;
}

void removeDirectory(std::string &path)
{
    if (path[path.length() - 1] != '/') {
        path += '/';
    }
    DIR *dir = opendir(path.c_str());
    if(dir != nullptr)
    {
        for(struct dirent *entry = readdir(dir); entry != nullptr; entry = readdir(dir))
        {
            std::string newPath(path);
            newPath += entry->d_name;

            if(entry->d_type & DT_DIR)
                removeDirectory(newPath);
            else
            {
                remove(newPath.c_str());
            }
        }
        closedir(dir);
        remove(path.c_str());
    }
}

static void removeFolder(const std::string &path) {
    DIR *dir = opendir(path.c_str());
    if (dir != nullptr) {
        for (struct dirent *entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
            if(entry->d_name[0] == '.' || !(entry->d_type & DT_DIR) || strlen(entry->d_name) != 8)
                continue;

            std::string fullPath(path);
            fullPath.append(entry->d_name);
            removeDirectory(fullPath);
        }
    }
}

static void cleanupCancelledInstallation() {
    const char *importPaths[] = {
            "/vol/storage_usb01/usr/import/",
            "/vol/storage_mlc01/usr/import/"
    };
    for (auto & importPath : importPaths) {
        removeFolder(importPath);
    }
}

Installer::Installer() : processing(false) {
    int handle = MCP_Open();
    if (handle == 0) {
        return;
    }

    info = reinterpret_cast<MCPInstallTitleInfoUnion *>(memalign(0x40, sizeof(MCPInstallTitleInfo)));
    installProgress = reinterpret_cast<MCPInstallProgress *>(memalign(0x40, sizeof(MCPInstallProgress)));
    installLock = reinterpret_cast<OSMutex *>(memalign(0x40, sizeof(OSMutex)));
    OSInitMutex(installLock);

    mcpHandle = handle;
    lastErr = 0;
    initialized = true;
}

Installer::~Installer() {
    if (initialized) {
        initialized = false;
        processing = false;
        MCP_Close(mcpHandle);
        free(info);
        free(installProgress);
        free(installLock);
    }
}

void Installer::cancel() {
    MCP_InstallTitleAbort(mcpHandle);
    lastErr = MCP_INSTALL_CANCELLED;
}

int Installer::install(MCPInstallTarget target, const std::string &wupPath, void (*installProgressCallback)(MCPInstallProgress*)) {
    if (!initialized) {
        return -1;
    }

    OSLockMutex(installLock);
    IMDisableAPD();
    lastErr = MCP_InstallGetInfo(mcpHandle, wupPath.c_str(), &info->info);
    if (lastErr != 0) {
        // 0xfffbf3e2 - no title.tmd
        // 0xfffbfc17 - internal error
        // everything else - ???
        goto cleanup;
    }
    lastErr = MCP_InstallSetTargetDevice(mcpHandle, target);
    if (lastErr != 0) {
        // Error setting installation destination
        goto cleanup;
    }

    info->mcpCallback = mcpInstallCallback;
    info->installer = this;
    processing = true;

    lastErr = MCP_InstallTitleAsync(mcpHandle, wupPath.c_str(), &info->titleInfo);
    if (lastErr != 0) {
        // Error starting async installation
        goto cleanup;
    }

    // Wait for install to finish
    while (processing) {
        lastErr = MCP_InstallGetProgress(mcpHandle, installProgress);
        if (lastErr == 0) {
            if (installProgressCallback != nullptr) {
                installProgressCallback(installProgress);
            }
            usleep(50000);
        }
    }

    if (lastErr != 0) {
        // Error during installation
        switch ((unsigned int) lastErr) {
            case 0xDEAD0002:
                // cancelled
                cleanupCancelledInstallation();
                goto cleanup;
            case 0xFFFBF43F:
            case 0xFFFBF446:
                // Possible missing or bad title.tik file
            case 0xFFFBF441:
                // Possible incorrect console for DLC title.tik file
            case 0xFFFCFFE4:
                // Not enough free space on target device
            case 0xFFFCFFE9:
                // if it's DLC, main game might be missing
                // else it's an error with the drive
            case 0xFFFFF825:
            case 0xFFFFF82E:
                // Files might be corrupt or bad storage medium
                goto cleanup;
            default:
                if ((lastErr & 0xFFFF0000) == 0xFFFB0000) {
                    // Either USB drive failure or corrupt files
                }
                else {
                    // unknown error
                }
                goto cleanup;
        }
    }
    lastErr = 0;
    removeFolder(wupPath);

    cleanup:
    IMEnableAPD();
    OSUnlockMutex(installLock);

    // install finished. Do any cleanup needed and then exit
    return lastErr;
}