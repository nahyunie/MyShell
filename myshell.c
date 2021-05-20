#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#define SIZE 512
 
/* 백그라운드 프로세스 정보를 저장하기 위한 구조체 */
struct backpro{
    char process_name[SIZE];
    int proid;
};
 
/* 백그라운드 프로세스 리스트를 저장할 배열 */
struct backpro pros[SIZE]={NULL};
 
int amper=0;
char cmd[512];
int i=0;
int fd;
int num=0;
 
char name[512];
 
int mainpid;
int fg=0;
 
int main(int argc, char* argv[]){
    while(1){
        printf("[prompt] $ ");
        fgets(cmd,SIZE,stdin);
        cmd[strlen(cmd)-1]='\0';
        /* exit이나 logout을 입력하면 바로 종료 */
        if(strcmp(cmd, "exit")==0)
            exit(1);
        else if(strcmp(cmd,"logout")==0)
            exit(1);
        /* 종료 명령어가 아니면 execute() 실행 */
        execute(cmd);
    }
 
}
 
/* 종료된 자식 프로세스의 시그널을 처리하기 위한 핸들러 */
void sighandler(){
    int pid;
    int status;
    pid = waitpid(-1, &status, WNOHANG);
    /* foreground에서 실행되고 있는 프로세스라면 */
    if(pid==mainpid){
        fg=0; //  fg 상태를 없다고 making
        return;
    }
 
	for(int i=0;i<num;i++){
		/* 백그라운드 프로세스 리스트에 있는 pid와 같다면 */
		/* 해당 리스트 원소를 삭제하고 나머지 원소들을 앞으로 이동 */
		if(pros[i].proid==pid){
			num--;
			memmove(&pros[i], &pros[i+1], sizeof(pros[i]));
		}
	}
}
/* 처음 시작하는 함수 */
void execute(char *cmd){
    int i=0;
    int status;
    int pid;
    
    char pros_name[SIZE];
    int pros_id;
    
    char msg[SIZE];
    int prosfiledes[2];
 
    char firstword[SIZE];
 
    /* pipe를 만들어줌 자식 프로세스의 pid를 받기위해 프로세스 간 통신 */
    pipe(prosfiledes);
 
    /* 명령어가 myjobs면 백그라운드 프로세스 리스트 출력 */
    if(strcmp(cmd,"myjobs")==0)
        showmyjobs();
    for(i=0;i<strlen(cmd);i++){
        /* 만약 백그라운드 실행이면 */
        if(cmd[i]=='&'){
            /* amper를 1로 만들고 맨뒤 &를 없앰 */
            amper=1;
            cmd[i]='\0';
            //printf("child name : %s\n", cmd);
            break;
        }
        else amper=0;
    }
 
    if(pid=fork()==0){ // 자식이면
        /* redirect가 있는지 확인하는 함수 */
        redirect(cmd);
        /* pipe가 있는지 확인하는 함수 */
        haspipe(cmd, prosfiledes[1]);
        /* 실행하는 함수 */
        real_execute(cmd, prosfiledes[1]);
    }
    else{
        /* 부모 프로세스에게 자식 프로세스 종료 핸들러 설정 */
         signal(SIGCHLD, sighandler);    
        /* 자식 프로세스로부터 pid를 받음 */
         read(prosfiledes[0], msg, SIZE);
         /* foreground에서 실행되는 프로세스면 fg라는 메세지와 pid를 전송받고 */
         /* background에서 실행되는 프로세스면 프로세스 이름과 pid를 전송받음 */
         sscanf(msg, "%s %d", &firstword, &pros_id);
         /* 'fg'라는 문자열이 없으면(background 프로세스면) */
         if(strcmp(firstword,"fg")!=0){
             sscanf(msg, "%s %d", &pros_name, &pros_id);
             /* 백그라운드 프로세스 리스트에 추가함 */
             strcpy(pros[num].process_name, pros_name);
             pros[num].proid = pros_id;
             num++;
         }
         /* 'fg'라는 문자열이 있으면(foreground에서 실행하는 프로세스면) */
         else if(strcmp(firstword, "fg")==0){
             /* mainpid를 전송받은 pid로 설정(핸들러에서 foreground인지 비교하기 위함) */
             mainpid=pros_id;
         }
         /* foreground 프로세스라면 실행이 완료될 때 까지 기다림 */
         if(amper==0){
             fg=1;
             /* pause에서 깨어나도 foreground가 실행중이면 다시 pause()를 반복 */
             while(fg==1){
                 pause();
             }
         }
    }
            
}
/* 백그라운드 프로세스 리스트를 보여주는 함수 */
void showmyjobs(){
    int i=0;
    printf("======background process======\n");
    for(i=0;i<num;i++)
        printf("%s\t\t%d\t\n",pros[i].process_name, pros[i].proid);
    
    printf("==============================\n");
}
/* 실제로 프로세스 실행하는 함수 */
void real_execute(char* cmd, int filedes){
    char msg[SIZE];
    
    /* background 프로세스라면 부모에게 프로세스 이름과 pid를 보내줌(파이프를 통해 전달) */
    if(amper==1){
        sprintf(msg, "%s %d", cmd, getpid());
        write(filedes, msg, SIZE);
    }
    /* foreground 프로세스라면 부모에게 fg라는 문자열과 pid를 보내줌(파이프를 통해 전달) */
    else if(amper==0){
        sprintf(msg, "fg %d", getpid());
        write(filedes, msg, SIZE);
    }
    /* 프로그램 실행 */
    execlp(cmd, cmd, NULL);
    exit(1);
}
/* redirect가 있는지 */
void redirect(char* cmd){
    int i=0;
    char *separ_cmd = NULL;
    int fd;
    for(i=0;i<strlen(cmd);i++){
        /* 출력 재지정 */
        if(cmd[i]=='>'){
            cmd[i]='\0';
            /* 출력할 파일을 만들어줌 */
            fd = creat(&cmd[i+1],0644);
            /* 표준 출력을 close */
            close(stdout);
            /* 만든 파일 디스크립터를 표준 출력에 복제 */
            dup2(fd,STDOUT_FILENO);
            close(fd);
            break;
        }
        /* 입력 재지정 */
        else if(cmd[i]=='<'){
            cmd[i]='\0';
            /* 표준 입력으로 사용할 파일 open */
            fd = open(&cmd[i+1],O_RDONLY);
            /* 표준 입력 close */
            close(stdin);
            /* 열린 파일 디스크립터를 표준 입력에 복제 */
            dup2(fd,STDIN_FILENO);
            close(fd);
            break;
        }
    }
    
}
/* 파이프가 있는지 */
void haspipe(char* cmd, int pfiledes){
    int i=0;
    int filedes[2];
    int pid=0;
    char msg[SIZE];
 
    for(i=0;i<strlen(cmd);i++){
        /* 파이프가 있으면 */
        if(cmd[i]=='|'){
            cmd[i]='\0';
            /* 파이프 만들고 */
            pipe(filedes);
            if(pid=fork()==0){
                close(stdout);
                /* 쓰기용 파이프를 표준 출력에 복제  */
                dup2(filedes[1],STDOUT_FILENO);
                close(filedes[1]);
                close(filedes[0]);
                execlp(cmd, cmd, NULL);
                perror("pipe");
            }
            else{
                close(stdin);
                /* 읽기용 파이프를 표준입력에 복제 */
                dup2(filedes[0], STDIN_FILENO);
                close(filedes[0]);
                close(filedes[1]);
                /* 부모 프로세스에게 pid 알려줌(프로세스 종료 처리를 위해) */
                sprintf(msg,"fg %d",getpid());
                write(pfiledes, msg, SIZE);
                execlp(&cmd[i+1], &cmd[i+1], NULL);
                exit(1);
                perror("pipe");
            }
        }
    }
}
