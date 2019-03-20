// build: gcc -Wall -Wextra -fdiagnostics-show-option -g ./wish.c -o wish
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

//////////////////////////////////////////////////
////////////////////////////////////////////////// globals
//////////////////////////////////////////////////

// constant for now, should be eventually dynamically allocated
const int MAX_COMMAND_COUNT=100;
const int MAX_ARGS_COUNT=100;

#define MAX_PATH_ELEMENTS 100

// where to look for binaries to execute
char *globalPath[MAX_PATH_ELEMENTS] = { 0 };

void printError(void)
{
	char *message = "An error has occurred\n";
	fwrite(message, strlen(message), 1, stderr); 
}

void printErrorAndExit(void)
{
	printError();
	exit(1);
}

void printPrompt(void)
{
	char prompt[] = "wish> ";
	fwrite(prompt, strlen(prompt), 1, stdout);
}

// simple shell command line parsing, tokenizes lines like this:
// ls -l>file&echo something &  pwd
char *getToken(char **s)
{
	// nothing...
	if(*s == NULL) {
		return NULL;
	}

	// still nothing...
	if(strlen(*s) == 0) {
		*s = NULL;
		return NULL;
	}

	char *cursor;

	// trim leading whitespace
	for(cursor = *s; isspace(*cursor); cursor++) {}

	char *begin = cursor;

	// deal with special tokens at the beginning of buffer
	switch(*cursor) {
		case '&':
		case '>':
			*s = cursor + 1;
			return strndup(cursor, 1);
			break;
	}

	// find token
	while(1) {
		switch(*cursor) {
			case '\0': // end of token and string
			case ' ':  // end of token
			case '&':  // special token &
			case '>':  // special token >
				*s = cursor;
				return strndup(begin, cursor - begin);
				break;
			default: // regular character in token
				cursor++;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////
////////////////////////////////////////////////// struct Cmd
//////////////////////////////////////////////////

// holds info about command to execute
// args is suitable for usage with execv
struct Cmd {
	char **args;
	int maxSize;
	int currentSize;
	char *redirectTarget;
};

struct Cmd *cmdNew(int maxSize)
{
	struct Cmd *c = (struct Cmd *) malloc(sizeof(struct Cmd));
	c->maxSize = maxSize;
	c->currentSize = 0;
	c->redirectTarget = NULL;
	// leave space for NULL at the end
	c->args = (char **)malloc((maxSize + 1) * sizeof(char*));
	for(int i = 0; i < maxSize; i++) { c->args[i] = NULL; }
	return c;
}

void cmdDelete(struct Cmd *cmd)
{
	if(cmd == NULL) { return; }
	for(int i = 0; i < cmd->maxSize; i++) {
		if(cmd->args[i] != NULL) {
			free(cmd->args[i]);
		}
	}
	free(cmd->args);
	if(cmd->redirectTarget != NULL) { free(cmd->redirectTarget); }
	free(cmd);
}

void cmdAppendArg(struct Cmd *cmd, char *arg)
{
	if(cmd->currentSize == cmd->maxSize) {
		printErrorAndExit();
	}
	cmd->args[cmd->currentSize++] = strdup(arg);
}

struct Cmd *cmdCopy(struct Cmd *sourceCmd)
{
	struct Cmd *destCmd = cmdNew(sourceCmd->maxSize);
	if(sourceCmd->redirectTarget != NULL) {
		destCmd->redirectTarget = strdup(sourceCmd->redirectTarget);
	}
	for(int i = 0; i < sourceCmd->currentSize; i++) {
		cmdAppendArg(destCmd, sourceCmd->args[i]);
	}
	return destCmd;
}

void cmdPrint(struct Cmd *cmd)
{
	for(int i = 0; i < cmd->currentSize; i++) {
		printf("[%s]", cmd->args[i]);
	}
	if(cmd->redirectTarget != NULL) {
		printf(" > [%s]", cmd->redirectTarget);
	}
}

//////////////////////////////////////////////////
////////////////////////////////////////////////// struct Cmds
//////////////////////////////////////////////////

// conatiner for Cmd structs
struct Cmds {
	struct Cmd **cmds;
	int maxSize;
	int currentSize;
};

struct Cmds *cmdsNew(int maxSize)
{
	struct Cmds *c = (struct Cmds *) malloc(
		sizeof(struct Cmds)
	);
	// leave space for NULL at the end
	c->cmds = (struct Cmd **) malloc(
		(maxSize + 1) * sizeof(struct Cmd *)
	);
	c->maxSize = maxSize;
	c->currentSize = 0;
	for(int i = 0; i < maxSize; i++) { c->cmds[i] = NULL; }
	return c;
}

void cmdsDelete(struct Cmds *cmds)
{
	if(cmds == NULL) {
		return;
	}
	for(int i = 0; i < cmds->maxSize; i++) {
		if(cmds->cmds[i] != NULL) {
			free(cmds->cmds[i]);
		}
	}
	free(cmds->cmds);
	free(cmds);
}

void cmdsAppend(struct Cmds *cmds, struct Cmd *cmd)
{
	if(cmds->currentSize == cmds->maxSize) {
		printErrorAndExit();
	}
	cmds->cmds[cmds->currentSize] = cmdCopy(cmd);
	cmds->currentSize++;
}

void cmdsPrint(struct Cmds *cmds)
{
	if(cmds == NULL) {
		printf("cmds: <NULL>\n");
		return;
	}
	for(int i = 0; i < cmds->currentSize; i++) {
		printf("cmd: ");
		cmdPrint(cmds->cmds[i]);
		printf("\n");
	}
}

//////////////////////////////////////////////////
////////////////////////////////////////////////// shell code
//////////////////////////////////////////////////

// returns 1 if token is empty
int tokenIsEmpty(char *token) {
	return (strcmp(token, "") == 0) ? 1 : 0;
}
// returns 1 if token is redirect '>' token
int tokenIsRedirect(char *token) {
	return (strcmp(token, ">") == 0) ? 1 : 0;
}

// returns 1 if token is background '&' token
int tokenIsBackground(char *token) {
	return (strcmp(token, "&") == 0) ? 1 : 0;
}

void parseLine(char *line, struct Cmds **cmds)
{
	enum validStates {
		expectCommand,  // filename of a command to execute
		expectArg,      // arg & >
		expectRedirect, // filename to redirect to
		done            // all done
	} state = expectCommand;

	char *buffer = strdup(line);
	char *workingBuffer = buffer;
	char *token;
	*cmds = cmdsNew(MAX_COMMAND_COUNT);
	struct Cmd *currentCmd = NULL;

	for(
		// token = strsep(&workingBuffer, " ");
		// state != done;
		// token = strsep(&workingBuffer, " ")
		token = getToken(&workingBuffer);
		state != done;
		token = getToken(&workingBuffer)
	) {
		switch(state) {
			case expectCommand:
				// expecting beginning of a command line,
				// first must be program to execute,
				// end of command line is also accepted,
				// anything else is an error
				if(token == NULL) {
					state = done;
				} else if(tokenIsEmpty(token)) {
					// ignore
				} else if(tokenIsRedirect(token)) {
					cmdsDelete(*cmds);
					*cmds = NULL;
					printError();
					free(token);
					return;
				} else if(tokenIsBackground(token)) {
					// not an error, just empty command
					// continue waiting for next command
					free(token);
				} else {
					currentCmd = cmdNew(MAX_ARGS_COUNT);
					cmdAppendArg(currentCmd, token);
					state = expectArg;
				}
				break;
			case expectRedirect:
				// expecting filename to redirect to,
				// everythign else is error
				if(token == NULL) {
					cmdsDelete(*cmds);
					*cmds = NULL;
					printError();
					return;
				} else if(tokenIsEmpty(token)) {
					// ignore
				} else if(tokenIsRedirect(token)) {
					cmdsDelete(*cmds);
					*cmds = NULL;
					printError();
					free(token);
					return;
				} else if(tokenIsBackground(token)) {
					cmdsDelete(*cmds);
					*cmds = NULL;
					printError();
					free(token);
					return;
				} else {
					currentCmd->redirectTarget =
						strdup(token);
					// end of command
					cmdsAppend(*cmds, currentCmd);
					cmdDelete(currentCmd);
					currentCmd = NULL;
					state = expectCommand;
				}
				break;
			case expectArg:
				// expecting an argument or & or > or end
				if(token == NULL) {
					cmdsAppend(*cmds, currentCmd);
					cmdDelete(currentCmd);
					currentCmd = NULL;
					state = done;
				} else if(tokenIsEmpty(token)) {
					// ignore
				} else if(tokenIsRedirect(token)) {
					state = expectRedirect;
				} else if(tokenIsBackground(token)) {
					cmdsAppend(*cmds, currentCmd);
					cmdDelete(currentCmd);
					currentCmd = NULL;
					state = expectCommand;
				} else {
					cmdAppendArg(currentCmd, token);
				}
				break;
			default:
				printErrorAndExit();
		}
	}

	if(token != NULL) { free(token); }
	free(buffer);
}

// caller must free return value
char *findInPath(char *cmd)
{
	char buffer[3000]; // TODO: should be dynamically allocated
	struct stat sb;

	for(int i = 0; globalPath[i] != NULL; i++) {
		sprintf(buffer, "%s/%s", globalPath[i], cmd);
		if((stat(buffer, &sb) == 0) && (sb.st_mode & S_IXUSR)) {
			return strdup(buffer);
		}
	}

	return strdup(cmd); // not found, return as is (might work)
}

void executeExit(struct Cmd *cmd)
{
	if(cmd->currentSize != 1) {
		printError();
		return;
	}
	exit(0);
}

void executeCd(struct Cmd *cmd)
{
	// cd must have one argument
	if(cmd->currentSize != 2) {
		printError();
		return;
	}

	if(chdir(cmd->args[1]) != 0) {
		printError();
	}
}

void executePath(struct Cmd *cmd)
{
	// clear old globalPath
	for(int i = 0; (globalPath[i] != NULL) && (i < MAX_PATH_ELEMENTS); i++) {
		free(globalPath[i]);
		globalPath[i] = NULL;
	}

	// add new globalPath (ignore first element, it's cmd name)
	for(int i = 1; cmd->args[i] != NULL; i++) {
		globalPath[i-1] = strdup(cmd->args[i]);
	}
}

void executeExternal(struct Cmd *cmd)
{
	// int childStatus;
	pid_t pid = fork();

	if(pid == -1) {
		printErrorAndExit();
	} else if(pid > 0) {
		// parent, just return, somebody else will wait
		return;
	} else {
		// child, execute external command
		if(cmd->redirectTarget != NULL) {
			int fd = open(
				cmd->redirectTarget,
				O_RDWR | O_CREAT | O_TRUNC,
				S_IRUSR | S_IWUSR
			);
			if(fd == -1) {
				printError();
			} else {
				dup2(fd, 1); // stdout goes to fd
				dup2(fd, 2); // stderr goes to fd
				close(fd);
			}
		}
		execv(findInPath(cmd->args[0]), cmd->args);
		printError();
		exit(1); // should never get here
	}
}

void executeCmd(struct Cmd *cmd) {
	if(strcmp(cmd->args[0], "exit") == 0) {
		executeExit(cmd);
	} else if(strcmp(cmd->args[0], "cd") == 0) {
		executeCd(cmd);
	} else if(strcmp(cmd->args[0], "path") == 0) {
		executePath(cmd);
	} else {
		executeExternal(cmd);
	}
}

void executeLine(char *line)
{
	struct Cmds *cmds = NULL;
	parseLine(line, &cmds);

	if(cmds == NULL) {
		return;
	}

	for(int i = 0; i < cmds->currentSize; i++) {
		executeCmd(cmds->cmds[i]);
	}

	int status;
	pid_t pid;
	int errorOccured = 0;
	while((pid = wait(&status)) > 0) {
		if(status != 0) { errorOccured = 1; }
	}
	if(errorOccured) {
		// provided tests don't expect error?
		// printError();
	}
}

void executeStream(FILE *f)
{
	char *line = NULL;
	size_t dummyLength = 0;
	ssize_t lineLength;
	
	int isInteractive = (f == stdin) ? 1 : 0;

	while(1) {
		if(isInteractive) { printPrompt(); }
		lineLength = getline(&line, &dummyLength, f);
		if(lineLength == -1) {
			exit(0);
		} else {
			line[strlen(line)-1] = 0; // get rid of '\n'
			executeLine(line);
			free(line);
			line = NULL;
		}
	}
}

void executeFile(char *batchFilename)
{
	FILE *f = fopen(batchFilename, "r");
	if(f == NULL) {
		printErrorAndExit();
	} else {
		executeStream(f);
		fclose(f);
	}
}

//////////////////////////////////////////////////
////////////////////////////////////////////////// main
//////////////////////////////////////////////////

int main(int argc, char **argv)
{
	// create some sensible path
	executeLine("path /bin /usr/bin");

	// and go!
	switch(argc) {
		case 1: executeStream(stdin); break;
		case 2: executeFile(argv[1]); break;
		default: printErrorAndExit();
	}
}

