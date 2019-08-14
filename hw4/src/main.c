#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <limits.h>
#include <sys/time.h>
#include "debug.h"
#include "imprimer.h"

#define MAXARGS 128
typedef enum {
    unknown =0,
    help,
    quit,
    type,
    printer,
    conversion,
    printers,
    jobs,
    print,
    cancel,
    paused,
    resume,
    disable,
    enable
} commands;


struct enumtypes
{
   commands color;
   char* str;
};

struct Node
{
  int data;
  struct Node *next;
};
typedef struct pids_main{
    pid_t pid;
    struct pids_main *next;
}main_pids;
struct Commands
{
  char** cmd;
  struct Commands *next;
};
typedef struct conversion_list{
    int from;
    int to;
    char** prog;
    //struct conversion_list* next;
}conversions;

typedef struct job_list{
    JOB *job;
    struct job_list* next;
}jobs_list;
typedef struct temp_list{
    conversions* conversion;
    struct temp_list* next;
}temp_list;

conversions conv_list[64][64];
conversions *list;
temp_list* list_temp;
//conversions *temp;
main_pids *pids = NULL;
jobs_list *job_list = NULL;
//Registers the type so that the program supports it
void add_type(char** cmd, int count);
//prints the help
void print_help();
//Frees up the memory and returns
void quit_command();
void print_unknown();
//Store the printer
void store_printer(char** cmd);
//Splits the line and returns the split line in an an array buffer
char** split_line(char* line);
//Executes command specified
void execute_cmd(char** cmd);
//Shows the printers
void show_printers();
void show_jobs();
void type_conversion(char** cmd);
int search_type_index(char* type);
//int length(char** types);
void get_args_exec(char **cmd,char** args_exec);
void print_command(char** cmd);
int search_printer_index(char* name);
PRINTER* get_printer(char* printer_name);
struct Node* find_path(int **conv_mat,int src_type_index,int dest_type_index);
int BFS(int **conv_mat,int src_type_index,int dest_type_index,int v,int pred[],int dist[]);
void conversion_pipeline(struct Commands* commands,PRINTER* printer, JOB* job);
//void conversion_pipeline(struct Node** path,PRINTER* printer,char* file_name);
conversions* add_conversion(conversions* conversionList, int from, int to,char** prog);
void delete_conversion(conversions* oldConversion);
conversions* create_conversion(int from, int to, char** conv_prog);
int length(int arr[],int size);
void append_node(struct Node** head_ref, int new_data);
int pop_node(struct Node **head_ref);
void push_node(struct Node** head_ref, int new_data);
struct Commands* getCommands(struct Node** path);
void append_command(struct Commands** head_ref, char** new_data);
char** pop_command(struct Commands **head_ref);
void push_command(struct Commands** head_ref, char** new_data);
int countPipes(struct Node** path);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
void unix_error(char *msg);
void child_handler(int sig);
void SigTerm_handler(int sig);
void SigStop_handler(int sig);
void SigCont_handler(int sig);
void SigAbrt_Handler(int sig);
void Sigemptyset(sigset_t *set);
void Sigfillset(sigset_t *set);
void Sigaddset(sigset_t *set, int signum);
void Sigdelset(sigset_t *set, int signum);
int Sigismember(const sigset_t *set, int signum);
int Sigsuspend(const sigset_t *set);
void append_job(JOB* job);
void delete_job(JOB *job);
jobs_list* create_job(JOB* job);
jobs_list* add_job(jobs_list* jobList,JOB* job);
void show_jobs();
void append_pid(main_pids** head_ref, pid_t new_pid);
pid_t pop_pid(main_pids **head_ref);
void push_pid(main_pids** head_ref, pid_t new_pid);
void pipeline(struct Commands* commands, char* file_name, PRINTER* printer_used);
void cancel_command(char** cmd);
void pause_command(char** cmd);
JOB* get_job(int jobid);
pid_t main_process;
void resume_command(char** cmd);
void disable_command(char ** cmd);
void enable_command(char ** cmd);
void delete_pid(pid_t new_pid);
int joblist_length();
JOB* get_job_pid(int pid);
void search_queued_jobs();
void check_and_print(JOB* job);
void free_job_structure(JOB* job);
void free_BFS_queue(struct Node* queue);
void free_converion_list();
void add_temp_list(conversions* conversion);
void del_temp_list();
void free_path(struct Node* path);
void free_commands(struct Commands* commands);
void check_completed_jobs();
//static void pipeline(struct Commands* commands);
sigset_t mask_child, mask_all,prev_one;

//void set_table_and_convert(char** cmd, int mat[][]);

// function to convert string to enum type
commands convert_string_to_enum(const char* str);

struct enumtypes cmds[] = {
  {help,"help"},
  {quit,"quit"},
  {type,"type"},
  {printer,"printer"},
  {conversion,"conversion"},
  {printers,"printers"},
  {jobs,"jobs"},
  {print,"print"},
  {cancel,"cancel"},
  {paused,"pause"},
  {resume,"resume"},
  {disable,"disable"},
  {enable,"enable"}
};

PRINTER printer_list[MAX_PRINTERS];
//JOB *job_list;
int job_count = 0;
int imp_num_printers=0;
size_t maxl = 512;
FILE* out;
int **conv_mat;
//array to store types
char **types;
int type_count=0;
int conv=0;

/*
 * "Imprimer" printer spooler.
 */
