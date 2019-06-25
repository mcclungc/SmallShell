//Program 3 - smallsh
//Connie McClung
//CS344 400 Winter 2018

///include statements
#include <stdio.h> //printf, fprintf
#include <stdlib.h>//malloc,EXIT_SUCCESS, size_t
#include <sys/wait.h> //wait declaration
#include <string.h>//string operations
#include <unistd.h> // POSIX API access
#include <fcntl.h>//file control
#include <signal.h>//signal

//constants
#define MAXARGS 513 //make room for 512 arguments plus the NULL entry to pass to execvp
#define MAXCOMMANDSIZE 2048 //maximum command line length is 2048 per assignment spec

int foreground;//global bool to track foreground only mode

//function declarations
void stringReplace(char* sourceString, char* stringToReplace, char* replacementString);//helper function to replace $$ with process ID
void sigtstpHandle(int signo);//function to handle SIGTSTP signals

	
int main()
{
	//variable declarations
	char* commandLine = NULL;//string to store getline results from stdin
	char* cmdArgumentArray[MAXARGS];//array of arguments parsed from string
	char* argEntry = NULL;//pointer to individual entry in parsed array
	int argArraySize; //size of command array

	char* inputFile = NULL; //will hold filename for input redirect
	char* outputFile = NULL; //will hold filename for output redirect

	int fdInput; //file descriptor for opening file for input redirect
	int fdOutput; //file descriptor for opening file for output redirect


	int inBackground; //bool to track if process runs in background

	int status = 0; //for returning status of last process

	pid_t currentPid; // pid for fork

	int activeFlag = 1; // flag for command loop

	int i; //variable for looping

	//signal handling setup
	// SIGINT 		
	struct sigaction actionSigInt = {0};//initialize struct
	actionSigInt.sa_handler = SIG_IGN;//parent process will ignore SIGINT
	actionSigInt.sa_flags = 0; //not setting any flags
	sigaction(SIGINT, &actionSigInt,NULL);//register signal handler for SIGINT
	
	// SIGTSTOP setup using handler function
	struct sigaction actionSigtStop = {0};//initialize struct
	actionSigtStop.sa_handler = sigtstpHandle;//pass to handler function
	sigfillset(&actionSigtStop.sa_mask);
	actionSigtStop.sa_flags = SA_RESTART;//setting up as described in 3.3, to allow getline to resume wait for input instead of returning with -1 after signal
	sigaction(SIGTSTP, &actionSigtStop, NULL);//register signal handler for SIGTSTP

	foreground = 0; //foreground only switch, only turned on by sigtstp signal
	
	while (activeFlag) //keep command loop active until flag is changed
	{
		//if last foreground child process terminated by signal, return status
		if ((WIFSIGNALED(status))&& (!inBackground)) // display terminating signal info
		{	
			printf("terminated by signal: %d\n", status);fflush(stdout);
		}	

		// before loop restarts, check for completed background child processes, placing at beginning of loop as recommended by professor
		currentPid = waitpid(-1, &status, WNOHANG);
		while(currentPid > 0)
		{//print background processe completed with exit status or terminating signal
			printf("Background pid %d is done: ", currentPid);fflush(stdout);
			if (WIFEXITED(status)) //terminated normally
			{
				printf("exit value: %d\n", WEXITSTATUS(status));fflush(stdout);
			}
			else//signal termination
			{	
				printf("terminated by  signal: %d\n", status); fflush(stdout);
			}
			currentPid = waitpid(-1, &status, WNOHANG);//keep checking for background child processes
		}			
		
		inBackground = 0; //by default, processes run in foreground
		fflush(stdout);//clear buffer just in case
		printf(": ");fflush(stdout); //print the prompt

		//use getline to read a string from stdin
		size_t bufferSize = MAXCOMMANDSIZE; //set input buffer to accomodate maximum command string length
		commandLine = malloc(bufferSize * sizeof(char));//allocate memory for command line string
		memset(commandLine, '\0', bufferSize); //initialize
		if (getline(&commandLine, &bufferSize, stdin)== -1) //if getlilne is unsuccessful, error 
		{
			fprintf(stderr,"Getline() failure\n");fflush(stderr);
			clearerr(stdin);//remove error status to prevent re-read loop
			//return;
		}

		//if blank line, don't pass to parser
		if (strcmp(commandLine, "\n") == 0) //blank line terminated with newline
		{
			continue;
		}
		
		//parse string to an array
		argArraySize = 0; //update array size counter after each string is added, this will be the number of arguments
		
		argEntry = strtok(commandLine, " \n"); //get first string separated by delimiter
		while (argEntry) //as long as strtok returns non-null value parse and add commands (or set flags for <, >, and &)
		{
			if(strcmp(argEntry, "<") == 0) // if parsed string is input redirect symbol
			{
				argEntry = strtok(NULL, " \n"); //get next parsed string, which contains the file name for input redirect
				inputFile = argEntry;//point inputFile to filename string
				argEntry = strtok(NULL, " \n");//parse the next string
			}
			else if(strcmp(argEntry, ">") == 0) //if output redirect symbol
			{
				argEntry = strtok(NULL, " \n");//get string containing file name for output redirect
				outputFile = argEntry; //point outputFile to filename string
				argEntry = strtok(NULL, " \n");//parse the next string
			}
			else 
			{
				//else, add parsed string to array of arguments
				cmdArgumentArray[argArraySize] = argEntry;
				//advance to next parsed string
				argEntry = strtok(NULL, " \n");
				argArraySize++;//increment array size
			}
		}
		//remove & if last arg and turn on background flag bool	
		if (strcmp(cmdArgumentArray[argArraySize - 1], "&") == 0)
		{
			inBackground = 1;
			argArraySize--;
		}
		
		//go through command array and expand $$ by searching for $$ and replacing with process id string
		for (i=0; i < argArraySize; i++)//loop through argument array
		{
			if (strstr(cmdArgumentArray[i],"$$") !=NULL)//if $$ found
			{
				//find a substring that leads up to occurence of $$ and use index of first $ to know where to insert replacement
				char * substringToExpand = strstr(cmdArgumentArray[i], "$$");//find substring that goes up to $$
				int indexBefore = substringToExpand - cmdArgumentArray[i];//figure out where to insert replacement pid string
			
				//create a string from process ID	
				unsigned int pidLength = sizeof(pid_t); //figure out how long process ids will be on this system
				pid_t processID = getpid();//get process id
				char * pidString = malloc(pidLength * sizeof(char));//allocate memory for a string that is long enough to hold process id
				memset(pidString, '\0', pidLength);//initialize pid string
				sprintf(pidString, "%d", processID);//generate string from pid
				//free(pidString);

				//replace all instances of $$ in string with process ID using helper function
				stringReplace(cmdArgumentArray[i], "$$", pidString);//for each argument in array, replace all occurrences of $$ with string pid		
			}
		}		
	
		// add last array entry, NULL
		cmdArgumentArray[argArraySize] = NULL;
	
		// determine if command is comment, builtin or not builtin
		
		// if comment, ignore 	
		if (strstr(cmdArgumentArray[0],"#") != NULL)
		{
			continue;
		}
		//if exit command, set flag to stop command loop, clean up and exit
		else if (strcmp(cmdArgumentArray[0], "exit") == 0)
		{
			activeFlag = 0;
			exit(0);//close all files, clean up,  and exit
		}
		//if status command, display exit status or terminating signal of last foreground command
		else if (strcmp(cmdArgumentArray[0], "status") == 0)
		{
			if(WIFEXITED(status)) //if exit status exists, display
			{
				printf("exit value: %d\n", WEXITSTATUS(status));fflush(stdout);
			}
			else //otherwise, display terminating signal info
			{
				printf("terminated by signal: %d\n", status);fflush(stdout);
			}
		}
		//if cd command
		else if (strcmp(cmdArgumentArray[0], "cd") == 0) 
		{
			if(argArraySize > 3)//if too many args, error message
			{
				fprintf(stderr, "Too many arguments for cd command\n"); fflush(stderr);
			}
			else if (cmdArgumentArray[1]==NULL) //if no argument, cd to  home dir listed in PATH
			{
				chdir(getenv("HOME"));
			}
			else //otherwise cd to directory passed as argument
			{
				chdir(cmdArgumentArray[1]);
			}
		}
		// if not a comment or builtin, handle fork, redirect and background, then exec command
		else
		{
			currentPid = fork(); //fork
			
			//if fork unsuccessful
			if (currentPid == -1)
			{
				//error message
				fprintf(stderr,"Error forking\n");fflush(stderr);
				status = 1;//set exit status and leave 
				break;
			}
			else if (currentPid == 0) //child process
			{
				if(!inBackground) // foreground child should terminate itself on receipt of sigint signal
				{
					actionSigInt.sa_handler = SIG_DFL;//default action for sigint is terminate
					actionSigInt.sa_flags = 0;
					sigaction(SIGINT, &actionSigInt, NULL);
				}
				//redirect input if input file provided or if child is running in background
				if(inputFile) //if input redirect, open file
				{
					fdInput = open(inputFile, O_RDONLY);//open for read only
					if (fdInput == -1)//if not opened successfuly, error message and set exit status to 1
					{
						fprintf(stderr,"Cannot open file %s for input\n", inputFile);fflush(stderr);
						exit(1);
					}
					if (dup2(fdInput, 0) == -1)
					{
						//if dup2 unsuccesful, error message and set exit status
						fprintf(stderr,"Error dup2 for input\n");fflush(stderr);
						exit(1);
					}					
					close(fdInput);//close file stream
				}
				else if(inBackground) //in background, redirect input to dev null
				{
					fdInput = open("/dev/null", O_RDONLY); //open dev/null for read
					if (fdInput == -1)//if not opened succesffully, error message and set exit status
					{
						fprintf(stderr,"Cannot open /dev/null for input\n" );fflush(stderr);
						exit(1);
					}
					if (dup2(fdInput, 0) == -1)
					{
						//if dup2 unsuccessful, error message and set exit status
						fprintf(stderr,"Error dup2 for input\n");fflush(stderr);
						exit(1);
					}					
					close(fdInput); //close file stream

				}
				if (outputFile)//if output redirect, open file
				{	//open output file for write only, truncate if exists, create if not exists
					fdOutput = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
					if (fdOutput == -1)//if file open unsuccessful, error message and set exit status
					{
						fprintf(stderr,"Cannot open file %s for output\n", outputFile);fflush(stderr);
						exit(1);
					}
					if (dup2(fdOutput, 1) == -1)
					{
						//if  dup2 unsuccesful error message and set exit status
						fprintf(stderr,"Error dup2 for output\n");fflush(stderr);
						exit(1);
					}					
					close(fdOutput);//close file stream
				}		
				
				// now that input output redirect is set up, execute command
				//if execvp can't execute command, error message and set exit status
				if (execvp(cmdArgumentArray[0],cmdArgumentArray) < 0)
				{
					fprintf(stderr,"Error processing command %s\n", cmdArgumentArray[0]);fflush(stderr);
					exit(1);
				}
			}
			else //if parent, wait for child process to return 
			{
				if((!inBackground)||(foreground))//if background flag not set or if foreground-only switch on
				{
					do{//call waitpid on child and wait for term or exit
						waitpid(currentPid, &status,0);
					} while (!WIFEXITED(status) && !WIFSIGNALED(status));
				}
				else//display process id of background process
				{
					printf("Background pid is: %d\n", currentPid);fflush(stdout);
				}
			}
		}					
			
		//clean up before next loop
		free(commandLine);
		for (i=0; i< argArraySize; i++)//loop through argument array
		{
			cmdArgumentArray[i] = NULL;//set char* at each index to null
		}

		inputFile = NULL;//set filename pointers back to null
		outputFile = NULL;//set filename pointers back to null
	}	

	return EXIT_SUCCESS; //exit main
}


