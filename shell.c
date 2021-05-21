#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#define SYS_CALL_DIRECTORY "/bin/"
#define THEME_RED "\x1b[31m"
#define THEME_BLUE "\x1b[34m"
#define THEME_RESET "\x1b[0m"

typedef struct{
	char *name;
	char *value;
}envVar;

envVar **envVarList;
int envListLength = 16;
int numEnvVar = 0;

typedef struct{
	char *name;
	struct tm *time;
	int code;
}Command;

Command **historyCMDsList;
int cmdListLength = 16;
int numCommand = 0;

void printPrompt();
void parseInput(char *toParse, int cmdIndex);
void execCommands(char **arguments, int argCount, int cmdIndex);
int changeTheme(char **arguments);
int print(char **arguments, int argCount);
void printHistoryCMD(Command *cmd);


int main(int argc, char const *argv[]){

	char *input = malloc(200);
	char *curDir = malloc(200);
	curDir = getcwd(curDir, 200);
	envVarList = (envVar**)malloc(sizeof(envVar*)*16);	
	historyCMDsList = (Command**)malloc(sizeof(Command*)*16);	

	strcat(curDir, "/");
	if(argc == 2){
		strcat(curDir, argv[1]);
		FILE *fp;
		if((fp = fopen(curDir, "r")) != NULL){
			while(fgets(input, 200, fp) != NULL){
				input = strtok(input, "\n");
				parseInput(input, numCommand-1);
				memset(input, 0, 200);
			}
		} else{
			printf("%s\n", "Error opening file");
		}
	}
	while(1){
		printPrompt();
		fgets(input, 200, stdin);
		time_t rawtime = time(NULL);

		Command *newCommand = (Command*)malloc(sizeof(Command*));
		newCommand->name =(char*)malloc(sizeof(char)*200);
		newCommand->time = localtime(&rawtime);
		strcpy(newCommand -> name, input);	

		if(numCommand == cmdListLength){
			cmdListLength *= 2;
			historyCMDsList = (Command**)realloc(historyCMDsList,sizeof(Command*)*cmdListLength);
		}
		historyCMDsList[numCommand] = newCommand;
		numCommand++;

		//Removed \n as it can cause strtok issues in parseInput().
		input = strtok(input, "\n");
		parseInput(input, numCommand-1);
		memset(input, 0, 200);
	}

	free(input);
	return 0;
}

void printPrompt(){
	printf("%s", "cshell$ ");
}

void parseInput(char *toParse, int cmdIndex){
	//Checks for special chars in input to determine right course of action.
	char **argsList = malloc(30);
	char varName[32];
	memset(varName, 0 , 32);
	char varValue[32];
	memset(varValue, 0 , 32);
	int start, end;
	bool exists = 0;

	if(strstr(toParse, "$") && strstr(toParse, "=")){
		//Todo: add variable creation & assignment
		//toParse: $var = 12   $<VAR> = <value>
		for(int i=0; i <= strlen(toParse); i++){
			if(toParse[i] == '$')
				start = i;
			if(toParse[i] == '=')
				end = i; 
		}
		// extract <VAR>
		for(int i = start; i < end; i++){
			varName[i-(start)] = toParse[i];
		}
		//extract <value>
		for(int i = end+1; i< strlen(toParse); i++){
			varValue[i-(end+1)] = toParse[i];
		}
		// find out if the variable already exists
		for(int i = 0; i < numEnvVar; i++){
			if(strcmp(envVarList[i]-> name, varName) == 0){
				memset(envVarList[i]->value, 0, 32);
				strcpy(envVarList[i]-> value, varValue);
				exists = 1;
				break;
			}
		}
		// if the variable doesn't exist
		//check if the list is full
		if(exists == 0){
			if(numEnvVar == envListLength){
				envListLength *= 2;
				envVarList = (envVar**)realloc(envVarList,sizeof(envVar*)*envListLength);
			}
			//create and add new envVar 
			envVar *newEnvVar = (envVar*)malloc(sizeof(envVar*));
			newEnvVar->name =(char*)malloc(sizeof(char)*32);
			newEnvVar->value =(char*)malloc(sizeof(char)*32);

			strcpy(newEnvVar-> name, varName);
			strcpy(newEnvVar->value, varValue);
			envVarList[numEnvVar] = newEnvVar;
			numEnvVar++;
		}
		historyCMDsList[cmdIndex] -> code = 0;
	} else{
		//splits input into substrings & store in arguments array.
		int i = 0;
		char *temp = strtok(toParse, " ");
		while(temp != NULL){
			argsList[i] = malloc(40);
			strcpy(argsList[i], temp);
			temp = strtok(NULL, " ");
			i++;
		}
		execCommands(argsList, i, cmdIndex);
	}
}

