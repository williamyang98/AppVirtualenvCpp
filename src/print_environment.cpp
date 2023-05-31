#include <stdio.h>
#include <stdlib.h>

#include <shlobj.h>
#include <objbase.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

void print_from_environment_variables(char** envp);
void print_from_registry();

// NOTE: Environment variables passed to the process via entry point on MSVC
int main(int argc, char** argv, char** envp) {
    printf("[SOURCE]: environment_variables\n");
    print_from_environment_variables(envp);
    printf("\n\n");

    printf("[SOURCE]: registry api\n");
    print_from_registry();
    printf("\n\n");
    return 0;
}

void print_from_environment_variables(char** envp) {
    for (int i = 0;; i++) {
        const char *env = envp[i];
        if (env == NULL) {
            break;
        }
        printf("%s\n", env);
    }
}

bool print_folder_from_registry(REFKNOWNFOLDERID folder_id) {
    PWSTR path = NULL;
    const HRESULT r = SHGetKnownFolderPath(folder_id, KF_FLAG_CREATE, NULL, &path);
    if (path == NULL) {
        return false;
    }
    printf("%ls", path);
    CoTaskMemFree(path);
    return true;
}

#define PRINT_FOLDER_FROM_REGISTRY(ID) { \
    printf("%s=", #ID); \
    print_folder_from_registry(ID); \
    printf("\n"); \
}

void print_from_registry() {
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_Profile);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_Favorites);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_Desktop);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_Documents);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_Music);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_Pictures);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_SavedGames);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_Videos);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_RoamingAppData);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_RecycleBinFolder);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_CommonStartup);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_ProgramData);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_PublicDesktop);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_PublicDocuments);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_ProgramFiles);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_ProgramFilesX86);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_ProgramFilesCommon);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_ProgramFilesCommonX86);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_Programs);
    PRINT_FOLDER_FROM_REGISTRY(FOLDERID_Windows);
}