// helper function to replace one substring with another (to expand $$ in this program)
//as explained on https://stackoverflow.com/questions/3659694/how-to-replace-substring-in-c
void stringReplace(char* sourceString, char* stringToReplace, char* replacementString)//takes string, original substring, replacement substring
{
	char * match = strstr(sourceString, stringToReplace);//find out if the string you are searching contains the string you seek
	do 
	{
		if(match)//if match exists
		{
			char tempBuffer[2048] = {0};;//allocate string buffer
				
			if(sourceString == match)
			{
				strcpy(tempBuffer, replacementString);//copy replacement string into buffer
				strcat(tempBuffer, (match + strlen(stringToReplace)));//cat the preceding portion of original string to replacement
			}
			else //or copy first part of original string then cat replacement string and remainder of original string
			{	
				strncpy(tempBuffer, sourceString, strlen(sourceString) - strlen(match));//first part 
				strcat(tempBuffer, replacementString);//cat replacement string
				strcat(tempBuffer, (match + strlen(stringToReplace)));//cat remainder
			}
			
			memset(sourceString, '\0', strlen(sourceString));// clear source string 
			strcpy(sourceString, tempBuffer);//and copy buffer contents to it

		}
	} while (match && (match = strstr(sourceString, stringToReplace)));//replace all instances of substring with replacement
	return;
}

// handler function for SIGTSTP to display specific message and set foreground only switch
void sigtstpHandle(int signo)//modeled on catch sigint function in 3.3 lecture
{
	if (!foreground)//if foreground-only mode is not currently set
	{
		char *notice = "\nEntering foreground-only (& is now ignored) \n";//message that we have entered a state where subsequent commands can no longer be run in background
		write(STDOUT_FILENO, notice, 46);//display informative message, use write because printf is non reentrant, exact # of chars (can't use strlen - not reentrant)
		foreground = 1;//turn on foreground-only flag
	}
	else//if already in foreground-only mode, switch back
	{	
		char *notice = "\nExiting foreground-only mode\n";//second sigtstp will restore normal condition where commands can run in background
		write(STDOUT_FILENO, notice, 30);//message, using write, not printf, not strlen, which are non-reentrant
		foreground = 0;//turn off foreground-only flag
	}
}

