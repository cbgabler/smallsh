#include "main.h"

// Globals to handle the flags and buffers
int allowBgProcesses = 1; // by default we want to be able to use BgProcesses
int lastExitStatus = -5;
pid_t bgProcesses[MAX_ARGS]; // buffer for background processes
int bgProcessCounter = 0;
pid_t fgProcessID = -1;

// Handlers for SigINT and SigTSTP
void sigTSTPHandler(int signo) {
    if (allowBgProcesses) {
        printf("Entering foreground-only mode (& is now ignored)\n");
        allowBgProcesses = 0;
    } else {
        printf("Exiting foreground-only mode\n");
        allowBgProcesses = 1;
    }
    fflush(stdout);
}

void sigINTHandler(int signo) {
    if (fgProcessID > 0) {
        kill(fgProcessID, SIGINT);
        printf("terminated by signal %d\n", signo);
        fflush(stdout);
    } else {
        printf("\n");
        fflush(stdout);
    }
}

// cd change directory
void changeDirectory(struct CmdStruct* cmd) {
    if(cmd->args[1] == NULL) { // If there are no args, go home
        chdir(getenv("HOME"));
    } else {
        char* directory = cmd->args[1];
        if(directory[0] == '/') { // Change to the directory if findable
            chdir(directory);
        } else {
            char currDir[1024];
            getcwd(currDir, sizeof(currDir));
            char targetPath[BUF_SIZE];
            snprintf(targetPath, sizeof(targetPath), "%s/%s", currDir, directory);
            chdir(targetPath);
        }
    }
}

// Ignore SIGINT for background processes
void ignoreSIGINTForBg() {
    struct sigaction sigINT_action;
    memset(&sigINT_action, 0, sizeof(sigINT_action)); // Initialize structure

    sigINT_action.sa_handler = SIG_IGN;  // Ignore SIGINT for background processes
    sigfillset(&sigINT_action.sa_mask);   // Block all signals while handling SIGINT
    sigINT_action.sa_flags = 0;           // No special flags

    if (sigaction(SIGINT, &sigINT_action, NULL) == -1) {
        perror("sigaction error");
        exit(1); // Exit if the action is unnatainable
    }
}

// Monitor all processes in the background so the shell knows when they are done
void monitorBgProcesses() {
    for (int i = 0; i < bgProcessCounter; i++) {
        pid_t pid = bgProcesses[i];
        int exitStatus; // Exit status -5 by default
        pid_t result = waitpid(pid, &exitStatus, WNOHANG);

        if(result == 0) { // If no processes
            printf("background pid is %d\n", pid);
            fflush(stdout);
        } else if (result > 0) {
            if(WIFEXITED(exitStatus)) { // If process is exited normally
                lastExitStatus = WEXITSTATUS(exitStatus);
                printf("background pid %d is done: exit value %d\n", pid, lastExitStatus);
                fflush(stdout);
            } else { // If process is killed
                lastExitStatus = WTERMSIG(exitStatus);
                printf("background pid %d is done: terminated by signal %d\n", pid, lastExitStatus);
                fflush(stdout);
            }
            for (int j = i; j < bgProcessCounter - 1; j++) {
                bgProcesses[j] = bgProcesses[j + 1];
            }
            bgProcessCounter--; // If complete, one less process
        }
    }
}

// Function to execute a command (both foreground and background)
void execCmd(struct CmdStruct* cmd) {
    pid_t pid = fork(); // Create a new process

    switch(pid) {
        case -1:
            perror("Error: fork failed");
            exit(1); // Exit if fork fails

        case 0: // Child process
            if(cmd->isBg == 1) { // If background process
                ignoreSIGINTForBg(); // Ignore SIGINT for background processes
            }

            // Handle input redirection
            if(cmd->inputFile) {
                int inputFD = open(cmd->inputFile, O_RDONLY); // Open input file
                if (inputFD == -1) { // If file open fails
                    perror(cmd->inputFile);
                    exit(1); // Exit with error
                }
                dup2(inputFD, 0);
                close(inputFD);
            }

            // Handle output redirection
            if(cmd->outputFile) {
                int outputFD = open(cmd->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644); // Open output file
                if (outputFD == -1) { // If file open fails
                    perror(cmd->outputFile);
                    exit(1); // Exit with error
                }
                dup2(outputFD, 1);
                close(outputFD);
            }

            // Execute the command
            execvp(cmd->cmdName, cmd->args); 
            perror(cmd->cmdName); // If execvp fails
            exit(1);

        default: // Parent process
            if((cmd->isBg == 1) && (allowBgProcesses == 1)) { // If background process and allowed
                bgProcesses[bgProcessCounter] = pid; // Add to background process list
                bgProcessCounter++; // Increment background process counter
            } else { // If foreground process
                fgProcessID = pid;
                waitpid(pid, &lastExitStatus, 0); // Wait for the process to finish
            }
    }
}

