#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<dirent.h>
#include<pwd.h>
#include<wait.h>
#include<sys/types.h>
#include <readline/readline.h>
#include <readline/history.h>


//define the printf color
#define L_GREEN "\e[1;32m"
#define L_BLUE  "\e[1;34m"
#define L_RED   "\e[1;31m"
#define WHITE   "\e[0m"

#define TRUE 1
#define FALSE 0
#define MAXPIDTABLE 32767


char lastdir[100];
char command[BUFSIZ];
char argv[100][100];
char **argvtmp1;
char **argvtmp2;
char argv_background[100];
char argv_redirect[100];
char argv_redirect1[100];
char oldpwd0[100];
char oldpwd1[100];
int  argc;
int  cdflag=0;
int BUILTIN_COMMAND = 0;
int PIPE_COMMAND = 0;
int BACKGROUND_COMMAND = 0;
int REDIRECT_COMMAND = 0;
int REDIRECT_COMMAND1 = 0;
//set the prompt
void set_prompt(char *prompt);
//analysis the command that user input
int analysis_command();
void builtin_command();
void do_command();
//print help information
void help();
void initial();
void init_lastdir();
void history_setup();
void history_finish();
void display_history_list();
void alias();
void unalias();
void cd_operation(struct passwd* pwp);
void echo_operation();

int main(){
	char prompt[BUFSIZ];
	char *line;
	
	init_lastdir();
	history_setup();	
	while(1) {
		set_prompt(prompt);
		if(!(line = readline(prompt))) 
			break;
		if(*line)
			add_history(line);
		strcpy(command, line);
		//strcat(command, "\n");
		if(!(analysis_command())){
			//todo deal with the buff
			if(BUILTIN_COMMAND){
				builtin_command();		
			}//if
			else{
				do_command();
			}//else		
		}//if analysis_command
		initial();//initial 
	}//while
	history_finish();
	
	return 0;
}

//set the prompt
void set_prompt(char *prompt){
	char hostname[100];
	char cwd[100];
	char super = '#';
	//to cut the cwd by "/"	
	char delims[] = "/";	
	struct passwd* pwp;
	
	if(gethostname(hostname,sizeof(hostname)) == -1){
		//get hostname failed
		strcpy(hostname,"unknown");
	}//if
    //getuid():用来取得执行目前进程的用户识别码。
	//getpwuid():根据传入的用户ID返回指向passwd的结构体 该结构体初始化了里面的所有成员 
	pwp = getpwuid(getuid());	
	//getcwd():将当前工作目录的绝对路径复制到参数buffer所指的内存空间中,成功则返回当前工作目录	
	if(!(getcwd(cwd,sizeof(cwd)))){
		//get cwd failed
		strcpy(cwd,"unknown");	
	}//if
	char cwdcopy[100];
	strcpy(cwdcopy,cwd);
	//strtok():该函数返回被分解的第一个子字符串，如果没有可检索的字符串，则返回一个空指针。
	//首次调用时，s必须指向要分解的字符串，随后调用要把s设成NULL。
	char *first = strtok(cwdcopy,delims);
	char *second = strtok(NULL,delims);
	//if at home 
	if(!(strcmp(first,"home")) && !(strcmp(second,pwp->pw_name))){
		int offset = strlen(first) + strlen(second)+2;
		char newcwd[100];
		char *p = cwd;
		char *q = newcwd;

		p += offset;
		while(*(q++) = *(p++));
		char tmp[100];
		strcpy(tmp,"~");
		strcat(tmp,newcwd);
		strcpy(cwd,tmp);			
	}	
	
	if(getuid() == 0)//if super
		super = '#';
	else
		super = '$';
	sprintf(prompt, "\001\e[1;31m\002[MyShell]\001\e[1;32m\002%s@%s\001\e[0m\002:\001\e[1;34m\002%s\001\e[0m\002%c",pwp->pw_name,hostname,cwd,super);	
	
}