void execCommands(char **arguments, int argCount, int cmdIndex){
	//Find right command or external exe to execute.
	int i;		//i is meant to store return code for built in commands
	if(strcmp(arguments[0], "exit") == 0){
		exit(0);
	} else if(strcmp(arguments[0], "log") == 0){
		for(int j=0; j< numCommand; j++){
			printHistoryCMD(historyCMDsList[j]);
		}
		printf("\n");
	} else if(strcmp(arguments[0], "print") == 0){
		i = print(arguments, argCount);
	} else if(strcmp(arguments[0], "theme") == 0){
		i = changeTheme(arguments);
	} else{
		int fds[2];
		if(pipe(fds) == -1){
			perror("pipe");
			exit(EXIT_FAILURE);
		}
		char buff[1000];
		memset(buff, 0, 1000);
		int fc = fork();
		if(fc < 0){
			printf("%s\n", "Fork failed");
		} else if(fc == 0){
			char tempPath[50] =  SYS_CALL_DIRECTORY;
			strcat(tempPath, arguments[0]);
			arguments[argCount] = NULL;
			close(fds[0]);
			if(dup2(fds[1], 1) == -1){
				perror("dup2");
				exit(EXIT_FAILURE);
			}
			if(execv(tempPath, arguments) < 0){
				printf("%s\n", "Error occured when executing command");
				i = -1;
			}
		} else{
			close(fds[1]);
			fflush(stdout);
			while(read(fds[0], buff, 1000) > 0){
				fprintf(stdout, "%s", buff);
			}
			close(fds[0]);
			wait(NULL);
		}
	}
	historyCMDsList[cmdIndex]-> code = i;
}

void printHistoryCMD(Command *cmd){
	switch(cmd -> time -> tm_wday){
		//0-6
		case 0:
		printf("\nSun ");
		break;
		case 1:
		printf("\nMon ");
		break;
		break;
		case 2:
		printf("\nTue ");
		break;
		case 3:
		printf("\nWed ");
		break;
		case 4:
		printf("\nThu ");
		break;
		case 5:
		printf("\nFri ");
		break;
		case 6:
		printf("\nSat ");
		break;
	}
	switch(cmd -> time -> tm_mon){
		//0-11
		case 0:
		printf("Jan ");
		break;
		case 1:
		printf("Feb ");
		break;
		case 2:
		printf("Mar ");
		break;
		case 3:
		printf("Apr ");
		break;
		case 4:
		printf("May ");
		break;
		case 5:
		printf("Jun ");
		break;
		case 6:
		printf("Jul ");
		break;
		case 7:
		printf("Aug ");
		break;
		case 8:
		printf("Sep ");
		break;
		case 9:
		printf("Oct ");
		break;
		case 10:
		printf("Nov ");
		break;
		case 11:
		printf("Dec ");
		break;
	}
	printf("%d ", cmd -> time -> tm_mday);
	printf("%02d:%02d:%02d ", cmd -> time -> tm_hour, cmd -> time -> tm_min, cmd -> time -> tm_sec);
	printf("%d ",(cmd -> time -> tm_year)+1900 );
	printf("\n%s", cmd -> name);
	printf("%d ", cmd -> code );
}
int changeTheme(char **arguments){
	//Supports 2 themes and ability to revert to default.
	if(strcasecmp(arguments[1], "red") == 0){
		printf(THEME_RED);
		return 0;
	} else if(strcasecmp(arguments[1], "blue") == 0){
		printf(THEME_BLUE);
		return 0;
	} else if(strcasecmp(arguments[1], "reset") == 0){
		printf(THEME_RESET);
		return 0;
	} else{
		printf("%s\n", "Theme not supported.");
		return -1;
	}
}

int print(char **arguments, int argCount){

	if(strstr(arguments[1], "$")){
		// print $var
		for(int i =0; i < numEnvVar; i++){
			if(strcmp(arguments[1], envVarList[i]-> name) == 0){
				printf("%s\n", envVarList[i]-> value);
				return 0;
			}
		}
		return -1;

	} else{
		for(int i = 1; i < argCount; i++){
			printf("%s", arguments[i]);
			printf("%s", " ");
		}
		printf("\n");
		return 0;
	}
}