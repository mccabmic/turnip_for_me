#define _GNU_SOURCE

#include "util.h"

// Globals
pid_t bg_pid[100];
int num_bg_process = 0;

// Signal Globals
volatile bool foreground_mode = false;
volatile bool signaled = false;
int bg_status = 0;

// Shell Prototypes
int sh_built_in(struct command* cmd);

void sh_process();
void sh_status();
void sh_chdir(struct command* cmd);
void sh_launch(struct command* cmd);
void sh_reap();
void sh_fgmode();

// Signal Handler for TSTP
static void sig_stp(int sig){
	signaled = !signaled;
	foreground_mode = !foreground_mode;
}

int main(){
	sh_process();
}

void sh_fgmode(){
	if (!signaled){
		return;
	}
	if(foreground_mode){
		printf("\nEntering foreground-only mode (& is now ignored)\n");
		fflush(stdout);
	}
	else{
		printf("\nExiting foreground-only mode\n");
		fflush(stdout);
	}
}
void sh_reap(){
	// loop through all pids and kill them
	for (int i = 0; i < num_bg_process; i++){
		kill(bg_pid[i], SIGKILL);
	}
}

void sh_process(){
	struct sigaction SIGINT_action = {{0}};
	struct sigaction ignore_action = {{0}};
	struct sigaction SIGSTP_action = {{0}};

	SIGINT_action.sa_flags = 0;
	ignore_action.sa_flags = 0;
	SIGSTP_action.sa_flags = 0;

	SIGSTP_action.sa_handler = &sig_stp;
	SIGINT_action.sa_handler = SIG_DFL;
	ignore_action.sa_handler = SIG_IGN;
	
	while(true){
		sigaction(SIGINT, &ignore_action, NULL);
		sigaction(SIGTSTP, &SIGSTP_action, NULL);		
		
		sh_fgmode();
		if(signaled){
			signaled = false;
		}

		printf(": ");
		fflush(stdout);
		
		char *line = NULL;
		size_t len = BUFFER;
		ssize_t nread;

		while((nread = getline(&line, &len, stdin)) == -1){
			clearerr(stdin);
		}

		line = expand(line, "$$");
		struct command* cmd = parse(line);
		int branch = sh_built_in(cmd);
		//print_command(cmd);
		
		// Exit
		if (branch == 2){
			free(line);
			free(cmd);
			break;
		}
		
		// Unhandled Command
		if (branch == 0){		
			if (foreground_mode){cmd->bg = false;}
			pid_t spawnid;
			spawnid = fork();
			bg_pid[num_bg_process] = spawnid;
			num_bg_process += 1;
			switch(spawnid){
				// Failure to fork
				case -1:
					perror("Fork failed!");
					exit(1);
				break;
				// Child
				case 0:
					sigaction(SIGTSTP, &ignore_action, NULL);
					if(cmd->bg != true){
						sigaction(SIGINT, &SIGINT_action, NULL);
					}
					sh_launch(cmd);
				break;
				// Parent
				default:
					// Child is running in foreground
					if (cmd->bg != true){
						waitpid(spawnid, &bg_status, 0);
						if(WIFSIGNALED(bg_status)){
							sh_status();
						}
					}
					// Child is running in background
					else{
						printf("%s: background process %d has started\n", __FILE__,spawnid);
						fflush(stdout);
						waitpid(spawnid, &bg_status, WNOHANG);
					}
				break;						
			}
		}
		
		free(line);
		free(cmd);
		// REAP THE ZOMBIE CHILDRENZ
		pid_t zombies = waitpid(-1, &bg_status, WNOHANG);
		while (zombies > 0){
			printf("%s: background process %d is done: ", __FILE__,zombies);
			fflush(stdout);
			sh_status();
			zombies = waitpid(-1, &bg_status, WNOHANG);
		}
	} // end while loop
} //end sh process

int sh_built_in (struct command* cmd){
	if(strcmp(cmd->command, "BLANK") == 0 || strcmp(cmd->command, "COMMENT") == 0){
		return 1;
	}

	if(strcmp(cmd->command, "exit") == 0){
		sh_reap();
		return 2;
	}
	
	if(strcmp(cmd->command, "status") == 0 || strcmp(cmd->command, "status &") == 0){ 
		sh_status();
		return 1;
	}

	if(strcmp(cmd->command, "cd") == 0){
		sh_chdir(cmd);
		return 1;
	}

	return 0;
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

void sh_status(){
	if (WIFEXITED(bg_status)){
		printf("%s: exit value: %d\n", __FILE__, WEXITSTATUS(bg_status));
		fflush(stdout);
	}

	else if (WIFSIGNALED(bg_status)){
		printf("%s: terminated by signal %d\n", __FILE__, WTERMSIG(bg_status));
		fflush(stdout);	
	}
}