char *is_alias(char *nickname,char *cc){
	FILE *fp;
	char *p;
	char szTest[1000];
	char *tmp;
	char *h="heihei";
	int location;
	int flag = 0;
	fp=fopen("/home/wangtian/alias.txt","r");
	while(!feof(fp)){
		memset(szTest,0,sizeof(szTest));
		fgets(szTest,sizeof(szTest)-1,fp);
		if(strcmp(szTest,"")==0)break;
		tmp = strtok(szTest,"=");
		//printf("%s 0 %s\n",nickname,tmp);
		if(strcmp(nickname,tmp)==0){//found    //若找到一致的别名，则将其对应的shell命令赋给返回值 
			//printf("hha");
			cc = strtok(NULL,"'");
			flag =1;
			break;
		}
	}
	if(flag==0) cc=h;   //如果没有找到，则将表示没有的字符串赋给返回值
	//printf("cc:%spp\n",cc);
	fclose(fp);
	return cc;
}


//分析用户输入的命令
int analysis_command(){    
	int i = 1;
	char *p;
	//用空格作为分隔符
	char delims[] = " ";
	argc = 1;
	
	strcpy(argv[0],strtok(command,delims));                //strtok 分解字符串command 为一组字符串，delims为分隔符（空格）
	while(p = strtok(NULL,delims)){
		strcpy(argv[i++],p);
		argc++;
	}
	//如果是内置命令
	if(!(strcmp(argv[0],"exit"))||!(strcmp(argv[0],"help"))|| !(strcmp(argv[0],"cd"))||!(strcmp(argv[0],"history"))){   
		BUILTIN_COMMAND = 1;	
	}            //strcmp 比较两个字符串，相同返回0
	int j;
	//判断是否为管道命令
	int pipe_location;
	for(j = 0;j < argc;j++){
		if(strcmp(argv[j],"|") == 0){    //strcmp相同放回0
			PIPE_COMMAND = 1;
			pipe_location = j;				
			break;
		}	
	}//for
	
	//判断是否为输出重定向命令
	int redirect_location;
	for(j = 0;j < argc;j++){
		if(strcmp(argv[j],">") == 0){
			REDIRECT_COMMAND = 1;
			redirect_location = j;				
			break;
		}
	}
	//判断是否为输入重定向命令
	int redirect_location1;
	for(j = 0;j < argc;j++){
		if(strcmp(argv[j],"<") == 0){
			REDIRECT_COMMAND1 = 1;
			redirect_location1 = j;				
			break;
		}
	}
	
//如果是管道命令
	if(PIPE_COMMAND){
		//command 1
		argvtmp1 = malloc(sizeof(char *)*pipe_location + 1);   //malloc分配内存空间
		int i;	
		for(i = 0;i < pipe_location + 1;i++){
			argvtmp1[i] = malloc(sizeof(char)*100);        //argvtmp1[i]是以空格分割后的指令数组
			if(i <= pipe_location)
				strcpy(argvtmp1[i],argv[i]);	
		}
		argvtmp1[pipe_location] = NULL;         //参数列表最后一行必须是NULL
		
		//command 2
		argvtmp2 = malloc(sizeof(char *)*(argc - pipe_location));
		int j;	
		for(j = 0;j < argc - pipe_location;j++){
			argvtmp2[j] = malloc(sizeof(char)*100);
			if(j <= pipe_location)
				strcpy(argvtmp2[j],argv[pipe_location + 1 + j]);	
		}
		argvtmp2[argc - pipe_location - 1] = NULL;               //参数列表最后一行必须是NULL
		
	}
//如果是重定向命令
	else if(REDIRECT_COMMAND){       //输出重定向命令>
		strcpy(argv_redirect,argv[redirect_location + 1]);
		argvtmp1 = malloc(sizeof(char *)*redirect_location + 1);
		int i;	
		for(i = 0;i < redirect_location + 1;i++){
			argvtmp1[i] = malloc(sizeof(char)*100);
			if(i < redirect_location)
				strcpy(argvtmp1[i],argv[i]);	
		}
		argvtmp1[redirect_location] = NULL;
	}
	
	else if(REDIRECT_COMMAND1){             //输入重定向命令<
		strcpy(argv_redirect1,argv[redirect_location1 + 1]);
		argvtmp1 = malloc(sizeof(char *)*redirect_location1 + 1);
		int i;	
		for(i = 0;i < redirect_location1 + 1;i++){
			argvtmp1[i] = malloc(sizeof(char)*100);
			if(i < redirect_location1)
				strcpy(argvtmp1[i],argv[i]);	
		}//for
		argvtmp1[redirect_location1] = NULL;
	}//redirect command

	else{
		argvtmp1 = malloc(sizeof(char *)*argc+1);          //分配内存空间
		int i;	
		for(i = 0;i < argc + 1;i++){
			argvtmp1[i] = malloc(sizeof(char)*100);
			if(i < argc)
				strcpy(argvtmp1[i],argv[i]);	
		}//for
		argvtmp1[argc] = NULL;
	}

	return 0;
}


