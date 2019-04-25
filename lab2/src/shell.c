/* TLFShell programed by TheLitFire
 * Last editing Date&Time: 19.04.25 17:45
 * Note that ERROR DETECTING is NOT well programed and is only used for normal-case debugging.
 * Pipes and input/output redirections together may cause poor-defined behaviors.
*/
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<stdlib.h>
#include<errno.h>

//variable//
char cmd[8192],*args[4096],buf[8192];

//function//
void run_builtin(char *args[]){
	if (strcmp(args[0],"cd")==0){//cd Built-in
		if (args[1]&&*args[1]&&chdir(args[1])==-1) perror("chdir: ");
	}else if (strcmp(args[0],"pwd")==0){//pwd Built-in
		char wd[4096];char *ptr=getcwd(wd,4096);
		if (ptr) puts(ptr);
		else perror("getcwd: ");
	}else if (strcmp(args[0],"export")==0){//export Built-in
		char *ptr=args[1];if (!ptr) return;
		for (;*ptr&&*ptr!='=';++ptr);
		if (*ptr=='='){
			*ptr++='\0';
			if (setenv(args[1],ptr,1)==-1) perror("setenv: ");
		}
	}else if (strcmp(args[0],"exit")==0) exit(0);//exit Built-in
}

int cmd_builtin(char *cmd){
	return strcmp(cmd,"cd")==0||strcmp(cmd,"pwd")==0||
		   strcmp(cmd,"export")==0||strcmp(cmd,"exit")==0;
}

void redirection_restore(int rredir,int wredir){
	if (rredir){
		dup2(rredir,STDIN_FILENO);
		close(rredir);
	}
	if (wredir){
		dup2(wredir,STDOUT_FILENO);
		close(wredir);
	}
}