char**cmd;
char* user_command;
char *tmp;
char* in_file = NULL;
char* out_file = NULL;
FILE* in;
int main(int argc, char *argv[])
{
    main_process = getpid();
    char* prompt="imp>";
    if(argc==1){
        out = stdout;
        do{
            user_command =readline(prompt);
            add_history(user_command);

            if(user_command == NULL){
                quit_command();
            }
            if(strcmp(user_command,"")==0){
                continue;
            }
            //split the command
            cmd = split_line(user_command);
            if(cmd!=NULL && strcmp(*cmd,"")!=0){
                execute_cmd(cmd);
            }
            free(cmd);
            free(user_command);

        }while(1);
    }
    else{
        char optval;

        int output=0;
        while(optind < argc) {
            if((optval = getopt(argc, argv, "i:o:")) != -1) {
                switch(optval) {
                case 'i':
                    in_file = optarg;
                    break;
                case 'o':
                    output++;
                    out_file = optarg;
                    break;
                case '?':
                    fprintf(out, "Usage: %s [-i <cmd_file>] [-o <out_file>]\n", argv[0]);
                    exit(EXIT_FAILURE);
                    break;
                default:
                    break;
                }
            }
        }
        if(output){
            out = fopen(out_file,"w");
            if(out==NULL){
                char buf[512];
                imp_format_error_message("Cannot open file",buf,512);
                fprintf(out,"%s\n",buf);
                close(0);
                close(1);
                close(2);
                exit(EXIT_FAILURE);
            }
        }
        else{
            out = stdout;
        }
        //open in_file
        in = fopen(in_file,"r");
        if(in==NULL){
            char buf[512];
            imp_format_error_message("Cannot open file",buf,512);
            fprintf(out,"%s\n",buf);
            close(0);
            close(1);
            close(2);
            if(output)
                fclose(out);
            exit(EXIT_FAILURE);
        }

        char *line = malloc(maxl * sizeof(char));
        //char line[1024];
        if(!line){
            char buf[512];
            imp_format_error_message("Memory not allocated",buf,512);
            fprintf(out,"%s\n",buf);
            exit(EXIT_FAILURE);
        }
        //read the file line by line untill EOF
        while (fgets(line, maxl, in)) {
            while(line[strlen(line) - 1] != '\n' && line[strlen(line) - 1] != '\r'){
                tmp = realloc (line, 2 * maxl * sizeof(char));
                maxl = 2*maxl;
                fseek(in,0,SEEK_CUR);
                if (tmp) {
                    line = tmp;
                    maxl *= 2;
                    fgets(line, maxl, in);
                    break;
                }
                else{
                    char buf[512];
                    imp_format_error_message("Memory not allocated",buf,512);
                    fprintf(out,"%s\n",buf);
                    exit(EXIT_FAILURE);
                }
            }
            //parse the line
            cmd = split_line(line);
            if(cmd!=NULL && strcmp(*cmd,"")!=0){
                execute_cmd(cmd);
            }
            free(cmd);
        }
        do{
            user_command =readline(prompt);
            add_history(user_command);
            if(user_command == NULL){
                quit_command();
            }
            if(strcmp(user_command,"")==0){
                continue;
            }
            //split the command
            cmd = split_line(user_command);
            if(cmd!=NULL && strcmp(*cmd,"")!=0)
                execute_cmd(cmd);
            free(cmd);
            free(user_command);
            //execute the command

        }while(1);

    }
    close(0);
    close(1);
    close(2);
    if(in_file)
        fclose(in);
    if(out_file)
        fclose(out);
    exit(EXIT_SUCCESS);
}


char** split_line(char* line){
    char** args=NULL;
    int spaces=0;
    for (char *p = strtok(line," "); p != NULL; p = strtok(NULL, " ")){
        p[strcspn(p, "\n")] = 0;
        p[strcspn(p,"\r")] = 0;
        args = realloc (args, sizeof (char*) * ++spaces);
        if (args == NULL){
            char buf[512];
            imp_format_error_message("memory allocation failed",buf,512);
            fprintf(out,"%s\n",buf);
            return NULL;
        }
        args[spaces-1] = p;
    }
    //realloc one extra element for the last NULL
    args = realloc (args, sizeof (char*) * (spaces+1));
    if (args == NULL){
        char buf[512];
        imp_format_error_message("memory allocation failed",buf,512);
        fprintf(out,"%s\n",buf);
        return NULL;
    }
    args[spaces] = 0;
    return args;

}

void execute_cmd(char** cmd){
    commands command = convert_string_to_enum(cmd[0]);
    switch(command){
        case help:
            print_help();
            search_queued_jobs();
            check_completed_jobs();
            break;
        case quit:
            quit_command();
            break;
        case type:
            type_count++;
            add_type(cmd,type_count);
            search_queued_jobs();
            check_completed_jobs();
            break;
        case printer:
            imp_num_printers++;
            store_printer(cmd);//fillup the printer structure add it to the list of printers
            search_queued_jobs();
            check_completed_jobs();
            break;
        case conversion:
            conv++;
            type_conversion(cmd);
            search_queued_jobs();
            check_completed_jobs();
            break;
        case printers:
            show_printers();
            search_queued_jobs();
            check_completed_jobs();
            break;
        case jobs:
            show_jobs();
            search_queued_jobs();
            check_completed_jobs();
            break;
        case print:
            job_count++;
            print_command(cmd);
            search_queued_jobs();
            check_completed_jobs();
            break;
        case cancel:
            cancel_command(cmd);
            check_completed_jobs();
            break;
        case paused:
            pause_command(cmd);
            check_completed_jobs();
            break;
        case resume:
            resume_command(cmd);
            check_completed_jobs();
            break;
        case disable:
            disable_command(cmd);
            search_queued_jobs();
            check_completed_jobs();
            break;
        case enable:
            enable_command(cmd);
            search_queued_jobs();
            check_completed_jobs();
            break;
        case unknown:
            print_unknown();
            search_queued_jobs();
            check_completed_jobs();
            break;

    }

}

commands convert_string_to_enum(const char* str){
   const int sz = sizeof(cmds) / sizeof(cmds[0]);

   for(int i = 0; i < sz; i++){
        if(strcmp(cmds[i].str, str) == 0)
            return cmds[i].color;
   }
   return unknown;
}

void print_help(){
    fprintf(out,"Miscellaneous commands\n  -help\n  -quit\nConfiguration commands\n  -type file_type\n  -printer printer_name file_type\n  -conversion file_type1 file_type2 conversion_program [arg1 arg2...]\nInformational commands\n  -printers\n  -jobs\nSpooling commands\n  -print file_name [printer1 printer2...]\n  -cancel job_number\n  -pause job_number\n  -resume job_number\n  -disable printer_name\n  -enable printer_name\n");
}


void quit_command(){

    if(conv_list!=NULL){
        free_converion_list();
    }
    if(out_file)
        if(fclose(out)==EOF){
            char buf[512];
            imp_format_error_message("Closing a file failed",buf,512);
            fprintf(out,"%s\n",buf);
        }
    if(in_file){
        if(fclose(in)==EOF){
            char buf[512];
            imp_format_error_message("Closing a file failed",buf,512);
            fprintf(out,"%s\n",buf);
        }
    }
    close(2);
    close(1);
    close(0);
    exit(EXIT_SUCCESS);
}

