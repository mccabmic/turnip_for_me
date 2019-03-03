#ifndef UTILITIES_H
#define UTILITIES_H

#define BUFFER 2048
#define NAME_MAX 255
#define MAX_ARGS 256
#define PID_LIM 80

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>

struct command {
	char command[NAME_MAX];
	char* args[MAX_ARGS];
	int argc;
	char input[NAME_MAX];
	char output[NAME_MAX];
	bool bg;
};

struct command* parse(char* string);
void print_command(struct command*);
char* expand(char* string, char* pattern);

#endif 