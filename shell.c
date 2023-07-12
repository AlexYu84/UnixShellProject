/* 
 * by the user.
 * Name: Alex Yu
 * Email: yuale@oregonstate.edu
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
/* This function is called when the user specifies the "status" command which this function prints out
 * the exit value/status.
 * 
 * input: int exit status value
 * output: none
 */
void printStatus(int exitStat){
    if(WIFEXITED(exitStat)){
        printf("exit value %d\n", WEXITSTATUS(exitStat));
    }
    else{
        printf("terminated by signal %d\n", WTERMSIG(exitStat));
    }
}
/* This function is the funciton that forks the shell and runs all the commands given by the user.
 * It also creates background processes as well as handles input and output redirection
 * 
 * input: input file array, output file array, user input array, int background status, sigaction signal
 * output: none
 */
int permit = 1;
void commandCenter(char* inputArray[], char input[], char output[], int* backgroundStat, int* exitStat, struct sigaction sigint){
    char* arguments[] = {"ls", NULL};
    //begin fork code is similar to the code given to us in modules
    pid_t spawnPid = fork();

    switch(spawnPid){
        case -1:
            perror("fork()\n");
            exit(1);
            break;
        case 0:
            // default to where process will take in ^c
            sigint.sa_handler = SIG_DFL;
            sigaction(SIGINT, &sigint, NULL);
            
            // handling input redirection
            if(strcmp(input, "")){
                int inputNum = open(input, O_RDONLY);
                if(inputNum == -1){
                    perror("Couldn't open input file\n");
                    exit(1);
                }
                int result = dup2(inputNum, 1);
                if(result == -1){
                    perror("Couldn't assign input file\n");
                    exit(2);
                }
                fcntl(inputNum, F_SETFD, FD_CLOEXEC);
            }

            // handling output redirection(basically the same as handling inputs)
            if(strcmp(output, "")){
                int outputNum = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(outputNum == -1){
                    perror("Couldn't open input file\n");
                    exit(1);
                }
                int result = dup2(outputNum, 1);
                if(result == -1){
                    perror("Couldn't assign output file\n");
                    exit(2);
                }
                //close the file descriptor on exe
                fcntl(outputNum, F_SETFD, FD_CLOEXEC);
            }
            //execute program/command given by the user. Errors should be handled as well
            if(execvp(inputArray[0], inputArray)){
                printf("%s: no such file or directory\n", inputArray[0]);
                fflush(stdout);
                exit(2);
            }
            break;
        default:
            // execute the process in background mode when user enables so
            if(*backgroundStat && permit){
                pid_t tempPid = waitpid(spawnPid, exitStat, WNOHANG);
                printf("background pid is %d\n", spawnPid);
                fflush(stdout);
            }
            // if user hasn't specified background execution, run the process normally
            else{
                pid_t tempPid = waitpid(spawnPid, exitStat, 0);
            }
        //constantly check for when the background has been terminated
        while((spawnPid == waitpid(-1, exitStat, WNOHANG)) > 0){
            printf("child pid: %d, has been terminated\n", spawnPid);
            printStatus(*exitStat);
            fflush(stdout);
        }
    }
}

/* This function takes the user input and compares it to the symbols such as "< > & $$". If any of these are detected
 * this function handles all those. It then stores that user input into inputArray for other functions to use
 * 
 * input: input file array, output file array, user input array, int background status
 * output: none
 */
