#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#define MAX_CMD_ARG 20
#define BUFSIZE 256

const char *prompt = "myshell> ";
char* cmdvector[MAX_CMD_ARG];
char  cmdline[BUFSIZE];
int sig_flag;
static struct sigaction act;
pid_t wpid;
void fatal(char *str){
    perror(str);
    exit(1);
}

int makelist(char *s, const char *delimiters, char** list, int MAX_LIST){
    int i = 0;
    int numtokens = 0;
    char *snew = NULL;

    if( (s==NULL) || (delimiters==NULL) ) return -1;

    snew = s + strspn(s, delimiters);	/* Skip delimiters */
    if( (list[numtokens]=strtok(snew, delimiters)) == NULL )
        return numtokens;

    numtokens = 1;

    while(1){
        if( (list[numtokens]=strtok(NULL, delimiters)) == NULL)
            break;
        if(numtokens == (MAX_LIST-1)) return -1;
        numtokens++;
    }
    return numtokens;
}

int main(int argc, char**argv){
    int i=0;
    sig_flag = 0;
    pid_t pid;
    void catch_func(int);
    act.sa_handler = catch_func;
    sigfillset(&(act.sa_mask));
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGCHLD, &act, NULL);
    sigaction(SIGTSTP, &act, NULL);
    while (1) {
        if(sig_flag != 2){
            fputs(prompt, stdout);
        }
        sig_flag = 0;
        fgets(cmdline, BUFSIZE, stdin);
        if(sig_flag != 0){
            continue;
        }
        cmdline[strlen(cmdline) -1] = '\0';
        int token_size = makelist(cmdline, " \t", cmdvector, MAX_CMD_ARG);
        if(!strcmp(cmdvector[0], "cd")){
            if(token_size == 1){
                chdir(getenv("HOME"));
            }
            else if(token_size == 2){
                if(chdir(cmdvector[1])){
                    fatal("cd");
                }
            }
            else{
                printf("잘못된 입력입니다.\n");
            }
        }
        else if(!strcmp(cmdvector[0], "exit")){
            printf("shell 종료\n");
            exit(1);
        }
        else{
            switch(pid=fork()){
                case 0:
                    act.sa_handler = SIG_DFL;
                    sigaction(SIGINT, &act, NULL);
                    sigaction(SIGQUIT, &act, NULL);
                    sigaction(SIGTSTP, &act, NULL);
                    for(int i = 0; i < token_size; i++){
                        if(!strcmp(cmdvector[i], "|")){
                            int pp[2];
                            pipe(pp);
                            pid_t npid = fork();
                            if(npid == 0){
                                char* newcmd[MAX_CMD_ARG];
                                for(int j = 0; j < token_size; j++){
                                    if(j < i){
                                        newcmd[j] = cmdvector[j];
                                    }
                                    else if(j == i){
                                        newcmd[j] = '\0';
                                        break;
                                    }
                                }
                                dup2(pp[1], STDOUT_FILENO);
                                close(pp[1]);
                                close(pp[0]);
                                execvp(newcmd[0], newcmd);
                                fatal("main()");
                            }
                            else if(npid > 0){
                                dup2(pp[0], STDIN_FILENO);
                                close(pp[1]);
                                close(pp[0]);
                                wait(NULL);
                                for(int j = 0; j < token_size; j++){
                                    if(j > i){
                                        cmdvector[j - (i + 1)] = cmdvector[j];
                                    }
                                }
                                int new_token = token_size - (i + 1);
                                for(int j = new_token; j < token_size; j++){
                                    cmdvector[j] = '\0';
                                }
                                token_size = new_token;
                                i = 0;
                            }
                        }
                        else if(!strcmp(cmdvector[i], ">")){
                            int filedes = open(cmdvector[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                            if(filedes == -1){
                                fatal("fileopen");
                            }
                            dup2(filedes, STDOUT_FILENO);
                            close(filedes);
                            cmdvector[i] = '\0';
                        }
                        else if(!strcmp(cmdvector[i], "<")){
                            int filedes = open(cmdvector[i + 1], O_RDONLY);
                            dup2(filedes, STDIN_FILENO);
                            close(filedes);
                            cmdvector[i] = '\0';
                        }
                    }
                    if(!strcmp(cmdvector[token_size - 1], "&")){
                        act.sa_handler = SIG_IGN;
                        sigaction(SIGINT, &act, NULL);
                        sigaction(SIGQUIT, &act, NULL);
                        sigaction(SIGTSTP, &act, NULL);
                        cmdvector[token_size - 1] = '\0';
                        pid_t ppid = fork();
                        if(ppid >  0){
                            exit(1);
                        }
                    }
                    execvp(cmdvector[0], cmdvector);
                    fatal("main()");
                case -1:
                    fatal("main()");
                default:
                    while((wpid = wait(NULL)) > 0);
                    sig_flag = 0;
            }
        }
    }
    return 0;
}
void catch_func(int signo){
    sig_flag = 1;
    if(signo == SIGCHLD){
        sig_flag = 2;
        wait(NULL);
    }

    else{
        printf("\n");
    }
}
