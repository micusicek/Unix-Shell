#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define ARRAY_LENGTH 20

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

// holds arrays of strings (char pointers), last one is NULL
// suitable for usage with execv
struct Strings {
	char **data;
	int maxSize;
	int currentSize;
};

struct Strings *stringsNew(int maxSize)
{
	struct Strings *s = (struct Strings *) malloc(sizeof(struct Strings));
	s->data = (char **)malloc((maxSize + 1) * sizeof(char*)); // space for NULL at the end
	s->maxSize = maxSize;
	s->currentSize = 0;
	for(int i = 0; i < maxSize; i++) { s->data[i] = NULL; }
	return s;
}

void stringsDelete(struct Strings *strings)
{
	for(int i = 0; i < strings->maxSize; i++) {
		if(strings->data[i] != NULL) {
			free(strings->data[i]);
		}
	}
	free(strings->data);
	free(strings);
}

void stringsAppend(struct Strings *strings, char *s)
{
	if(strings->currentSize == strings->maxSize) {
		exitWithError();
	}
	strings->data[strings->currentSize++] = strdup(s);
}

void stringsPrint(struct Strings *strings)
{
	for(int i = 0; i < strings->currentSize; i++) {
		printf("[%s]", strings->data[i]);
	}
}

// holds commands suitable for execv
struct Commands {
	struct Strings *commands;
	int maxSize;
	int current;
}

// add new, delete, append, print

#ifdef ergnsepognpreotg
struct Command {
	// char cmd[ARRAY_LENGTH];
	char *args[ARRAY_LENGTH];
};

void freeCommand(struct Command *command)
{
	for(int i = 0; (command->args[i] != NULL) && (i < ARRAY_LENGTH); i++) {
		free(command->args[i]);
		command->args[i] = NULL;
	}
}
#endif // ergnsepognpreotg

void parseLine(char *line, struct Command *commands)
{
	char *buffer = strdup(line);
	char *workingBuffer = buffer;
	char *token;
	int commandIndex, argIndex;
	struct Command *currentCommand;

	for(
		commandIndex = argIndex = 0, currentCommand = &commands[0];
		(token = strsep(&workingBuffer, " ")) != NULL;
	) {
		if(strcmp(token, "&&") == 0) {
			// next command
			currentCommand->args[argIndex] = NULL;
			argIndex = 0;
			commandIndex++;
			currentCommand = &commands[commandIndex];
		} else {
			// add argument
			currentCommand->args[argIndex] = strdup(token);
			argIndex++;
		}
	}

	commands[commandIndex].args[0] = NULL;

	free(buffer);
}

// caller must free return value
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

	return strdup(cmd); // not found in path so just return cmd itself, might be executable
}

void executeCd(struct Command *command)
{
	// cd must have one argument
	if((command->args[1] == NULL) || (command->args[2] != NULL)) {
		exitWithError();
	}

	if(chdir(command->args[1]) != 0) {
		exitWithError();
	}
}

void executePath(struct Command *command)
{
	// clear old globalPath
	for(int i = 0; (globalPath[i] != NULL) && (i < ARRAY_LENGTH); i++) {
		free(globalPath[i]);
		globalPath[i] = NULL;
	}

	// add new globalPath (ignore first element, it's command name)
	for(int i = 1; command->args[i] != NULL; i++) {
		globalPath[i-1] = strdup(command->args[i]);
	}
}

void executeExternal(struct Command *command)
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
		execv(findInPath(command->args[0]), command->args);
		exit(1);
	}
}

void executeCommand(struct Command *cmd) {
	if(strcmp(cmd->args[0], "exit") == 0) {
		exit(0);
	} else if(strcmp(cmd->args[0], "cd") == 0) {
		executeCd(cmd);
	} else if(strcmp(cmd->args[0], "path") == 0) {
		executePath(cmd);
	} else {
		executeExternal(cmd);
	}
}

// void executeCommands(

void runLine(char *line)
{
	// test Strings, Commands BEGIN
	// strings
	struct Strings *s = stringsNew(3);
	stringsAppend(s, "one");
	stringsAppend(s, "two");
	stringsAppend(s, "three");
	// stringsAppend(s, "four"); // causes error, out of bounds
	stringsPrint(s);
	// commands

	exit(0);
	// test Strings, Commands END

	// ignore emnpty lines
	if(strcmp(line, "") == 0) {
		return;
	}

	struct Command commands[ARRAY_LENGTH];
	parseLine(line, commands);

	for(int i = 0; (commands[i].args[0] != NULL) && (i < ARRAY_LENGTH); i++) {
		executeCommand(&commands[i]);
		// TODO wait for all children
		freeCommand(&commands[i]);
	}
}

void run(FILE *f)
{
	char *line = NULL;
	size_t dummyLength = 0;
	ssize_t lineLength;
	
	int isInteractive = (f == stdin) ? 1 : 0;
	int done = 0;

	while(1) {
		if(isInteractive) { printPrompt(); }
		lineLength = getline(&line, &dummyLength, f);
		if(lineLength == -1) {
			exit(0);
		} else {
			line[strlen(line)-1] = 0; // get rid of '\n'
			runLine(line);
			free(line);
			line = NULL;
		}
	}
}

void runBatch(char *batchFilename)
{
	FILE *f = fopen(batchFilename, "r");
	if(f == NULL) {
		exitWithError();
	} else {
		run(f);
		fclose(f);
	}
}

int main(int argc, char **argv)
{
	switch(argc) {
		case 1: run(stdin); break;
		case 2: runBatch(argv[1]); break;
		default: exitWithError();
	}
}