// Execute cmds
void execBuiltinOrCmd(struct CmdStruct* cmd) {
    if(strcmp(cmd->args[0], "status") == 0) { // Handle "status" command
        if (lastExitStatus != -5) {
            if (WIFEXITED(lastExitStatus)) { // If last process exited normally
                printf("exit value %d\n", WEXITSTATUS(lastExitStatus));
            } else if (WIFSIGNALED(lastExitStatus)) { // If last process was terminated by signal
                printf("terminated by signal %d\n", WTERMSIG(lastExitStatus));
            }
        } else {
            printf("exit value 0\n"); // If no process has run yet
        }
        fflush(stdout);

    } else if (strcmp(cmd->args[0], "cd") == 0) { // Handle "cd" command
        changeDirectory(cmd);
    } else { // Handle external commands
        execCmd(cmd);
    }
}

// Initialize the CmdStruct with default values
void initializeCmdStruct(struct CmdStruct* cmd) {
    cmd->cmdName = NULL;
    for(int i = 0; i < MAX_ARGS; i++) {
        cmd->args[i] = NULL;
    }
    cmd->inputFile = NULL;
    cmd->outputFile = NULL;
    cmd->isBg = 0;
}

// Function to check and parse input commands
void inputCheck(char input[]) {
    if (input[0] == '#') {
        return;  // Return
    }

    struct CmdStruct* cmd = malloc(sizeof(struct CmdStruct));
    initializeCmdStruct(cmd); // Initialize command structure
    char* token;
    int argCounter = 0;
    char* saveptr;
    token = strtok_r(input, " ", &saveptr); // Tokenize the input string

    while(token) {
        if(strcmp(token, "<") == 0) { // Input redirection
            token = strtok_r(NULL, " ", &saveptr);
            if(token) {
                cmd->inputFile = calloc(strlen(token) + 1, sizeof(char)); // Allocate memory for input file
                strcpy(cmd->inputFile, token);
            }
        } else if (strcmp(token, ">") == 0) { // Output redirection
            token = strtok_r(NULL, " ", &saveptr);
            if(token) {
                cmd->outputFile = calloc(strlen(token) + 1, sizeof(char)); // Allocate memory for output file
                strcpy(cmd->outputFile, token);
            }
        } else if (strcmp(token, "&") == 0) { // Background process
            cmd->isBg = 1;
        } else { // Command arguments
            cmd->args[argCounter] = calloc(strlen(token) + 1, sizeof(char));
            strcpy(cmd->args[argCounter], token); // Store argument
            argCounter++;
        }
        token = strtok_r(NULL, " ", &saveptr);
    }

    if(argCounter > 0) { // If there are arguments, store the command name
        cmd->cmdName = calloc(strlen(cmd->args[0]) + 1, sizeof(char));
        strcpy(cmd->cmdName, cmd->args[0]);
    }
    execBuiltinOrCmd(cmd); // Execute the command
}

// Function to expand $$ to the process ID in the input
void expandPID(char input[]) {
    int expand = 1;
    char expanded[BUF_SIZE];

    while(expand) {
        char* pidLoc = strstr(input, "$$");
        if(pidLoc) {
            char pre[BUF_SIZE] = {0};
            strncpy(pre, input, pidLoc - input);
            char post[BUF_SIZE] = {0};
            strcpy(post, pidLoc + 2);
            snprintf(expanded, sizeof(expanded), "%s%d%s", pre, getpid(), post);
            strcpy(input, expanded); // Update input with expanded PID
        } else {
            expand = 0;
        }
    }
}

int main() {
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = sigTSTPHandler; // Handle SIGTSTP
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = sigINTHandler; // Handle SIGINT
    sigaction(SIGINT, &SIGINT_action, NULL);

    char input[BUF_SIZE]; // Input buffer
    while(1) {
        monitorBgProcesses(); // Monitor background processes
        printf(": ");
        fflush(stdout);

        if(fgets(input, sizeof(input), stdin) != NULL) {
            input[strlen(input) - 1] = '\0'; // Remove newline
            expandPID(input); // Expand $$ if present
            inputCheck(input); // Check and parse input
        } else {
            if(feof(stdin)) {
                break;
            }
        }
    }
    return 0;
}
