#include <stdio.h>
#include <stdlib.h>

// Prints out the environment variables passed to the process
int main(int argc, char **argv, char **envp) {
    for (int i = 0;; i++) {
        const char *env = envp[i];
        if (env == NULL) {
            break;
        }
        printf("%s\n", env);
    }
    return 0;
}
