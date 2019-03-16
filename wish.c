#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define ARRAY_LENGTH 1024

struct ParsedLine {
	char path[ARRAY_LENGTH];
	char *args[ARRAY_LENGTH];
};

void exitWithError(void)
{
	char *error_message = "An error has occurred\n";
	fwrite(error_message, strlen(error_message), 1, stdout); 
	exit(1);
}

void printPrompt(void)
{
	char prompt[] = "wish> ";
	fwrite(prompt, strlen(prompt), 1, stdout);
}

void freeParsedLineArgs(struct ParsedLine *parsedLine)
{
	// TODO
}

void parseLine(char *line, struct ParsedLine *parsedLine)
{
	char *buffer = strdup(line);
	char *workingBuffer = buffer;
	char *found;
	int i = 0;

	for(i=0; (found = strsep(&workingBuffer, " ")) != NULL; i++) {
		parsedLine->args[i] = strdup(found);
		// i++;
	}

	parsedLine->args[i] = NULL;
	strcpy(parsedLine->path, parsedLine->args[0]);

	free(buffer);
}

void executeCd(struct ParsedLine *parsedLine)
{
	printf("executeCd: TODO!\n");
}

void executePath(struct ParsedLine *parsedLine)
{
	printf("executePath: TODO!\n");
}

void executeExternal(struct ParsedLine *parsedLine)
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
		execv(parsedLine->path, parsedLine->args);
		exit(1);
	}
}

// returns 1 if exit requested, 0 otherwise
int executeLine(char *line)
{
	struct ParsedLine parsedLine;
	parseLine(line, &parsedLine);

	// printf("executeLine: parsedLine.path [%s]\n", parsedLine.path);

	if(strcmp(parsedLine.path, "exit") == 0) {
		return 1;
	} else if(strcmp(parsedLine.path, "cd") == 0) {
		executeCd(&parsedLine);
	} else if(strcmp(parsedLine.path, "path") == 0) {
		executePath(&parsedLine);
	} else {
		executeExternal(&parsedLine);
	}

	freeParsedLineArgs(&parsedLine);

	return 0;
}

void run(void)
{
	char *line = NULL;
	size_t dummyLength = 0;
	ssize_t lineLength;
	
	int isInteractive = 1; // TODO: get as an argument
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

