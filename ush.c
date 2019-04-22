#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/types.h>

#define BUFSIZE 1024
#define MAX_OF_CMD_ELEMENTS 10
#define IN
#define OUT


int readline(OUT char** l);
char** extractArgs(char* line);
int getQuoteStr(char* line, char** res);
char*** extractCmds(char** args);
int isredirectsym(char c);
int executeCommands(char*** cmds);
int launchCommand(char*** cmds);
void displayCmds(char*** cmds);
/*miscellaneous*/
int getlen(void** cmds);
int freePtrs(const int depth, IN OUT void *ptr);
int reallocbuf(void** args, size_t size);

/*built-in commands*/
int changeDir(char** arg);
int help(char** arg);
int exitUsh(char** arg);
int numOfBuiltIns();
static char cwd[1024];

int (*fnSpecCmds[])(char**) = {
	&changeDir,
	&help,
	&exitUsh
};


char *asSpecCmds[] = {
	"cd",
	"help",
	"exit"
};



int main() {
	size_t size;
	char* line = NULL;
	char** args;
	char*** cmds;
	
	while (1) {
		getcwd(cwd, sizeof(cwd));
		printf("%s>", cwd);

		if (readline(&line) == -1 ||
				(args = extractArgs(line)) == NULL ||
				(cmds = extractCmds(args)) == NULL) {
			fprintf(stderr, "Problems with getting input\n");
			free(line);
			continue;
		}
		//printf("%d\n", _msize(cmds));
		if (executeCommands(cmds) == 0)
			return 0;
		
		if (freePtrs(3, cmds) < 0) 
			fprintf(stderr, "Unable to free commands\n");
		
	}
}

//overwrites *l	
int readline(OUT char** l) {
	*l = NULL;
	size_t bufsize = 0; //have getline allocate buffer for us
	return getline(l, &bufsize, stdin);
}

//TODO return char* rather than init dest
int getQuoteStr(char* line, char** dest) {
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
	*dest = malloc(sizeof(char) * (position + 1));
	memcpy(*dest, buffer, position + 1);
	free(buffer);

	return position;
}

char** extractArgs(char* line) {
	if (line == NULL)
		return NULL;

	int bufsize = BUFSIZE, posOfBuf = 0;
	int left = -1, right;
	char** args = malloc(sizeof(char *) * bufsize);
	char** res;

	while (line[left + 1] != '\0') {
		while (line[++left] == ' ' && line[left] != '\0' && line[left] != '\n');
		if (line[left] == '\0' || line[left] == '\n') {
			//free(line + nextToken); //TODO free all left whitespaces
			break;
		}
		if (line[left] == '"') {
			int size = getQuoteStr(&line[left], &args[posOfBuf++]);
			if (size == -1) {
				args[--posOfBuf] = NULL;
				break; //returns not all arguments
			}
			else {
				left += size + 1; //1 for "
				//check if buf overflow?
				continue;
			}

		}
		right = left;

		while (line[++right] != ' ' && line[right] != '\0' && line[right] != '\n');

		int size = right - left;//redundancy for reducing complexity
		char* arg = malloc(sizeof(char) * (size + 1));
		memcpy(arg, &line[left], size);
		arg[size] = '\0';
		args[posOfBuf++] = arg;

		if (line[right] == '\0')	
			break;

		if (posOfBuf >= bufsize && (reallocbuf((void**)args, bufsize += BUFSIZE)) == -1) { //TODO free args and all strings inside args
			freePtrs(2, args);
			return NULL;
		}
		left = right;
	}

	if (posOfBuf >= bufsize && (reallocbuf((void**)args, bufsize += BUFSIZE)) == -1) {

		return NULL;
	}
		
	args[posOfBuf] = NULL;

	res = malloc(sizeof(char*) * (posOfBuf + 1));
	
	memcpy(res, args, sizeof(char*) * (posOfBuf + 1));
	free(args);
	return res;
}

/*
 * Free all null-terminating pointers to pointers and ultimate pointed data structures
 * @param ptr - <code>depth</code> pointer
 * @param depth - depth of indireciton (number of * in specified ptr)
 * if depth == 1: free(ptr)
 * if depth > 1: free all pointers on contiguous region of memory ptr points to until null
 * @return 0 - success; -1 - failure; -2 - not all pointers were freed
 */
int freePtrs(const int depth, IN OUT void *ptr) {
	if (depth < 1 || ptr == NULL) {
		return -1;
	}

	if (depth == 1) {
		free(ptr);
		return 0;
	}
	void **cur = (void**)ptr;
	while (*cur != NULL) {
		if (freePtrs(depth - 1, *cur) == -1)
			return -1;
		cur++;
	}
	//better to put *(void**)ptr=null here
	free(ptr);
	*(void **)ptr = NULL; //not thread-safe? and bad
	return 0;
}

int reallocbuf(void** args, size_t size) {
	if (realloc(args, size) == NULL) {
		fprintf(stderr, "Unable to realloc buffer\n");
		return -1;
	}
}