void builtin_command(){
	struct passwd* pwp;
	char *q;
	//exit when command is exit	
	if(strcmp(argv[0],"exit") == 0){
		exit(EXIT_SUCCESS);
	}
	else if(strcmp(argv[0],"help") == 0){
		help();
	}//else if
	else if(strcmp(argv[0],"history") == 0){
		display_history_list();
	}//else if	
	else if(strcmp(argv[0],"cd") == 0){
		cd_operation(pwp);   
	}//else if cd
	else if(strcmp(argv[0],"alias") == 0){alias();}
	else if(strcmp(argv[0],"unalias") == 0){q = argv[1];unalias(q);}
	else if(strcmp(argv[0],"echo") == 0){echo_operation();}
}

void unalias(char *nickname){
	FILE *fp1,*fp2;
	char szTest[1000]={0};
	char tmp[1000];
	char tp[1000];
	char copy[100][1000];
	char p[1000];
	char *q;
	int i = 0, cnt;
	fp1 = fopen("/home/wangtian/alias.txt","r");
	while(!feof(fp1)){
		memset(szTest,0,sizeof(szTest));
		fgets(szTest,sizeof(szTest)-1,fp1);
		strcpy(p,szTest);
		if(strcmp(szTest,"")==0)break;
		q = strtok(szTest,"=");
		//strcpy(tmp,q);
		if(strcmp(nickname,q)!=0)
			strcpy(copy[i++],p);
	}   //以写入方式打开alias.txt将copy中的字符串逐行写入
	cnt = i;
	fclose(fp1);
	fp2 = fopen("/home/wangtian/alias.txt","w");
	i = 0;
	while(i<cnt){
		fprintf(fp2,"%s",copy[i++]);
	}
	fclose(fp2);
				
}

void alias(){
	char *p;
	char szTest[1000];
	char nickname[1000];
	char inname[1000];
	char delims2[] = "'";
	char tmp[1000]="";
	char tp[1000];
	int i = 2;
	int location = 0;
	argc = 1;
	FILE *fp;
	
	//printf("argv[1]:%s\n",argv[1]);
	while(strcmp(argv[i],"")){          //将argv[1]即以后的数组拼接起来赋给argv[1]
		strcat(argv[1]," ");
		strcat(argv[1],argv[i]);
		i++;
		argc++;
	}
	//printf("alias");
	//printf("argv[0]:%s\n",argv[0]);
	//printf("argv[1]:%s\n",argv[1]);
	if(!strcmp(argv[1],"")){          //当alias后没有其他命令，及执行alias查询功能
		printf("alias:\n");
		fp=fopen("/home/wangtian/alias.txt","r");    //打开alias.txt逐行读取并输出
		while(!feof(fp)){
			memset(szTest,0,sizeof(szTest));
			fgets(szTest,sizeof(szTest)-1,fp);
			if(strcmp(szTest,"")==0)break;
			printf("alias ");
			printf("%s",szTest);
		}
		fclose(fp);
	}
	else{       //当alias后有其他命令，及执行alias添加别名
		p = argv[1];
		fp=fopen("/home/wangtian/alias.txt","a");     //打开alias.txt并追加写入别名
		fprintf(fp,"%s\n",argv[1]);
		fclose(fp);
		while(*p != '='){
			p++;
			location++;
		}
		p = argv[1];
		strncpy(nickname,p,location);
	
		strcpy(tmp,strtok(argv[1],delims2));
		p = strtok(NULL,delims2);
		strcpy(inname,p);	
			
	}
}



