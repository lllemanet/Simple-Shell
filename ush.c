#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#define BUFSIZE 1024
#define MAX_OF_CMD_ELEMENTS 10

int readline(char** l);
char** extract_args(char* line);
int getQArg(char* line, char** res);
void exitWithMsg(const char* msg);
void umemcpy(void* dest, void* src, size_t size);

int main() {
	size_t size;
	char* line = NULL;
	char** args;
	getQArg("\"\\\"Hello world\\\"\"", &line);
	readline(&line);
	args = extract_args(line);
	while (*args != NULL) {
		printf("%s", *args++);
	}

	


	while (1) {
		printf(">");

		if (readline(&line) == -1 ||
			(args = extract_args(line)) == NULL) {
			fprintf(stderr, "Problems with getting input\n");
			free(line);
			continue;
		}

		execvp(args[0], (const char * const *)args);
		printf("%s", line);
		//free(line); TODO free args
	}
}

//overwrites *l	
int readline(char** l) {
	*l = NULL;
	size_t bufsize = 0; //have getline allocate buffer for us
	return getline(l, &bufsize, stdin);
}


int getQArg(char* line, char** dest) {
	int position = 0, bufsize = BUFSIZE;
	char* buffer = malloc(sizeof(char) * bufsize);
	char* c = line;

	if (*c != '"')
		return -1;

	c++;

	while (*c != '"') {
		if (*c == '\\')
			c++;
		if (*c == '\0') {
			return -1;
		}

		buffer[position++] = *c++;

		if (position >= bufsize) {
			bufsize += BUFSIZE;
			buffer = realloc(buffer, bufsize);
			if (!buffer) {
				return -1;
			}
		}
	}
	buffer[position] = '\0';
	*dest = malloc(sizeof(char) * position + 1);
	memcpy(*dest, buffer, position + 1);

	return position;
}


char** extract_args(char* line) {
	if (line == NULL)
		return NULL;

	int bufsize = BUFSIZE;
	int left = -1, right, posOfBuf = 0;
	char** args = malloc(sizeof(char *) * bufsize);
	char** res;

	while (line[left + 1] != '\0') {
		while (line[++left] == ' ' && line[left] != '\0' && line[left] != '\n');
		if (line[left] == '\0' || line[left] == '\n') {
			//free(line + nextToken); //TODO free all left whitespaces
			break;
		}
		if (line[left] == '"') {
			int size;
			if ((size = getQArg(&line[left], &args[posOfBuf++])) == -1) {
				args[--posOfBuf] = NULL;
				//TODO free all remaining args
				return args;
			}
			else {
				left += size + 1; //1 for ""
				//check if buf overflow?
				continue;
			}

		}
		right = left;

		while (line[++right] != ' ' && line[right] != '\0' && line[right] != '\n');
		if (line[right] == '\0') {
			args[posOfBuf++] = &(line[left]);
			break;
		}
		line[right] = '\0';

		args[posOfBuf++] = &(line[left]);
		if (posOfBuf >= bufsize)
			realloc(args, (bufsize += BUFSIZE));
		left = right;
	}

	if (posOfBuf >= bufsize)
		realloc(args, (bufsize += BUFSIZE));
	args[posOfBuf] = NULL;

	res = malloc(sizeof(char*) * (posOfBuf + 1));
	
	memcpy(res, args, sizeof(char*) * (posOfBuf + 1));
	free(args);
	return res;
}