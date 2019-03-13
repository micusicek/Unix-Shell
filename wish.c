#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct ParsedLine {
	char *path;
	char *args[];
};

void exitWithError(void)
{
	char error_message[30] = "An error has occurred\n";
	fwrite(error_message, strlen(error_message), 1, stdout); 
	exit(1);
}

void printPrompt(void)
{
	char prompt[] = "wish> ";
	fwrite(prompt, strlen(prompt), 1, stdout);
}

void parseLine(char *line, struct ParsedLine *parsedLine)
{
	// TODO: implement!
}

// returns 1 if exit requested, 0 otherwise
int executeLine(char *line)
{
	struct ParsedLine parsedLine;
	parseLine(line, &parsedLine);

	if(strcmp(parsedLine.path, "exit") == 0) {
		return 1;
	} else if(strcmp(parsedLine.path, "cd") == 0) {
		// TODO: implement cd
	} else if(strcmp(parsedLine.path, "path") == 0) {
		// TODO: implement cd
	} else {
		// TODO: implement external command
	}
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
		// TODO: read line
		lineLength = getline(&line, &dummyLength, stdin);
		if(lineLength == -1) {
			done = 1;
		} else {
			// TODO: execute!
			done = executeLine(line);
		}
	}
}

int main(int argc, char **argv)
{
	// TODO: read args (0 or 1)
	switch(argc) {
		case 1: // interactive
			run();
			break;
		case 2: // batch
			break;
		default: // error
			exitWithError();
	}
	// TODO: batch or interactive mode
	// TODO: run main loop (read file or stdin)

#ifdef wlierjgneorjgnoire
	char *line = NULL;
	size_t linesize = 0;
	ssize_t linelen;

	fwrite("wish> ", 6, 1, stdout);

	while ((linelen = getline(&line, &linesize, stdin)) != -1) {
		if (strncmp("exit", line, 4) == 0) {
			// user wants to exit
			exit(0);
		}
		fwrite(line, linelen, 1, stdout);

		// fork
		// if child, exec
		// if parent, wait
	}

	free(line);
	if (ferror(stdin))
		err(1, "getline");
#endif // wlierjgneorjgnoire
}

