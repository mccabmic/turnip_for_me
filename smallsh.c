#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define BUFFER 2048
#define NAME_MAX 255
#define MAX_ARGS 256
#define PID_LIM 80

struct command {
	char command[NAME_MAX];
	char* args[MAX_ARGS];
	int argc;
	char input[NAME_MAX];
	char output[NAME_MAX];
	bool bg;
};

void shell_process();
struct command* parse(char* string);
void prompt();
int status();
bool cd(char* path);
int sh_exit();
void print_command(struct command*);
void sh_status(int* status);
int handle_command(struct command* cmd);
void sh_chdir(struct command* cmd);
void sh_launch(struct command* cmd);
char* expand(char* string, char* pattern);

int main(){
	shell_process();
}


void shell_process(){
	/* listen for commands
	* when a non-built in command is received, fork()
	* this child will do I/O redirection before execing the command given	
	* when handling redirection ,just pass ls into exec, so get rid of redir symbol and des/source
	*/
	
	int bg_status = 0;
	bool loop = true;
	while(loop){
		// Shell Variables
		struct sigaction SIGINT_action, ignore_action = {0};
		
		SIGINT_action.sa_handler = SIG_DFL;
		ignore_action.sa_handler = SIG_IGN;

		sigaction(SIGINT, &ignore_action, NULL);
		
		// Prompt Variables
		char *line = NULL;
		size_t len = BUFFER;
		ssize_t nread;
		
		printf(": ");
		fflush(stdout);
		while((nread = getline(&line, &len, stdin)) == -1){
			clearerr(stdin);
		}

		line = expand(line, "$$");
		
		// Build-in function, exit
		if (strcmp(line, "exit\n") == 0){
			free(line);
			break;
		}

		// Built-in function status
		else if (strcmp(line, "status\n") == 0 || strcmp(line, "status &\n") == 0){
			sh_status(&bg_status);	
		}

		else{
			struct command* cmd = parse(line);
			if(handle_command(cmd)){
				pid_t spawnid;
				spawnid = fork();
				switch(spawnid){
					case -1:
						perror("Fork failed!");
						exit(1); break;
					case 0:
						 sigaction(SIGINT, &SIGINT_action, NULL);
						 sh_launch(cmd);
						 break;
					default:
					 	if (cmd->bg != true){
							waitpid(spawnid, &bg_status, 0);
							if(WIFSIGNALED(bg_status)){
								sh_status(&bg_status);
							}
						}
						else{
							printf("background process %d has started\n", spawnid);
							fflush(stdout);
							waitpid(spawnid, &bg_status, WNOHANG);
						}
						break;						
				}
			}
			free(cmd); // what if parent frees command before child gets to it?
		}

		// REAP THE ZOMBIE CHILDRENZ
		pid_t zombies = waitpid(-1, &bg_status, WNOHANG);
		while (zombies > 0){
			printf("background process %d is done: ", zombies);
			fflush(stdout);
			sh_status(&bg_status);
			zombies = waitpid(-1, &bg_status, WNOHANG);
		}
		free(line);
	}
}
void sh_launch(struct command* cmd){
	int result, input, output;
	if (strcmp(cmd->input, "") != 0){
		if((input = open(cmd->input, O_RDONLY)) == -1){
			fprintf(stderr, "%s: Unable to open input file\n", __FILE__);
			exit(1);
		}
		if ((result = dup2(input, 0)) == -1){
			perror("dup2 error");
			exit(1);
		}
		fcntl(input, F_SETFD, FD_CLOEXEC);
	}
	
	if (strcmp(cmd->output, "") != 0){
		if ((output = open(cmd->output, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1){
			fprintf(stderr, "%s: Unable to open output file\n", __FILE__);
			exit(1);
		}
		if ((result = dup2(output, 1)) == -1){
			perror("dup2 error");
			exit(1);
		}
		fcntl(output, F_SETFD, FD_CLOEXEC);
	}

	if (execvp(cmd->args[0], cmd->args)){
		printf("%s: Invalid command: \"%s\"\n", __FILE__, cmd->args[0]);
		exit(1);
	}

}

int handle_command (struct command* cmd){
	if(strcmp(cmd->command, "BLANK") == 0 || strcmp(cmd->command, "COMMENT") == 0){
		return 0;
	}
	
	if (strcmp(cmd->command, "cd") == 0){
		sh_chdir(cmd);
		return 0;
	}
	return 1;
}

void sh_chdir(struct command* cmd){
	if (cmd->argc == 1){
		chdir(getenv("HOME"));
	}
	else if (cmd->argc == 2){
		if (!chdir(cmd->args[1]) == 0){
			perror("cd");
		}	
	}
	else{
		return;
	}
}

void sh_status(int* status){
	if (WIFEXITED(*status)){
		printf("exit value: %d\n", WEXITSTATUS(*status));
		fflush(stdout);
	}

	else if (WIFSIGNALED(*status)){
		printf("terminated by signal %d\n", WTERMSIG(*status));
		fflush(stdout);	
	}

	else {
		printf("unknown error in __FILE__\n");
		fflush(stdout);
	}
}

void print_command(struct command* cmd){
	printf("command: %s\n", cmd->command);
	printf("output: %s\n", cmd->output);
	printf("input: %s\n", cmd->input);
	printf("background: %d\n", cmd->bg);
	printf("args: "); for (int i = 0; cmd->args[i] != NULL; i++) printf("%s ", cmd->args[i]);
	printf("\n");
}

//https://stackoverflow.com/questions/779875/what-is-the-function-to-replace-string-in-c
char* expand(char* string, char* pattern){
	
	char pid_string[PID_LIM] = {0};
	sprintf(pid_string, "%ld", (long)getpid());

	size_t new_string_size = strlen(string) + 1;
	char* new_string = malloc(new_string_size);
	size_t offset = 0;
	
	char* delim;
	char* in = string;
	
	while (delim = strstr(in, pattern)){
		memcpy(new_string + offset, in, delim - in);
		offset += delim - in;

		in = delim + strlen(pattern);
		
		new_string_size = new_string_size - strlen(pattern) + strlen(pid_string);
		new_string = realloc(new_string, new_string_size);

		memcpy(new_string + offset, pid_string, strlen(pid_string));
		offset += strlen(pid_string);
	}

	strcpy(new_string + offset, in);
	
	free(string);
	return new_string;
		
}

struct command* parse(char string[BUFFER]){
	const char delimit[] = " \t\r\n\v\f";
	struct command* cmd = malloc(sizeof(struct command));
	if (string[0] == '\n'){
		snprintf(cmd->command, sizeof(cmd->command), "BLANK");
		return cmd;
	}
	else if (string[0] == '#'){
		snprintf(cmd->command, sizeof(cmd->command), "COMMENT");
		return cmd;
	}

	// Init Struct
	snprintf(cmd->input, sizeof(cmd->input), "");
	snprintf(cmd->output, sizeof(cmd->output), "");
	cmd->bg = false;
	
	char* token;
	// Set command
	token = strtok(string, delimit);
	snprintf(cmd->command, sizeof(cmd->command), token);
	
	size_t arg_count = 0;
	// Start setting loop
	while(token != NULL){
		if (strcmp(token, "<") == 0){
			snprintf(cmd->input, sizeof(cmd->input), strtok(NULL, delimit));
		}
		else if (strcmp(token, ">") == 0){
			snprintf(cmd->output, sizeof(cmd->output), strtok(NULL, delimit));
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
			snprintf(cmd->input, sizeof(cmd->input), "/dev/null");
		}
		if(strcmp(cmd->output, "") == 0){
			snprintf(cmd->output, sizeof(cmd->output), "/dev/null");
		}
	
	}
	cmd->args[arg_count] = NULL;
	return cmd;
}