void do_command(){
	//do_command
	pid_t pid2;
	if(PIPE_COMMAND){
		int fd[2],res;
		int status;
		res = pipe(fd);//创建管道
		if(res == -1)
			printf("pipe failed !\n");
		pid_t pid1 = fork();//创建子进程1
		if(pid1 == -1){
			printf("fork failed !\n");		
		}//if
		else if(pid1 == 0){
				dup2(fd[1],1);//复制标准输入
				close(fd[0]);//关闭读的一端
				if(execvp(argvtmp1[0],argvtmp1) < 0){
					printf("%s:command not found\n",argvtmp1[0]);		
				}//if			
		}//子进程1
		else{
			waitpid(pid1,&status,0);
			pid_t pid2 = fork();//创建子进程2
			if(pid2 == -1){
				printf("fork failed !\n");		
			}//if
			else if(pid2 == 0){
				close(fd[1]);//关闭写的一端
				dup2(fd[0],0);//复制标准输入
				if(execvp(argvtmp2[0],argvtmp2) < 0){
					printf("%s:command not found\n",argvtmp2[0]);		
				}//if
			}//子进程2
			else{
				close(fd[0]);//关闭读的一端
				close(fd[1]);//关闭写的一端
				waitpid(pid2,&status,0);
			}//else 
		}//父进程
	}//if pipe command
    else if(BACKGROUND_COMMAND){
		pid_t pid = fork();	 //创建子进程
		if(pid == -1){
			printf("fork failed !\n");		
		}//if
		else if(pid != 0){
			pid_t pid2 = fork();          //创建子进程2
			if(pid2 == -1){
				printf("fork failed !\n");		
			}//if
			else if(pid2 == 0){
				printf("后台进程pid:%u\n",getpid());   //输出子进程2后台运行的PID
				int i;
				int process_pid[MAXPIDTABLE]={0};
				for(i=0;i<MAXPIDTABLE;i++)
					if(process_pid[i]==0){
						process_pid[i]=pid;   //存储子进程号
						break;
					}
				if(i==MAXPIDTABLE){
					printf("Too much background processes!\n");
				}	
			}//else if pid2 == 0
			else{
				int pidReturn = wait(NULL);
			}//else 	
			
		}//else if 
		else{
			waitpid(pid2,NULL,0);
			int background_flag = 0;
			if(execvp(argvtmp1[0],argvtmp1) < 0){
				background_flag = 1;//execvp this background command failed		
			}//if
			if(background_flag){
				printf("%s:command not found\n",argvtmp1[0]);
			}//background flag			
		}//else 
	}//else if background command 
	else if(REDIRECT_COMMAND){      //输入重定向
		pid_t pid = fork();	  //创建子进程
		if(pid == -1){
			printf("fork failed !\n");		
		}//if
		else if(pid == 0){
			int redirect_flag = 0;
			FILE* fstream;            
			fstream = fopen(argv_redirect,"w+");           //输入的文件名传递变量并打开，进行写操作
			freopen(argv_redirect,"w",stdout);
			if(execvp(argvtmp1[0],argvtmp1) < 0){
				redirect_flag = 1;//execvp this redirect command failed		
			}//if
			fclose(stdout);          //关闭，结束标准输入
			fclose(fstream);
			if(redirect_flag){
				printf("%s:command not found\n",argvtmp1[0]);
			}//redirect flag	
				
		}//else if 
		else{
			int pidReturn = wait(NULL);	
		}//else  
	}//else if redirect command 

	else if(REDIRECT_COMMAND1){           //输出重定向
		pid_t pid = fork();	            //创建子进程
		if(pid == -1){
			printf("fork failed !\n");		
		}//if
		else if(pid == 0){
			int redirect_flag = 0;
			FILE* fstream;
			fstream = fopen(argv_redirect1,"r+");        //要读的文件名传递变量并打开，进行读操作
			freopen(argv_redirect1,"r",stdin);
			if(execvp(argvtmp1[0],argvtmp1) < 0){
				redirect_flag = 1;//execvp this redirect command failed		
			}//if
			fclose(stdin);       //关闭，结束标准输出
			fclose(fstream);
			if(redirect_flag){
				printf("%s:command not found\n",argvtmp1[0]);
			}//redirect flag	
				
		}//else if 
		else{
			int pidReturn = wait(NULL);	
		}//else  
	}//else if redirect command 

	else{
		pid_t pid = fork();	
		if(pid == -1){
			printf("fork failed !\n");		
		}//if
		else if(pid == 0){
			if(execvp(argvtmp1[0],argvtmp1) < 0){
				printf("%s:command not found\n",argvtmp1[0]);			
			}//if	
		}//else if 
		else{
			int pidReturn = wait(NULL);	
		}//else  
	}//else normal command

        free(argvtmp1);
	free(argvtmp2);
}

void help(){
	struct passwd* pwp;
	pwp = getpwuid(getuid());
	printf("Hi, %s !\n",pwp->pw_name);
	printf("Here are the built-in commands:\n\n");
	printf("1. cd\n");
	printf("2. history\n");
	printf("3. help\n");
	printf("4. exit\n");
}

