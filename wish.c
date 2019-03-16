#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define ARRAY_LENGTH 1024

struct CmdSpecs {
	char cmd[ARRAY_LENGTH];
	char *args[ARRAY_LENGTH];
};

char *globalPath[ARRAY_LENGTH] = { 0 };

void exitWithError(void)
{
	char *message = "An error has occurred\n";
	fwrite(message, strlen(message), 1, stdout); 
	exit(1);
}

void printPrompt(void)
{
	char prompt[] = "wish> ";
	fwrite(prompt, strlen(prompt), 1, stdout);
}

void freeParsedLineArgs(struct CmdSpecs *cmdSpecs)
{
	for(int i = 0; (cmdSpecs->args[i] != NULL) && (i < ARRAY_LENGTH); i++) {
		free(cmdSpecs->args[i]);
		cmdSpecs->args[i] = NULL;
	}
}

void parseLine(char *line, struct CmdSpecs *cmdSpecs)
{
	char *buffer = strdup(line);
	char *workingBuffer = buffer;
	char *found;
	int i;

	for(i = 0; (found = strsep(&workingBuffer, " ")) != NULL; i++) {
		cmdSpecs->args[i] = strdup(found);
	}

	cmdSpecs->args[i] = NULL;
	strcpy(cmdSpecs->cmd, cmdSpecs->args[0]);

	free(buffer);
}

char *findInPath(char *cmd)
{
	char buffer[ARRAY_LENGTH];
	struct stat sb;

	for(int i = 0; (globalPath[i] != NULL) && (i < ARRAY_LENGTH); i++) {
		sprintf(buffer, "%s/%s", globalPath[i], cmd);
		if((stat(buffer, &sb) == 0) && (sb.st_mode & S_IXUSR)) {
			return strdup(buffer);
		}
	}

	return cmd; // not found in path so just return cmd itself, might be executable
}

void executeCd(struct CmdSpecs *cmdSpecs)
{
	// cd must have one argument
	if((cmdSpecs->args[1] == NULL) || (cmdSpecs->args[2] != NULL)) {
		exitWithError();
	}

	if(chdir(cmdSpecs->args[1]) != 0) {
		exitWithError();
	}
}

void executePath(struct CmdSpecs *cmdSpecs)
{
	// clear old globalPath
	for(int i = 0; (globalPath[i] != NULL) && (i < ARRAY_LENGTH); i++) {
		free(globalPath[i]);
		globalPath[i] = NULL;
	}

	// add new globalPath (ignore first element, it's command name)
	for(int i = 1; cmdSpecs->args[i] != NULL; i++) {
		globalPath[i-1] = strdup(cmdSpecs->args[i]);
	}
}

void executeExternal(struct CmdSpecs *cmdSpecs)
{
	int childStatus;
	pid_t pid = fork();

	if(pid == -1) {
		exitWithError();
	} else if(pid > 0) {
		waitpid(pid, &childStatus, 0);
		if(childStatus != 0) {
			exitWithError();
		}
	} else {
		execv(findInPath(cmdSpecs->cmd), cmdSpecs->args);
		exit(1);
	}
}

// returns 1 if exit requested, 0 otherwise
// TODO: add support for &&
int executeLine(char *line)
{
	struct CmdSpecs cmdSpecs;
	parseLine(line, &cmdSpecs);

	if(strcmp(cmdSpecs.cmd, "exit") == 0) {
		return 1;
	} else if(strcmp(cmdSpecs.cmd, "cd") == 0) {
		executeCd(&cmdSpecs);
	} else if(strcmp(cmdSpecs.cmd, "path") == 0) {
		executePath(&cmdSpecs);
	} else {
		executeExternal(&cmdSpecs);
	}

	freeParsedLineArgs(&cmdSpecs);

	return 0;
}

void run(void)
{
	char *line = NULL;
	size_t dummyLength = 0;
	ssize_t lineLength;
	
	int isInteractive = 1;
	int done = 0;

	while(!done) {
		if(isInteractive) { printPrompt(); }
		lineLength = getline(&line, &dummyLength, stdin);
		if(lineLength == -1) {
			done = 1;
		} else {
			line[lineLength-1] = 0; // get rid of '\n'
			done = executeLine(line);
		}
	}
}

int main(int argc, char **argv)
{
	switch(argc) {
		case 1: // interactive
			run();
			break;
		case 2: // batch
			// TODO: implement
			break;
		default: // error
			exitWithError();
	}
}