void userInput(char input[], char output[], char* inputArray[], int* backgroundStat){
    char hold[2048];

    printf(": ");
    fflush(stdout);
    fgets(hold, 2048, stdin);
    int characters = strlen(hold);
    hold[characters-1] = 0; // makes the new line NULL;
    char hold2[32];
    sprintf(hold2, "%d", getpid()); //get the pid value and turn it into a string
    //find out if its a black line inputted
    if(strcmp(hold, "") == 0){
        inputArray[0] = "";
        return;
    }
    
    const char spaces[2] = " ";
    char* saveptr;
    char* token = strtok_r(hold, spaces, &saveptr);

    int i = 0;
    while(token != NULL){
        //next 3 if else statements check for the 3 main symbols "<, >, and &"
        if(strcmp(token, "&") == 0){
            *backgroundStat = 1;
        }
        else if(strcmp(token, ">") == 0){
            token = strtok_r(NULL, token, &saveptr);
            strcpy(output, token);
        }
        else if(strcmp(token, "<") == 0){
            token = strtok_r(NULL, token, &saveptr);
            strcpy(input, token);
        }
        //none of the 3 main symbols were detected so that means its a normal command
        else{
            inputArray[i] = token;
            for(int j = 0; j < strlen(inputArray[i]); j++){
                if(inputArray[i][j] == '$' && inputArray[i][j+1] == '$'){
                    inputArray[i][j] = '\0';
                    strcat(inputArray[i], hold2);
                }
            }
        }
        token = strtok_r(NULL, spaces, &saveptr);
        i++;
    }
    inputArray[i] = NULL;
}

/* This function permits whether or not foreground-only mode is allowed
 * 
 * input: none
 * output: a print whether or not foreground-only mode is active or not
 */
void catch(){
    if(permit == 1){
        printf("Foreground-only mode enabled\n");
        fflush(stdout);
        permit = 0;
    }
    else{
        printf("Foreground-only mode disabled\n");
        fflush(stdout);
        permit = 1;
    }
}

/* This function is the function that runs everything. Calls user input, uses signals to ignore or change modes, 
 * calls the execute commands.
 * 
 * input: none
 * output: none
 */
void smallsh(){
    int exitStat = 0;
    int backgroundStat = 0;
    int run = 1;
    
    // This ignores the ^c command (ctrl c)
    struct sigaction sigint = {0};
    sigint.sa_handler = SIG_IGN;
    sigfillset(&sigint.sa_mask);
    sigint.sa_flags = 0;
    sigaction(SIGINT, &sigint, NULL);

    // this allows the user to switch from foreground-only mode by input ^z (ctrl z)
    struct sigaction sigtstp = {0};
    sigtstp.sa_handler = catch;
    sigfillset(&sigtstp.sa_mask);
    sigtstp.sa_flags = 0;
    sigaction(SIGTSTP, &sigtstp, NULL);

    //runs the whole small shell
    while(run == 1){
        //create input, output, and the user inputArray which will be what is eventually going to execvp
        char** inputArray = malloc(sizeof(char*)*2048);
        char input[2048] = "";
        char output[2048] = "";
        userInput(input, output, inputArray, &backgroundStat);
        // skip the comments/blanks
        int userInpLen = strlen(inputArray[0]); //find out if the user input starts with a comment or is a blank
        //required a 2d array incase they typed something after the # (example #sdfasdf)
        if(inputArray[0][0] == '#' || userInpLen <= 0){
            continue;
        }
        // find out whether or not the user is trying to call the first function - cd
        else if(strcmp(inputArray[0], "cd") == 0){
            // if theres something following the cd exists. Looking for the directory name
            if(inputArray[1]){
                // if theres no such directory, tell them
                if(chdir(inputArray[1]) == -1){
                    printf("no such directory exists\n");
                    //fflush everytime we printf just incase
                    fflush(stdout);
                }
            }
            else{
                //if theres no directory specified by user return them back with getenv("HOME")
                chdir(getenv("HOME"));
            }
        }
        // second function - status
        else if(strcmp(inputArray[0], "status") == 0){
            // call the print exit status
            printStatus(exitStat);
        }
        // end program if user specified exit. first function - exit
        else if(strcmp(inputArray[0], "exit") == 0){
            run = 0;
            break;
        }
        // if none of the 3 commands were specified in the beginnings it probably a built in commands
        else{
            commandCenter(inputArray, input, output, &backgroundStat, &exitStat, sigint);
        }
        // reset the whole process by making things null or setting their values back to 0
        backgroundStat = 0;
        input[0] = '\0';
        output[0] = '\0';
    }
}

int main(){
    // printf("$ smallsh\n");
    smallsh();

    return 0;
}