void initial(){
	int i = 0;
	while(strcmp(argv[i],"")!=0)strcpy(argv[i++],"\0");
	/*for(i = 0;i < argc;i++){
		strcpy(argv[i],"\0");	
	}*/
	argc = 0;
	BUILTIN_COMMAND = 0;
	PIPE_COMMAND = 0;
	REDIRECT_COMMAND = 0;
	REDIRECT_COMMAND1 = 0;
	BACKGROUND_COMMAND = 0;
}

void init_lastdir(){
	getcwd(lastdir, sizeof(lastdir));
}

void history_setup(){                                          //开始建立history
	using_history();                                           //使用history
	stifle_history(50);                                         //限定字符串长度
	read_history("/tmp/msh_history");	                            //读取history
}
void history_finish(){                                          //停止记录history
	append_history(history_length, "/tmp/msh_history");    //列表末尾添加history的长度
	history_truncate_file("/tmp/msh_history", history_max_entries);     //截断history文件
}
void display_history_list(){                                     //展示history列表
	HIST_ENTRY** h = history_list();                         //h用于存储历史命令
	if(h) {                         //用history_list()逐条获取历史命令赋给参数h
		int i = 0;
		int max = 0;
		int j = 0;
		while(h[max]){
			max++;
		}
		if((strlen(argv[1])) == 0 ){	                                //if history
		    while(h[i]) {
			    printf("%d: %s\n", i, h[i]->line);
			    i++;
		    }
		 }
		 else {                                                 //if history n
		    if((strlen(argv[1])) == 1 ){
			         j=argv[1][0]-48;
           }
           else if((strlen(argv[1])) == 2 ){
                   j = 10*(argv[1][0]-48)+argv[1][1]-48;
           }
           for(i=max-j;i<max;i++){
				     printf("%d: %s\n", i, h[i]->line);
		     }
		 }
	}
}

void cd_operation(struct passwd* pwp){                           //cd
        char cd_path[100];               //用于暂时存储cd及cd ~应该进入到的目录
		
	if((strlen(argv[1])) == 0 ){                                 //if cd
		pwp = getpwuid(getuid());
		sprintf(cd_path,"/home/%s",pwp->pw_name);
		strcpy(argv[1],cd_path);
		argc++;			
	}
	else if((strcmp(argv[1],"~") == 0) ){                        //if cd ~
		pwp = getpwuid(getuid());
		sprintf(cd_path,"/home/%s",pwp->pw_name);
		strcpy(argv[1],cd_path);			
	}
	else if((strcmp(argv[1],"-") == 0) ){                       //if cd -
                if(cdflag%2 == 0){          //判断哪个oldpwd是上次工作的目录
                        strcpy(argv[1],oldpwd0);
                }
                else{
                        strcpy(argv[1],oldpwd1);
                }
	}

	if((chdir(argv[1]))< 0){                                    //路径不存在
		printf("cd: %s: No such file or directory\n",argv[1]);
	}
	else{//获取当前路径
if(cdflag%2 == 0){      //判断应当将当前目录存至哪个oldpwd中                                      getcwd(oldpwd0,sizeof(oldpwd0));        
                }
                else{
                        getcwd(oldpwd1,sizeof(oldpwd1));
                }
                cdflag++;
                //printf("%s\n",oldpwd0);
                //printf("%s\n",oldpwd1);
         }
}

void echo_operation(){                                         //echo
	char ev[100];                           //用于存储环境变量的名字
	int i=0;
	int flag=0;                               //用于确定'及"前是否存在转义符号
	for(i=0;argv[1][i];i++){
	        if(argv[1][i]=='$'){                               //if $
	                int j=0;
		        while(argv[1][i+1]){                     //将$后的字符串存至ev中
			        ev[j]=argv[1][i+1];
			        j++; i++;
                }
                        printf("%s",getenv(ev));	
	        }
	        else if(argv[1][i]=='\''||argv[1][i]=='\"'){       //if ' "
                        if(flag==2){                        //如果存在转义符号
                                printf("%c",argv[1][i]);
                                flag=0;
                        }
                        else{                            //如果不存在转义符号
                                flag++;
                        }   
           }
	        else{                                              //else
	                printf("%c",argv[1][i]);
	        }
	}
	printf("\n");
}