/*from args like { "ls", "-a", "|", "wc", "-w", ">", "a.txt" } 
extracts		 { {"ls", "-a"}, {"|"}, {"wc", "-w"}, {">"}, {"a.txt"} } */
char*** extractCmds(char** args) {
	if (args == NULL)
		return NULL;

	int bufsize = BUFSIZE;
	int left = -1, right = -1, posOfCmd = 0;
	char*** cmds = malloc(sizeof(char **) * bufsize);
	char*** res;

	while (args[left + 1] != NULL) {
		while (args[++right] != NULL && !isredirectsym(args[right][0]));
		if (args[right] == NULL) { //if we reached end of args
			left++;
			int cmdSize = right - left;
			char** cmd = malloc(sizeof(char *) * (cmdSize + 1));
			memcpy(cmd, &(args[left]), sizeof(char*) * (cmdSize));
			cmd[cmdSize] = NULL; //null-terminating
			cmds[posOfCmd++] = cmd;
			break;
		}
		//next argument should be 1-symbol long (since available indirect symbols are '|', '<' and '>'
		if (args[right][1] != '\0') { 
			fprintf(stderr, "Pipe symbols are only '|', '<' and '>'!\n");
			cmds[posOfCmd] = NULL;
			freePtrs(3, cmds);
			return NULL;
		}
		left++;
		//TODO duplicated code
		int cmdSize = right - left;
		char** cmd = malloc(sizeof(char *) * (cmdSize + 1));
		memcpy(cmd, &args[left], sizeof(char*) * (cmdSize));
		cmd[cmdSize] = NULL;
		cmds[posOfCmd++] = cmd;
		//+ redir symbol
		cmd = malloc(sizeof(char *) * 2);
		cmd[0] = args[right];
		cmd[1] = NULL;
		cmds[posOfCmd++] = cmd;

		left = right;

		if (posOfCmd >= bufsize)
			realloc(args, (bufsize += BUFSIZE)); //TODO check for null
	}

	if (posOfCmd >= bufsize)
		realloc(args, (bufsize += BUFSIZE));
	cmds[posOfCmd] = NULL;

	res = malloc(sizeof(char**) * (posOfCmd + 1));

	memcpy(res, cmds, sizeof(char**) * (posOfCmd + 1));
	free(cmds);
	return res;
}

void displayCmds(char*** cmds) {
	while (*cmds != NULL) {
		char** cmd = *cmds;
		while (*cmd != NULL) {
			printf("%s_", *cmd);
			cmd++;
		}
		printf("\n");
		cmds++;
	}
}

// 1 - succes, 0 - exit, -1 - failure
int executeCommands(char*** cmds) {
	for (int i = 0; i < numOfBuiltIns(); i++) {
		char* firstArg = **cmds;
		if (strcmp(firstArg, asSpecCmds[i]) == 0)
			return (*fnSpecCmds[i])(*cmds);
	}

	return launchCommand(cmds);
}

int launchCommand(char*** cmds) {
	if (cmds == NULL)
		return -1;
		int cmdnum = getlen((void**)cmds),
		pos = 0, status, step;
	int lfd[2]; //left file in&out descriptors
	int rfd[2] = { STDIN_FILENO, STDOUT_FILENO };
	int temp[2];
	pid_t pid;

	while (pos < cmdnum) {

		if (cmdnum - pos > 1) { //if <cmd> <redirop> <cmd>
			switch (cmds[pos + 1][0][0]) { //get the redir symbol
			case '|':
				pipe(temp);
				lfd[0] = rfd[0];	//doesn't change input
				lfd[1] = temp[1];	//output is out of pipe now
				rfd[0] = temp[0];	//in of next cmd is in of pipe now
				rfd[1] = STDOUT_FILENO;
				break;
			case '<': //possible only at the start
				rfd[0] = open(cmds[pos + 2][0], O_RDONLY); //TOrovide normal interface for cmds
				cmds[pos + 2] = cmds[pos];//TODO free prev pls)
				pos += 2;
				continue;
			case '>':
				rfd[1] = open(cmds[pos + 2][0], O_WRONLY | O_CREAT); //TODO provide normal interface for cmds
				printf("%d\n", rfd[1]);
				cmds[pos + 2] = cmds[pos];//TODO free prev pls)
				pos += 2;
				continue;
			}
			step = 2; //to know how to change pos before calling execvp
		}
		else {
			lfd[0] = rfd[0];
			lfd[1] = rfd[1];
			step = 1;
		}
		pid = fork();
		if (pid < 0) {
			//TODO close fds!
			return -1;
		}

		if (pid == 0) {
			//TODO get rig of lfd[0] and lfd[1] if they're not 0 and 1
			//close read end if |
			dup2(lfd[0], 0);
			dup2(lfd[1], 1);
			execvp(cmds[pos][0], cmds[pos]);
			_exit(EXIT_FAILURE);
		}
		else { //else is useless
			waitpid(pid, &status, WUNTRACED);
			if (lfd[1] != 1)
				close(lfd[1]);
			if (lfd[0] != 0)
				close(lfd[0]);
			//TODO delete all unecessary fds
		}
		pos += step;
	}
	return 1;
}

int getlen(void** cmds) {
	int len = 0;
	while (*cmds++ != NULL) {
		len++;
	}
	return len;
}

int isredirectsym(char c) {
	return c == '|' || c == '<' || c == '>';
}

int exitUsh(char** arg) {
	return 0;
}

int changeDir(char** args) {
	if (args[1] == NULL)
		fprintf(stderr, "Invalid argument for change directory\n");
	else if (chdir(args[1]) == -1)
		fprintf(stderr, "Can't change directory\n");
	return 1;
}

int help(char** arg) {
	printf("Type 'exit' to exit\n");
	return 1;
}

int numOfBuiltIns() {
	return sizeof(asSpecCmds) / sizeof(char *);
}