void print_unknown(){
    fprintf(out,"Invalid command\n");
}

void store_printer(char** cmd){
    if(cmd[1]){
        if(cmd[2]){
            if(!cmd[3]){
                char* name =malloc(sizeof(cmd[1]));
                if(name == NULL){
                    char buf[512];
                    imp_format_error_message("Memory Allocation Failed",buf,512);
                    fprintf(out,"%s\n",buf);
                    return;
                }
                strcpy(name,cmd[1]);
                char* type=malloc(sizeof(cmd[2]));
                if(type == NULL){
                    char buf[512];
                    imp_format_error_message("Memory Allocation Failed",buf,512);
                    fprintf(out,"%s\n",buf);
                    return;
                }
                strcpy(type,cmd[2]);
                //Check if type is present in type list
                if(search_type_index(type)!=-1){
                    printer_list[imp_num_printers-1].id = imp_num_printers-1;
                    printer_list[imp_num_printers-1].name = name;
                    printer_list[imp_num_printers-1].type = type;
                    printer_list[imp_num_printers-1].enabled = 0;
                    printer_list[imp_num_printers-1].busy = 0;
                    char buf[512];
                    imp_format_printer_status(&printer_list[imp_num_printers-1], buf, 512);
                    fprintf(out, "%s\n", buf);
                }
                else{
                    imp_num_printers--;
                    free(name);
                    free(type);
                    char buf[512];
                    imp_format_error_message("file type not supported!",buf,512);
                    fprintf(out, "%s\n", buf);
                    return;
                }
            }
            else{
                char buf[512];
                imp_format_error_message("Invalid command",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
        }
        else{
            char buf[512];
            imp_format_error_message("Missing Argument",buf,512);
            fprintf(out,"%s\n",buf);
            return;
        }
    }
    else{
        char buf[512];
        imp_format_error_message("Missing Argument",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }

}

void add_type(char **cmd, int count){
    if(count==1){
        types = (char**)malloc(5*sizeof(char*));
        if (types == NULL){
            char buf[512];
            imp_format_error_message("memory allocation failed",buf,512);
            fprintf(out,"%s\n",buf);
            return;
        }
        if(cmd[1]){
            types[0]=malloc(sizeof(cmd[1]));
            if (types[0] == NULL){
                char buf[512];
                imp_format_error_message("memory allocation failed",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
            strcpy(types[0],cmd[1]);
            if(cmd[2]){
                char buf[512];
                imp_format_error_message("Invalid Command",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
        }
        else{
            char buf[512];
            imp_format_error_message("Invalid Command",buf,512);
            fprintf(out,"%s\n",buf);
            return;
        }
    }
    else{
        if(type_count<=5){
            if(cmd[1]){
                types[count-1]=malloc(sizeof(cmd[1]));
                if (types[type_count-1] == NULL){
                    char buf[512];
                    imp_format_error_message("memory allocation failed",buf,512);
                    fprintf(out,"%s\n",buf);
                    return;
                }
                strcpy(types[type_count-1],cmd[1]);
                if(cmd[2]){
                    char buf[512];
                    imp_format_error_message("Invalid Command",buf,512);
                    fprintf(out,"%s\n",buf);
                    return;
                }
            }
            else{
                char buf[512];
                imp_format_error_message("Invalid Command",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
        }
        else{
            types=realloc(types,count*sizeof(char*));
            if (types == NULL){
                char buf[512];
                imp_format_error_message("memory allocation failed",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
            if(cmd[1]){
                types[type_count-1]=malloc(sizeof(cmd[1]));
                if (types[type_count-1] == NULL){
                    char buf[512];
                    imp_format_error_message("memory allocation failed",buf,512);
                    fprintf(out,"%s\n",buf);
                    return;
                }
                strcpy(types[type_count-1],cmd[1]);;
                if(cmd[2]){
                    char buf[512];
                    imp_format_error_message("Invalid Command",buf,512);
                    fprintf(out,"%s\n",buf);
                    return;
                }
            }
            else{
                char buf[512];
                imp_format_error_message("Invalid Command",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
        }
    }
}

void show_printers(){
    char buf[512];
    for(int i=0;i<imp_num_printers;i++){
        imp_format_printer_status(&printer_list[i], buf, 512);
        fprintf(out,"%s\n",buf);
    }
}
void show_jobs(){
    char buf[512];
    jobs_list* temp = job_list;
    while(temp){
        imp_format_job_status(temp->job,buf,512);
        fprintf(out,"%s\n",buf);
        temp = temp->next;
    }
}

void type_conversion(char** cmd){
    if(conv==1){
        if((conv_mat = (int**)malloc(sizeof(int*) * type_count))==NULL){
            char buf[512];
            imp_format_error_message("memory allocation failed",buf,512);
            fprintf(out,"%s\n",buf);
            return;
        }
        for(int i=0; i<type_count; i++){
            conv_mat[i]= (int*)malloc(sizeof(int) * type_count);
            if(conv_mat[i] == NULL){
                char buf[512];
                imp_format_error_message("memory allocation failed",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
        }
        for (int i=0; i < type_count; i++){
           for (int j=0; j < type_count; j++){
              conv_mat[i][j] = -1;
           }
        }
        if(cmd[1]){
            int i;
            if((i=search_type_index(cmd[1]))==-1){
                char buf[512];
                imp_format_error_message("Unknown Type",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
            if(cmd[2]){
                int j;
                if((j=search_type_index(cmd[2]))==-1){
                    char buf[512];
                    imp_format_error_message("Unknown Type",buf,512);
                    fprintf(out,"%s\n",buf);
                    return;
                }
                if(cmd[3]){
                    conversions *new_conv;
                    new_conv=add_conversion(list,i,j,cmd+3);
                    add_temp_list(new_conv);
                    conv_list[i][j] = *new_conv;
                    conv_mat[i][j] = j;
                }
                else{
                    char buf[512];
                    imp_format_error_message("Invalid Command",buf,512);
                    fprintf(out,"%s\n",buf);
                    return;
                }
            }
            else{
                char buf[512];
                imp_format_error_message("Invalid Command",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
        }

        else{
            char buf[512];
            imp_format_error_message("Invalid Command",buf,512);
            fprintf(out,"%s\n",buf);
            return;
        }

    }
    else{
        if(cmd[1]){
            int i;
            if((i=search_type_index(cmd[1]))==-1){
                char buf[512];
                imp_format_error_message("Unknown Type",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
            if(cmd[2]){
                int j;
                if((j=search_type_index(cmd[2]))==-1){
                    char buf[512];
                    imp_format_error_message("Unknown Type",buf,512);
                    fprintf(out,"%s\n",buf);
                    return;
                }
                if(cmd[3]){
                    conversions *new_conv;
                    new_conv=add_conversion(list,i,j,cmd+3);
                    add_temp_list(new_conv);
                    conv_list[i][j] = *new_conv;
                    conv_mat[i][j]= j;
                        // }
                }
                else{
                    char buf[512];
                    imp_format_error_message("Invalid Command",buf,512);
                    fprintf(out,"%s\n",buf);
                    return;
                }
            }
            else{
                char buf[512];
                imp_format_error_message("Invalid Command",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
        }
        else{
            char buf[512];
            imp_format_error_message("Invalid Command",buf,512);
            fprintf(out,"%s\n",buf);
            return;
        }

    }
}

int search_type_index(char* type){
    for(int i=0;i<type_count;i++){
        if(strcmp(type,types[i])==0)
            return i;
    }
    return -1;
}

int search_printer_index(char* name){
    for(int i=0;i<imp_num_printers;i++){
        if(strcmp(name,printer_list[i].name)==0)
            return i;
    }
    return -1;
}

PRINTER* get_printer(char* printer_name){
    int i=0;
    for(;i<imp_num_printers;i++){
        if(strcmp(printer_name,printer_list[i].name)==0)
            return &printer_list[i];
    }
    return NULL;
}
void print_command(char** cmd){
    JOB *job=malloc(sizeof(JOB));
    job->jobid = joblist_length();
    job->status = QUEUED;
    job->pgid = 0;
    gettimeofday(&job->change_time,NULL);
    gettimeofday(&job->creation_time,NULL);
    append_job(job);
    //get type
    char* job_type;
    if(cmd[1]){
        job_type = strchr(cmd[1],'.');
        job->file_name = strdup(cmd[1]);
        job->file_type = strdup(job_type+1);
    }
    else{
        //print error
        char buf[512];
        imp_format_error_message("Invalid command", buf, 512);
        fprintf(out, "%s\n",buf);
        return;
    }
    //check if previously declared
    job_type=job_type+1;
    if(search_type_index(job_type)==-1){
        char buf[512];
        imp_format_error_message("Type not supported", buf, 512);
        fprintf(out, "%s\n",buf);
        return;
    }
    //if optional printers are specified check if these are declared using printer command
    PRINTER_SET eligible_printers=0x00000000;
    int i=2;
    if(*(cmd+i)){//printer name specified
        // define the set of eligible printers for this job
        while(*(cmd+i)){
            if(search_printer_index(*(cmd+i))!=-1){
                PRINTER *p = get_printer(*(cmd+i));
                eligible_printers = eligible_printers | (0x1 << p->id);
                job->eligible_printers = eligible_printers;
            }
            i++;
        }
        if(eligible_printers ==0){
            char buf[512];
            imp_format_error_message("printer name provided is not registered", buf, 512);
            fprintf(out, "%s\n",buf);
            return;
        }
    }
    else{
        //printer name not specified
        eligible_printers = 0xffffffff;
        job->eligible_printers = eligible_printers;
    }
    char buf[512]="";
    imp_format_job_status(job, buf, 512);
    fprintf(out, "%s\n",buf);
    //Check if the eligible printer can convert file in the job to that can be printed by the printer
    check_and_print(job);

}

struct Node* find_path(int **conv_mat,int src_type_index,int dest_type_index){
    int v =type_count;
    int pred[v], dist[v];
    struct Node* path = NULL;
    if(BFS(conv_mat,src_type_index,dest_type_index,v,pred,dist)==0)
        return NULL;
    else{

        int idx = dest_type_index;
        push_node(&path,idx);
        while (pred[idx] != -1) {
            push_node(&path,pred[idx]);
            idx = pred[idx];
        }
    }
    return path;
}

int BFS(int **conv_mat,int src_type_index,int dest_type_index,int v,int pred[],int dist[]){
    if(conv_mat==NULL)
        return 0;
    struct Node* queue = NULL;
    int visited[v];
    for (int i = 0; i < v; i++) {
        visited[i] = 0;
        dist[i] = INT_MAX;
        pred[i] = -1;
    }
    visited[src_type_index] = 1;
    dist[src_type_index] = 0;
    append_node(&queue, src_type_index);
    int i=0;
    while(queue!=NULL){

        int u = pop_node(&queue);
        for (int j = 0; j < type_count; j++) {
            if (conv_mat[u][j]!=-1 && visited[conv_mat[u][j]] == 0) {
                visited[conv_mat[u][j]] = 1;
                dist[conv_mat[u][j]] = dist[u] + 1;
                pred[conv_mat[u][j]] = u;
                append_node(&queue,conv_mat[u][j]);
                i++;
                // We stop BFS when we find
                // destination.
                if(conv_mat[u][j] == dest_type_index){
                    free_BFS_queue(queue);
                    return 1;
                }
            }
        }
    }
    free_BFS_queue(queue);
    return 0;
}

void conversion_pipeline(struct Commands* commands, PRINTER* printer_used, JOB* job){
    pid_t pid_master;
    sigset_t mask_all, mask_child, prev_one;
    Sigfillset(&mask_all);
    Sigemptyset(&mask_child);
    Sigaddset(&mask_child, SIGCHLD);
    Signal(SIGCHLD,child_handler);
    Sigprocmask(SIG_BLOCK, &mask_child, &prev_one);
    pid_master = fork();

    if(pid_master==0){

        //Sigprocmask(SIG_BLOCK, &mask_all, &prev_one);
        if(setpgid(0,0)< 0){
            char buf[512];
            imp_format_error_message("Setpgid Failed", buf, 512);
            fprintf(out, "%s\n",buf);
        }
        //Sigprocmask(SIG_SETMASK, &prev_one, NULL);
        Sigprocmask(SIG_SETMASK, &prev_one, NULL);
        pipeline( commands, job->file_name, printer_used);
    }
    else if(pid_master < 0){
        char buf[512];
        imp_format_error_message("Invalid Command",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }

    //Sigprocmask(SIG_BLOCK, &mask_all, NULL); /* Parent process */
    setpgid(pid_master,0);
    job->pgid = pid_master;
    append_pid(&pids,pid_master);
    Sigprocmask(SIG_SETMASK, &prev_one, NULL);
}

//set_table_and_convert();

conversions* create_conversion(int from, int to, char** conv_prog) {
    conversions* newConversion =(conversions*) malloc(sizeof(conversions));
    if (NULL != newConversion){
        newConversion->from=from;
        newConversion->to = to;
        int len=0;
        while(*(conv_prog+len)){
            len++;
        }
        newConversion->prog = (char**)malloc((len + 1) * sizeof(char*));
        int i=0;
        while(*(conv_prog+i)){
            newConversion->prog[i] = strdup(conv_prog[i]);
            i++;
        }
        newConversion->prog[i] = NULL;
    }
    else{
        char buf[512];
        imp_format_error_message("malloc failed",buf,512);
        fprintf(out,"%s\n",buf);
        return NULL;
    }
    return newConversion;
}

conversions* add_conversion(conversions* conversionList, int from, int to,char** prog) {
        conversions* newConversion = create_conversion(from, to, prog);
        return newConversion;
}

jobs_list* create_job(JOB* job) {
        jobs_list* newJob = malloc(sizeof(jobs_list));
        if (NULL != newJob){
                newJob->job = job;
                newJob->next = NULL;
        }
        else{
            char buf[512];
            imp_format_error_message("malloc failed",buf,512);
            fprintf(out,"%s\n",buf);
            return NULL;
        }
        return newJob;
}

jobs_list* add_job(jobs_list* jobList,JOB* job) {
        jobs_list* newConversion = create_job(job);
        if (NULL != newConversion) {
                newConversion->next = jobList;
        }
        return newConversion;
}
int length(int arr[],int size){
    int i=0;
    while(arr[i]!=-1)
        i++;
    return i;
}

void append_node(struct Node** head_ref, int new_data)//append to the last
{
    struct Node* new_node = (struct Node*) malloc(sizeof(struct Node));
    if(new_node == NULL){
        char buf[512];
        imp_format_error_message("malloc failed",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    struct Node *last = *head_ref;
    new_node->data  = new_data;
    new_node->next = NULL;
    if (*head_ref == NULL)
    {
       *head_ref = new_node;
       return;
    }
    while (last->next != NULL)
        last = last->next;
    last->next = new_node;
    return;
}


int pop_node(struct Node **head_ref)//delete first node
{
    // Store head node
    struct Node* temp = *head_ref, *prev;
    prev=temp;
    temp=temp->next;
    *head_ref = temp;
    int ans = prev->data;
    free(prev);
    return ans;
}

void push_node(struct Node** head_ref, int new_data)//push to front of the list
{
    struct Node* new_node = (struct Node*) malloc(sizeof(struct Node));
    if(new_node == NULL){
        char buf[512];
        imp_format_error_message("malloc failed",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    new_node->data  = new_data;
    new_node->next = (*head_ref);
    (*head_ref)    = new_node;
}

void append_pid(main_pids** head_ref, pid_t new_pid)//append to the last
{
    main_pids* new_node = (main_pids*) malloc(sizeof(main_pids));
    if(new_node == NULL){
        char buf[512];
        imp_format_error_message("malloc failed",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    main_pids *last = *head_ref;
    new_node->pid  = new_pid;
    new_node->next = NULL;
    if (*head_ref == NULL)
    {
       *head_ref = new_node;
       return;
    }
    while (last->next != NULL)
        last = last->next;
    last->next = new_node;
    return;
}


pid_t pop_pid(main_pids **head_ref)//delete first node
{
    // Store head node
    main_pids* temp = *head_ref, *prev;
    prev=temp;
    temp=temp->next;
    *head_ref = temp;
    pid_t ans = prev->pid;
    free(prev);
    return ans;
}

void push_pid(main_pids** head_ref, pid_t new_pid)//push to front of the list
{
    main_pids* new_node = (main_pids*) malloc(sizeof(main_pids));
    new_node->pid  = new_pid;
    new_node->next = (*head_ref);
    (*head_ref)    = new_node;
}

void delete_pid(pid_t new_pid)//push to front of the list
{   debug("start");
    main_pids* temp = pids, *prev;
    prev=temp;
    while(temp->next){
        if(temp->pid ==new_pid){
            prev->next=temp->next;
            main_pids* node=temp;
            temp=temp->next;
            free(node);
            break;
        }
        temp=temp->next;
    }
    debug("delte ");
}
void append_job(JOB* job)//append to the last
{
    jobs_list* new_job = (jobs_list*) malloc(sizeof(jobs_list));
    if(new_job == NULL){
        char buf[512];
        imp_format_error_message("malloc failed",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    jobs_list *last;
    last = job_list;
    new_job->job  = job;
    new_job->next = NULL;
    if (job_list == NULL)
    {
       job_list = new_job;
       return;
    }
    while (last->next != NULL)
        last = last->next;
    last->next = new_job;
    return;
}


void delete_job(JOB *job)//delete the specified job
{
    jobs_list* temp = job_list, *prev;
    prev=job_list;
    if (temp != NULL && temp->job == job){
        job_list = temp->next;   // Changed head
        free(temp->job->file_name);
        free(temp->job->file_type);
        free(temp->job);
        free(temp);               // free old head
        return;
    }
    while(temp && temp->job!=job){
        prev=temp;
        temp=temp->next;
    }
    if (temp == NULL){
        char buf[512];
        imp_format_error_message("No such job in list",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    prev->next = temp->next;
    free(temp->job->file_name);
    free(temp->job->file_type);
    free(temp->job);
    return;
}
JOB* get_job(int jobid)//delete the specified job
{
    // Store head node
    jobs_list* temp = job_list;
    while(temp){
        if(temp->job->jobid ==jobid){
            return temp->job;
        }
        temp=temp->next;
    }
    return NULL;
}
JOB* get_job_pid(int pid){
    jobs_list* temp = job_list;
    while(temp){
        if(temp->job->pgid ==pid){
            return temp->job;
        }
        temp=temp->next;
    }
    return NULL;
}
int countPipes(struct Node** path){
    int count=0;
    struct Node* temp = *path;
    while(temp){
        count++;
        temp=temp->next;
    }
    return count-1;
}

void append_command(struct Commands** head_ref, char** new_data)//append to the last
{
    struct Commands* new_node = (struct Commands*) malloc(sizeof(struct Commands));
    if(new_node == NULL){
        char buf[512];
        imp_format_error_message("malloc failed",buf,512);
        fprintf(out,"%s\n",buf);
    }
    struct Commands *last = *head_ref;
    new_node->cmd  = new_data;
    new_node->next = NULL;
    if (*head_ref == NULL)
    {
       *head_ref = new_node;
       return;
    }
    while (last->next != NULL)
        last = last->next;
    last->next = new_node;
    return;
}


char** pop_command(struct Commands **head_ref)//delete first node
{
    // Store head node
    struct Commands* temp = *head_ref, *prev;
    prev=temp;
    temp=temp->next;
    *head_ref = temp;
    char** ans = prev->cmd;
    free(prev);
    return ans;
}

void push_command(struct Commands** head_ref, char** new_data)//push to front of the list
{
    struct Commands* new_node = (struct Commands*) malloc(sizeof(struct Node));
    if(new_node == NULL){
        char buf[512];
        imp_format_error_message("malloc failed",buf,512);
        fprintf(out,"%s\n",buf);
    }
    new_node->cmd  = new_data;
    new_node->next = (*head_ref);
    (*head_ref)    = new_node;
}

struct Commands* getCommands(struct Node** path){
    struct Commands* commands = NULL;
    struct Node* temp = *path;
    while(temp && temp->next){
        append_command(&commands ,(conv_list[temp->data][temp->next->data].prog));
        temp=temp->next;
    }
    return commands;

}


handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* Restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0){
        unix_error("Signal error");
    }
    return (old_action.sa_handler);
}
/* $end sigaction */

void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if (sigprocmask(how, set, oldset) < 0)
    unix_error("Sigprocmask error");
    return;
}

void unix_error(char *msg) /* Unix-style error */
{
    fprintf(out, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

void child_handler(int sig){
    sigset_t mask_all, prev_all;
    pid_t pid;
    Sigfillset(&mask_all);
    int status;
    while ((pid = waitpid(-1, &status,WSTOPPED | WUNTRACED | WCONTINUED)) > 0){ /* Reap a zombie child */
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        JOB*job = get_job_pid(pid);
        if(WIFSTOPPED(status)){
            job->status = PAUSED;
            gettimeofday(&job->change_time,NULL);
            char buf[512];
            imp_format_job_status(job,buf,512);
            fprintf(out,"%s\n",buf);
            search_queued_jobs();
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);

            //Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            return;
        }
        else if(WIFEXITED(status)){
            int state = WEXITSTATUS(status);
            if(state!=0){
                //TODO:set job to aborted
                //Sigprocmask(SIG_BLOCK, &mask_all, NULL);
                job->status =ABORTED;
                job->pgid = pid;
                gettimeofday(&job->change_time,NULL);
                char buf[512];
                imp_format_job_status(job,buf,512);
                fprintf(out,"%s\n",buf);
                search_queued_jobs();
                Sigprocmask(SIG_SETMASK, &prev_all, NULL);

                return;
            }
            else if(state==0){
                //TODO:set job to be completed
                //Sigprocmask(SIG_BLOCK, &mask_all, &prev_one)
                job->status =COMPLETED;
                job->pgid = pid;
                job->chosen_printer->busy = 0;
                gettimeofday(&job->change_time,NULL);
                char buf[512];
                imp_format_job_status(job,buf,512);
                fprintf(out,"%s\n",buf);
                Sigprocmask(SIG_SETMASK, &prev_all, NULL);
                search_queued_jobs();
                return;
            }
        }
        else if(WIFSIGNALED(status)){
            job->status =ABORTED;
            job->pgid = pid;
            job->chosen_printer->busy= 0;
            char buf[512];
            imp_format_job_status(job,buf,512);
            fprintf(out,"%s\n",buf);
            char buf2[512];
            imp_format_printer_status(job->chosen_printer,buf2,512);
            fprintf(out,"%s\n",buf2);
            search_queued_jobs();
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
            return;
        }

        else if(WIFCONTINUED(status)){
            //Sigprocmask(SIG_BLOCK, &mask_all, NULL);
            job->status = RUNNING;
            char buf[512];
            imp_format_job_status(job,buf,512);
            fprintf(out,"%s\n",buf);
            search_queued_jobs();
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
            return;
        }
    }
    if (errno != ECHILD){
        char buf[512];
        imp_format_error_message("errno not set to ECHILD",buf,512);
        fprintf(out,"%s\n",buf);
    }
    Sigprocmask(SIG_SETMASK, &prev_all, NULL);
}

void Sigemptyset(sigset_t *set)
{
    if (sigemptyset(set) < 0)
    unix_error("Sigemptyset error");
    return;
}

void Sigfillset(sigset_t *set)
{
    if (sigfillset(set) < 0)
    unix_error("Sigfillset error");
    return;
}

void Sigaddset(sigset_t *set, int signum)
{
    if (sigaddset(set, signum) < 0)
    unix_error("Sigaddset error");
    return;
}

void Sigdelset(sigset_t *set, int signum)
{
    if (sigdelset(set, signum) < 0)
    unix_error("Sigdelset error");
    return;
}

int Sigismember(const sigset_t *set, int signum)
{
    int rc;
    if ((rc = sigismember(set, signum)) < 0)
    unix_error("Sigismember error");
    return rc;
}

int Sigsuspend(const sigset_t *set)
{
    int rc = sigsuspend(set); /* always returns -1 */
    if (errno != EINTR)
        unix_error("Sigsuspend error");
    return rc;
}

void pipeline(struct Commands* commands, char* file_name, PRINTER* printer_used){
   int tmpin = dup(0);
    int tmpout =dup(1);
    Signal(SIGCHLD, SIG_DFL);
    int fdin;
    int file_fd=open(file_name,O_RDWR,0777);
    if(file_fd==-1){
        char buf[512];
        imp_format_error_message("File not valid",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    else{
        fdin = file_fd;
    }
    int ret,fdout;
    while(commands){
        if(dup2(fdin,0)==-1){
            char buf[512];
            imp_format_error_message("dup2 failed",buf,512);
            fprintf(out,"%s\n",buf);
            return;
        }
        if(close(fdin)==-1){
            char buf[512];
            imp_format_error_message("Closing the file failed",buf,512);
            fprintf(out,"%s\n",buf);
            return;
        }
        if(commands->next == NULL){
            int outfile = imp_connect_to_printer(printer_used, PRINTER_NORMAL);
            if(outfile==-1){
                char buf[512];
                imp_format_error_message("Printer not able to connect",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
            else{
                fdout= outfile;
            }
        }
        else{
            int fdpipe[2];
            if(pipe(fdpipe)==-1){
                char buf[512];
                imp_format_error_message("Pipe failed",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
            fdout = fdpipe[1];
            fdin = fdpipe[0];
        }
        if(dup2(fdout,1)==-1){
            char buf[512];
            imp_format_error_message("dup2 failed ",buf,512);
            fprintf(out,"%s\n",buf);
            return;
        }
        if(close(fdout)==-1){
            char buf[512];
            imp_format_error_message("closing the file failed ",buf,512);
            fprintf(out,"%s\n",buf);
            return;
        }
        ret = fork();
        if(ret==0){
            //debug("execvp statud:%d",execvp(*(commands->cmd),commands->cmd));
            if(execvp(*(commands->cmd),commands->cmd)<0){
                char buf[512];
                imp_format_error_message("execvp failed",buf,512);
                fprintf(out,"%s\n",buf);
                exit(1);
            }
        }
        else if(ret < 0){
            char buf[512];
            imp_format_error_message("fork error",buf,512);
            fprintf(out,"%s\n",buf);
            return;
        }
        commands = commands->next;


    }
    if(dup2(tmpin,0)==-1){
    char buf[512];
    imp_format_error_message("dup2 failed ",buf,512);
    fprintf(out,"%s\n",buf);
    return;
    }
    if(dup2(tmpout,1)==-1){
        char buf[512];
        imp_format_error_message("dup2 failed ",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    if(close(tmpin)==-1){
        char buf[512];
        imp_format_error_message("closing failed ",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    if(close(tmpout)==-1){
        char buf[512];
        imp_format_error_message("closing failed ",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    int status;
    if(waitpid(-1,&status,0)<0){
        char buf[512];
        imp_format_error_message("waitpid error",buf,512);
        fprintf(out,"%s\n",buf);
        exit(1);
    }
    else if(WIFEXITED(status)){
        int state = WEXITSTATUS(status);
        exit(state);
    }
    else if(WIFSIGNALED(status)){
        int sig = WTERMSIG(status);
        exit(sig);

    }
    if(errno !=ECHILD){
        char buf[512];
        imp_format_error_message("errno not set to ECHILD",buf,512);
        fprintf(out,"%s\n",buf);
    }
}


void cancel_command(char** cmd){
    sigset_t prev_all;
    Sigfillset(&mask_all);
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

    if(cmd[1]){

        int jobid= atoi(cmd[1]);
        JOB* job =get_job(jobid);
        if(job != NULL){
            if(job->status==RUNNING){
                if(killpg(job->pgid,SIGTERM)==-1){
                    char buf[512];
                    imp_format_error_message("killpg failed ",buf,512);
                    fprintf(out,"%s\n",buf);
                    //return;
                }
            }
            else if(job->status == QUEUED){
                job->status = ABORTED;
                char buf[512];
                imp_format_job_status(job,buf,512);
                fprintf(out,"%s\n",buf);
                search_queued_jobs();
            }
            else{
                char buf[512];
                imp_format_error_message("job not running",buf,512);
                fprintf(out,"%s\n",buf);
                //return;
            }
        }
        else{
            char buf[512];
            imp_format_error_message("job doesn't exist",buf,512);
            fprintf(out,"%s\n",buf);
            //return;
        }
    }
    else{
        char buf[512];
        imp_format_error_message("Invalid Command",buf,512);
        fprintf(out,"%s\n",buf);
        //return;
    }
    if(cmd[2]){
        char buf[512];
        imp_format_error_message("Invalid Command",buf,512);
        fprintf(out,"%s\n",buf);
        //return;
    }
    Sigprocmask(SIG_SETMASK, &prev_one, NULL);
}

void pause_command(char** cmd){
    sigset_t prev_all;
    Sigfillset(&mask_all);
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    if(cmd[1]){
        int jobid= atoi(cmd[1]);
        JOB* job =get_job(jobid);
        if(job != NULL){
            if(job->status==RUNNING){
                if(killpg(job->pgid,SIGSTOP)==-1){
                    char buf[512];
                    imp_format_error_message("killpg failed ",buf,512);
                    fprintf(out,"%s\n",buf);
                    return;
                }
            }
            else{
                char buf[512];
                imp_format_error_message("job not running",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
        }
        else{
            char buf[512];
            imp_format_error_message("job doesn't exist",buf,512);
            fprintf(out,"%s\n",buf);
            return;
        }
    }
    else{
        char buf[512];
        imp_format_error_message("Invalid Command",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    if(cmd[2]){
        char buf[512];
        imp_format_error_message("Invalid Command",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    Sigprocmask(SIG_SETMASK, &prev_all, NULL);
}

void resume_command(char** cmd){
    if(cmd[1]){
        int jobid= atoi(cmd[1]);
        JOB* job =get_job(jobid);
        if(job != NULL){
            if(job->status==PAUSED){
                if(killpg(job->pgid,SIGCONT)==-1){
                    char buf[512];
                    imp_format_error_message("killpg failed ",buf,512);
                    fprintf(out,"%s\n",buf);
                    return;
                }
            }
            else{
                char buf[512];
                imp_format_error_message("job not paused",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
        }
        else{
            char buf[512];
            imp_format_error_message("job doesn't exist",buf,512);
            fprintf(out,"%s\n",buf);
            return;
        }
    }
    else{
        char buf[512];
        imp_format_error_message("Invalid Command",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    if(cmd[2]){
        char buf[512];
        imp_format_error_message("Invalid Command",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
}

void disable_command(char ** cmd){
    if(cmd[1]){
        PRINTER* p =get_printer(cmd[1]);
        if(p){
            if(p->enabled == 0){
                char buf[512];
                imp_format_error_message("Printer already disabled",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
            p->enabled = 0;
            char buf[512];
            imp_format_printer_status(p,buf,512);
            fprintf(out,"%s\n",buf);
            search_queued_jobs();
            //print printer status
        }
    }
    else{
        char buf[512];
        imp_format_error_message("Invalid command",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    if(cmd[2]){
        char buf[512];
        imp_format_error_message("Invalid command",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
}

void enable_command(char ** cmd){
    if(cmd[1]){
        PRINTER* p =get_printer(cmd[1]);
        if(p){
            if(p->enabled ==1){
                char buf[512];
                imp_format_error_message("Printer already enabled",buf,512);
                fprintf(out,"%s\n",buf);
                return;
            }
            p->enabled = 1;
            //Search for the pending job and process it if the printer supports
            char buf[512];
            imp_format_printer_status(p,buf,512);
            fprintf(out,"%s\n",buf);
            search_queued_jobs();
        }
    }
    else{
        char buf[512];
        imp_format_error_message("Invalid command",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
    if(cmd[2]){
        char buf[512];
        imp_format_error_message("Invalid command",buf,512);
        fprintf(out,"%s\n",buf);
        return;
    }
}

int joblist_length(){
    jobs_list* head= job_list;
    int len=0;
    while(head){
        len++;
        head=head->next;
    }
    return len;
}

void search_queued_jobs(){
    jobs_list* head =job_list;
    while(head){
        if(head->job->status == QUEUED){
            check_and_print(head->job);
        }
        head = head->next;
    }
}
void check_completed_jobs(){
    jobs_list* head =job_list;
    while(head){
        //debug("head job status is:%d",head->job->status);
        if(head->job->status == COMPLETED || head->job->status == ABORTED){
            struct timeval cur_time;
            gettimeofday(&cur_time,NULL);
            double diff;
            diff = (cur_time.tv_sec - head->job->change_time.tv_sec) * 1000.0;
            diff += (cur_time.tv_usec - head->job->change_time.tv_usec) / 1000.0;
            if(diff>=60000.00)
                delete_job(head->job);
        }
        head = head->next;
    }
}
void check_and_print(JOB* job){
    for(int i=0;i< imp_num_printers;i++){
        if(job->eligible_printers & (0x1 <<i)){

            int job_type_index = search_type_index(job->file_type);
            int printer_type_index = search_type_index(printer_list[i].type);
            //job->chosen_printer = &printer_list[i];
            if(job_type_index == printer_type_index && printer_list[i].enabled == 1 && printer_list[i].busy == 0){
                job->chosen_printer = &printer_list[i];
                job->status  = RUNNING;
                gettimeofday(&job->change_time,NULL);
                printer_list[i].busy = 1;
                char buf[512];
                imp_format_job_status(job,buf,512);
                fprintf(out,"%s\n",buf);
                char buf2[512];
                imp_format_printer_status(&printer_list[i],buf2,512);
                fprintf(out,"%s\n",buf2);
                struct Commands* commands = NULL;
                char *cmds[] = {"/bin/cat", 0};
                append_command(&commands,cmds);
                //Sigprocmask(SIG_BLOCK, &mask_all, NULL);
                conversion_pipeline(commands,&printer_list[i],job);
                //Sigprocmask(SIG_SETMASK, &prev_one, NULL);
                free_commands(commands);
                break;
            }
            struct Node* path;
            if((path=find_path(conv_mat,job_type_index,printer_type_index))!=NULL && printer_list[i].enabled == 1 && printer_list[i].busy == 0)
            {
                job->chosen_printer = &printer_list[i];
                job->status = RUNNING;
                gettimeofday(&job->change_time,NULL);
                char buf[512];
                imp_format_job_status(job,buf,512);
                fprintf(out,"%s\n",buf);
                printer_list[i].busy = 1;
                char buf2[512];
                imp_format_printer_status(&printer_list[i],buf2,512);
                fprintf(out,"%s\n",buf2);

                struct Commands* commands = getCommands(&path);
                free_path(path);
                conversion_pipeline(commands,&printer_list[i],job);
                free_commands(commands);
                break;
            }
        }
    }
}

void free_job_structure(JOB* job){
    free(job->file_name);
    free(job->file_type);
}

void free_BFS_queue(struct Node* queue){
    struct Node* tmp;

   while (queue != NULL)
    {
       tmp = queue;
       queue = queue->next;
       free(tmp);
    }
}

void free_converion_list(){

    for(int i=0;i<64;i++){
        for(int j=0;j<64;j++){
            //debug("%p",&conv_list[i][j]);
            if(conv_list[i][j].prog){

                int k=0;
                while((conv_list[i][j].prog[k])){
                    free(conv_list[i][j].prog[k]);
                    k++;
                }
                free(conv_list[i][j].prog);

            }

        }
    }
    //free(conv_list);
    if(list_temp!=NULL){
        del_temp_list();
    }
}
void add_temp_list(conversions* conversion){
    temp_list* head =malloc(sizeof(temp_list));
    head->conversion = conversion;
    head->next=list_temp;
    list_temp=head;
}
void del_temp_list(){
    temp_list* tmp;

   while (list_temp != NULL)
    {
       tmp = list_temp;
       list_temp = list_temp->next;
        free(tmp->conversion);
        free(tmp);
    }
}
void free_path(struct Node* path){
    struct Node* tmp;

   while (path != NULL)
    {
       tmp = path;
       path = path->next;
       free(tmp);
    }
}

void free_commands(struct Commands* commands){
    struct Commands* tmp;

   while (commands != NULL)
    {
       tmp = commands;
       commands = commands->next;
       free(tmp);
    }
}