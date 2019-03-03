#include "util.h"

// Parse and create the cmd structure
struct command* parse(char string[BUFFER]){
	const char delimit[] = " \t\r\n\v\f";
	struct command* cmd = malloc(sizeof(struct command));

	if (string[0] == '\n'){
		memcpy(cmd->command, "BLANK", sizeof(cmd->command));
		return cmd;
	}
	else if (string[0] == '#'){
		memcpy(cmd->command, "COMMENT", sizeof(cmd->command));
		return cmd;
	}

	// Init Struct
	sprintf(cmd->input, "%s", "");
	sprintf(cmd->output, "%s", "");
	cmd->bg = false;
	
	char* token;
	// Set command
	token = strtok(string, delimit);
	strncpy(cmd->command, token, sizeof(cmd->command));
	
	size_t arg_count = 0;
	// Start setting loop
	while(token != NULL){
		if (strcmp(token, "<") == 0){
			strncpy(cmd->input, strtok(NULL, delimit), sizeof(cmd->input));
		}
		else if (strcmp(token, ">") == 0){
			strncpy(cmd->output, strtok(NULL, delimit), sizeof(cmd->output));
		}
		else{
			cmd->args[arg_count] = token;
			arg_count += 1;
		}
		token = strtok(NULL, delimit);
	}
	cmd->argc = arg_count;
	if(strcmp(cmd->args[arg_count - 1],"&") == 0){
		cmd->args[arg_count -1] = '\0';
		cmd->bg = true;
		if(strcmp(cmd->input, "") == 0){
			memcpy(cmd->input, "/dev/null", sizeof(cmd->input));
		}
		if(strcmp(cmd->output, "") == 0){
			memcpy(cmd->output,  "/dev/null", sizeof(cmd->output));
		}
	}
	cmd->args[arg_count] = NULL;
	return cmd;
}

// Print a command, used in debugging.
void print_command(struct command* cmd){
	printf("command: %s\n", cmd->command);
	printf("output: %s\n", cmd->output);
	printf("input: %s\n", cmd->input);
	printf("background: %d\n", cmd->bg);
	printf("args: "); for (int i = 0; cmd->args[i] != NULL; i++) printf("%s ", cmd->args[i]);
	printf("\n");
}

// Code for expanding $$
// Source: https://stackoverflow.com/questions/779875/what-is-the-function-to-replace-string-in-c
// Made some modifications to this so it doesn't create a memory leak
char* expand(char* string, char* pattern){
	
	char pid_string[PID_LIM] = {0};
	sprintf(pid_string, "%ld", (long)getpid());

	size_t new_string_size = strlen(string) + 1;
	
	// Create a blank string
	char* new_string = malloc(new_string_size);
	size_t offset = 0;
	
	char* delim;
	
	// Keep track of where the original string is in memory
	char* in = string;
	
	// While $$ exists in the string
	while ((delim = strstr(in, pattern))){
		
		// Copy everything up to the $$
		memcpy(new_string + offset, in, delim - in);
		offset += delim - in;
		
		// Move forward to after the $$ found
		in = delim + strlen(pattern);
		
		// Reallocate the string size to fit the pid
		new_string_size = new_string_size - strlen(pattern) + strlen(pid_string);
		new_string = realloc(new_string, new_string_size);
		
		// Copy in the pid
		memcpy(new_string + offset, pid_string, strlen(pid_string));
		offset += strlen(pid_string);
	}
	
	// Copy in the end of the string
	strcpy(new_string + offset, in);
	
	// Free the old string
	free(string);
	return new_string;		
}

