#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_ARGS 512
#define BUF_SIZE 2048

extern int allowBgProcesses;
extern int lastExitStatus;
extern pid_t bgProcesses[MAX_ARGS];
extern int bgProcessCounter;
extern pid_t fgProcessID;

struct CmdStruct {
    char* cmdName;
    char* args[MAX_ARGS];
    char* inputFile;
    char* outputFile;
    int isBg;
};

void sigTSTPHandler(int signo);
void sigINTHandler(int signo);
void changeDirectory(struct CmdStruct* cmd);
void ignoreSIGINTForBg();
void monitorBgProcesses();
void execCmd(struct CmdStruct* cmd);
void execBuiltinOrCmd(struct CmdStruct* cmd);
void initializeCmdStruct(struct CmdStruct* cmd);
void inputCheck(char input[]);
void expandPID(char input[]);
int processCheck(char input[]);
void setUpSignalHandlers();

#endif

