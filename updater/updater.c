/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <cutils/properties.h>

#include "edify/expr.h"
#include "updater.h"
#include "install.h"
#include "blockimg.h"
#include "minzip/Zip.h"
#include "minzip/SysUtil.h"

// Generated by the makefile, this function defines the
// RegisterDeviceExtensions() function, which calls all the
// registration functions for device-specific extensions.
#include "register.inc"

// Where in the package we expect to find the edify script to execute.
// (Note it's "updateR-script", not the older "update-script".)
#define SCRIPT_NAME "META-INF/com/google/android/updater-script"
// golden/janice unification
#define UNIFIED_BUILD

struct selabel_handle *sehandle;

int main(int argc, char** argv) {
    // Various things log information to stdout or stderr more or less
    // at random (though we've tried to standardize on stdout).  The
    // log file makes more sense if buffering is turned off so things
    // appear in the right order.
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    if (argc != 4) {
        printf("unexpected number of arguments (%d)\n", argc);
        return 1;
    }

    char* version = argv[1];
    if ((version[0] != '1' && version[0] != '2' && version[0] != '3') ||
        version[1] != '\0') {
        // We support version 1, 2, or 3.
        printf("wrong updater binary API; expected 1, 2, or 3; "
                        "got %s\n",
                argv[1]);
        return 2;
    }

    // Set up the pipe for sending commands back to the parent process.

    int fd = atoi(argv[2]);
    FILE* cmd_pipe = fdopen(fd, "wb");
    setlinebuf(cmd_pipe);

    // Extract the script from the package.

    const char* package_filename = argv[3];
    MemMapping map;
    if (sysMapFile(package_filename, &map) != 0) {
        printf("failed to map package %s\n", argv[3]);
        return 3;
    }
    ZipArchive za;
    int err;
    err = mzOpenZipArchive(map.addr, map.length, &za);
    if (err != 0) {
        printf("failed to open package %s: %s\n",
               argv[3], strerror(err));
        return 3;
    }

#ifndef UNIFIED_BUILD
    const ZipEntry* script_entry = mzFindZipEntry(&za, SCRIPT_NAME);
    if (script_entry == NULL) {
        printf("failed to find %s in %s\n", SCRIPT_NAME, package_filename);
        return 4;
    }
#else
    // deviceName - the name of the device (eg. golden)
    char deviceName[PROPERTY_VALUE_MAX];
    property_get("ro.product.device", deviceName, "");
	
    // scriptName - due to golden and janice having different partition layouts
    // (eg. /system is mmcblk0p22 on golden and mmcblk0p3 on janice), we need to have
    // different updater scripts for the two devices.
    char scriptName[PROPERTY_VALUE_MAX];
    if (!strcmp(deviceName, "codina")) strcpy(scriptName, "META-INF/com/google/android/updater-script-janice"); // janice will use updater-script-janice
    else strcpy(scriptName, "META-INF/com/google/android/updater-script"); // while golden will use updater-script
	
    // print out the device name for debug purposes
    printf("%s device detected", deviceName);
	
    const ZipEntry* script_entry = mzFindZipEntry(&za, scriptName);
    if (script_entry == NULL) {
        printf("failed to find %s in %s\n", scriptName, package_filename);
        return 4;
    }
#endif

    char* script = malloc(script_entry->uncompLen+1);
    if (!mzReadZipEntry(&za, script_entry, script, script_entry->uncompLen)) {
        printf("failed to read script from package\n");
        return 5;
    }
    script[script_entry->uncompLen] = '\0';

    // Configure edify's functions.

    RegisterBuiltins();
    RegisterInstallFunctions();
    RegisterBlockImageFunctions();
    RegisterDeviceExtensions();
    FinishRegistration();

    // Parse the script.

    Expr* root;
    int error_count = 0;
    int error = parse_string(script, &root, &error_count);
    if (error != 0 || error_count > 0) {
        printf("%d parse errors\n", error_count);
        return 6;
    }

    struct selinux_opt seopts[] = {
      { SELABEL_OPT_PATH, "/file_contexts" }
    };

    sehandle = selabel_open(SELABEL_CTX_FILE, seopts, 1);

    if (!sehandle) {
        fprintf(cmd_pipe, "ui_print Warning: No file_contexts\n");
    }

    // Evaluate the parsed script.

    UpdaterInfo updater_info;
    updater_info.cmd_pipe = cmd_pipe;
    updater_info.package_zip = &za;
    updater_info.version = atoi(version);
    updater_info.package_zip_addr = map.addr;
    updater_info.package_zip_len = map.length;

    State state;
    state.cookie = &updater_info;
    state.script = script;
    state.errmsg = NULL;

    char* result = Evaluate(&state, root);
    if (result == NULL) {
        if (state.errmsg == NULL) {
            printf("script aborted (no error message)\n");
            fprintf(cmd_pipe, "ui_print script aborted (no error message)\n");
        } else {
            printf("script aborted: %s\n", state.errmsg);
            char* line = strtok(state.errmsg, "\n");
            while (line) {
                fprintf(cmd_pipe, "ui_print %s\n", line);
                line = strtok(NULL, "\n");
            }
            fprintf(cmd_pipe, "ui_print\n");
        }
        free(state.errmsg);
        return 7;
    } else {
        fprintf(cmd_pipe, "ui_print script succeeded: result was [%s]\n", result);
        free(result);
    }

    if (updater_info.package_zip) {
        mzCloseZipArchive(updater_info.package_zip);
    }
    sysReleaseMap(&map);
    free(script);

    return 0;
}