//solve//
int main(void){
	while (1){
		//get command
		memset(cmd,0,sizeof cmd);
		memset(args,0,sizeof args);
		printf("# ");fflush(stdin);
		fgets(cmd,4096,stdin);
		for (int i=0;;++i) if (cmd[i]=='\n'){
			cmd[i]='\0';
			break;
		}
		//extra space for special syntax "|", "<", "<<", "<<<", ">", ">>"
		for (char *cur=cmd;*cur;++cur){
			if (*cur=='|'){
				size_t len=strlen(cur+1);memmove(cur+3,cur+1,len);
				*cur=' ';*(cur+1)='|';*(cur+2)=' ';cur+=2;
			}else if (*cur=='<'){
				if (*(cur+1)=='<'){
					if (*(cur+2)=='<'){//"<<<"
						size_t len=strlen(cur+3);memmove(cur+5,cur+3,len);
						*cur=' ';*(cur+3)='<';*(cur+4)=' ';cur+=4;
					}else{//"<<"
						size_t len=strlen(cur+2);memmove(cur+4,cur+2,len);
						*cur=' ';*(cur+2)='<';*(cur+3)=' ';cur+=3;
					}
				}else{//"<"
					size_t len=strlen(cur+1);memmove(cur+3,cur+1,len);
					*cur=' ';*(cur+1)='<';*(cur+2)=' ';cur+=2;
				}
			}else if (*cur=='>'){
				if (*(cur+1)=='>'){//">>"
					size_t len=strlen(cur+2);memmove(cur+4,cur+2,len);
					*cur=' ';*(cur+2)='>';*(cur+3)=' ';cur+=3;
				}else{//">"
					size_t len=strlen(cur+1);memmove(cur+3,cur+1,len);
					*cur=' ';*(cur+1)='>';*(cur+2)=' ';cur+=2;
				}
			}
		}
		//separate cmd into args
		for (args[0]=cmd;*args[0]&&(*args[0]<=' '||*args[0]==127);++args[0]);
		for (int i=0,flag=0;*args[i];++i,flag=0)
			for (args[i+1]=args[i]+1;*args[i+1];++args[i+1]){
				while (*args[i+1]&&(*args[i+1]<=' '||*args[i+1]==127)){
					flag=1;
					*args[i+1]++='\0';
				}
				if (flag) break;
			}
		int cmdtot;
		for (cmdtot=0;args[cmdtot];++cmdtot) if (*args[cmdtot]=='\0'){
			args[cmdtot]=NULL;break;
		}
		//null command
		if (!args[0]) continue;
		//cut commands with pipe '|'
		for (int cmdptr=0,pipeptr=0,cur_read=0,cur_write=0,pre_read=0;cmdptr<cmdtot;cmdptr=pipeptr+1,pre_read=cur_read,cur_write=cur_read=0){
			for (pipeptr=cmdptr;args[pipeptr]&&strcmp(args[pipeptr],"|");++pipeptr);
			if (args[pipeptr]){//find '|' and cut commands
				args[pipeptr]=NULL;
				int pipefd[2];
				pipe(pipefd);
				cur_read=pipefd[0];cur_write=pipefd[1];//cur_read is used for next command, while pre_read is used for current command
			}
			//input&output redirection symbol
			int rredir=0,wredir=0;
			for (int redirptr=cmdptr;redirptr<pipeptr;++redirptr){
				if (!strcmp(args[redirptr],"<")){//input redirect file "r"
					rredir=dup(STDIN_FILENO);
					if (freopen(args[redirptr+1],"r",stdin)==NULL) perror("freopen \"r\": ");
					args[redirptr]=NULL;
				}else if (!strcmp(args[redirptr],"<<")){//input redirect multiple-lines
					rredir=dup(STDIN_FILENO);
					char temp[128]="/tmp/shell-XXXXXX";
					int temp_fd=mkstemp(temp);
					FILE *ofp=fdopen(temp_fd,"w");
					while (1){
						putchar('>');
						fgets(buf,8192,stdin);buf[strlen(buf)-1]='\0';
						if (!strcmp(buf,args[redirptr+1])) break;
						buf[strlen(buf)]='\n';fputs(buf,ofp);
					}
					fclose(ofp);
					if (freopen(temp,"r",stdin)==NULL) perror("freopen \"r\": ");
					remove(temp);
					args[redirptr]=NULL;
				}else if (!strcmp(args[redirptr],"<<<")){//input redirect single-line
					rredir=dup(STDIN_FILENO);
					char temp[128]="/tmp/shell-XXXXXX";
					int temp_fd=mkstemp(temp);
					FILE *ofp=fdopen(temp_fd,"w");
					fputs(args[redirptr+1],ofp);fputc('\n',ofp);
					fclose(ofp);
					if (freopen(temp,"r",stdin)==NULL) perror("freopen \"r\": ");
					remove(temp);
					args[redirptr]=NULL;
				}else if (!strcmp(args[redirptr],">")){//output redirect file "w"
					wredir=dup(STDOUT_FILENO);
					if (freopen(args[redirptr+1],"w",stdout)==NULL) perror("freopen \"w\": ");
					args[redirptr]=NULL;
				}else if (!strcmp(args[redirptr],">>")){//output redirect file "a"
					wredir=dup(STDOUT_FILENO);
					if (freopen(args[redirptr+1],"a",stdout)==NULL) perror("freopen \"a\": ");
					args[redirptr]=NULL;
				}
			}
			//close pipe if redirection exists
			if (rredir){
				if (pre_read){
					close(pre_read);pre_read=0;
				}
			}
			if (wredir){
				if (cur_write){
					close(cur_write);cur_write=0;
				}
			}
			//handle execution with pipe or IO redirection
			int isbuiltin=cmd_builtin(args[cmdptr]);
			if (cur_read||pre_read){//pipe
				pid_t pid=fork();
				if (!pid){//child
					if (pre_read){
						dup2(pre_read,STDIN_FILENO);
						close(pre_read);
					}
					if (cur_write){
						dup2(cur_write,STDOUT_FILENO);
						close(cur_write);
					}
					if (cur_read) close(cur_read);
					if (isbuiltin){
						run_builtin(args+cmdptr);
						return 0;
					}else{
						execvp(args[cmdptr],args+cmdptr);
						perror("Externel function error");
						return 255;
					}
				}else{//parent
					if (pre_read) close(pre_read);
					if (cur_write) close(cur_write);
					if (!cur_read){
						int status;
						waitpid(pid,&status,0);
					}
					redirection_restore(rredir,wredir);
				}
			}else{//no pipe
				if (isbuiltin){
					run_builtin(args+cmdptr);
					redirection_restore(rredir,wredir);
				}else{
					pid_t pid=fork();
					if (!pid){
						execvp(args[cmdptr],args+cmdptr);
						perror("Externel function error");
						return 255;
					}else{
						int status;
						waitpid(pid,&status,0);
						redirection_restore(rredir,wredir);
					}
				}
			}
		}
		//END FOR: cut commands with pipe '|'
	}
}

