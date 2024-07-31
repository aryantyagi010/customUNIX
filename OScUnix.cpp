#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <signal.h>

#define MAX_LINE 80 
#define MAX_ARGS 10 
#define HISTORY_SIZE 10 
#define JOBS_SIZE 10 

typedef struct {
    pid_t pid;
    char command[MAX_LINE];
    int running;
} job_t;

char history[HISTORY_SIZE][MAX_LINE];
int history_count = 0;
job_t jobs[JOBS_SIZE];
int job_count = 0;

void add_history(char *cmd) {
    if (history_count < HISTORY_SIZE) {
        strcpy(history[history_count], cmd);
        history_count++;
    } else {
        for (int i = 1; i < HISTORY_SIZE; i++) {
            strcpy(history[i - 1], history[i]);
        }
        strcpy(history[HISTORY_SIZE - 1], cmd);
    }
}

void show_history() {
    printf("Command History:\n");
    for (int i = 0; i < history_count; i++) {
        printf("%d: %s\n", i + 1, history[i]);
    }
}

void add_job(pid_t pid, char *command) {
    if (job_count < JOBS_SIZE) {
        jobs[job_count].pid = pid;
        strcpy(jobs[job_count].command, command);
        jobs[job_count].running = 1;
        job_count++;
    } else {
        printf("Job list full, cannot add more jobs.\n");
    }
}

void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].running = 0;
            return;
        }
    }
}

void list_jobs() {
    printf("Job List:\n");
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].running) {
            printf("[%d] %d %s\n", i + 1, jobs[i].pid, jobs[i].command);
        }
    }
}

void bring_job_to_foreground(int job_num) {
    if (job_num < 1 || job_num > job_count || !jobs[job_num - 1].running) {
        printf("Invalid job number.\n");
        return;
    }
    pid_t pid = jobs[job_num - 1].pid;
    int status;
    printf("Bringing job [%d] %d to foreground\n", job_num, pid);
    if (kill(pid, SIGCONT) == -1) {
        perror("Failed to continue job");
        return;
    }
    waitpid(pid, &status, WUNTRACED);
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        remove_job(pid);
    }
}

void continue_job_in_background(int job_num) {
    if (job_num < 1 || job_num > job_count || !jobs[job_num - 1].running) {
        printf("Invalid job number.\n");
        return;
    }
    pid_t pid = jobs[job_num - 1].pid;
    printf("Continuing job [%d] %d in background\n", job_num, pid);
    if (kill(pid, SIGCONT) == -1) {
        perror("Failed to continue job");
        return;
    }
}

void sigchld_handler(int sig) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            remove_job(pid);
        }
    }
}

void executeCommand(char *args[], int background, char *input) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    } else if (pid == 0) { // Child process
        if (execvp(args[0], args) == -1) {
            perror("Execution failed");
        }
        exit(0);
    } else { // Parent process
        if (background) {
            printf("Started background job: %d\n", pid);
            add_job(pid, input);
        } else {
            int status;
            waitpid(pid, &status, WUNTRACED);
            if (WIFSTOPPED(status)) {
                add_job(pid, input);
            }
        }
    }
}

void my_listFiles() {
    struct dirent *de;
    DIR *dr = opendir(".");
    if (dr == NULL) {
        perror("Could not open current directory");
        return;
    }
    while ((de = readdir(dr)) != NULL) {
        printf("%s\n", de->d_name);
    }
    closedir(dr);
}

void my_createFile(char *filename) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        perror("File creation failed");
        return;
    }
    printf("File created: %s\n", filename);
    fclose(fp);
}

void my_deleteFile(char *filename) {
    if (remove(filename) == 0) {
        printf("Deleted file: %s\n", filename);
    } else {
        perror("File deletion failed");
    }
}

void my_listProcesses() {
    system("ps -e");
}

void my_killProcess(char *pid) {
    if (kill(atoi(pid), SIGKILL) == 0) {
        printf("Process %s killed\n", pid);
    } else {
        perror("Failed to kill process");
    }
}

void my_memoryInfo() {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        printf("Total RAM: %ld MB\n", info.totalram / 1024 / 1024);
        printf("Free RAM: %ld MB\n", info.freeram / 1024 / 1024);
    } else {
        perror("sysinfo failed");
    }
}

void my_systemInfo() {
    printf("System information:\n");
    printf("OS: %s\n", "Linux"); 
    printf("Kernel version: ");
    system("uname -r");
}

int main(void) {
    signal(SIGCHLD, sigchld_handler); 

    char *args[MAX_ARGS + 1]; 
    char input[MAX_LINE]; 
    int should_run = 1; 
    while (should_run) {
        printf("osh> ");
        fflush(stdout);


        if (fgets(input, MAX_LINE, stdin) == NULL) {
            perror("fgets failed");
            continue;
        }

        
        size_t length = strlen(input);
        if (length > 0 && input[length - 1] == '\n') {
            input[length - 1] = '\0';
        }

        
        add_history(input);

        
        int argc = 0;
        char *token = strtok(input, " ");
        int background = 0;

        while (token != NULL) {
            args[argc] = token;
            argc++;
            token = strtok(NULL, " ");
        }
        args[argc] = NULL;

        if (argc == 0) {
            continue; 
        }

        
        if (argc > 0 && strcmp(args[argc - 1], "&") == 0) {
            background = 1;
            args[argc - 1] = NULL;
        }

        
        if (strcmp(args[0], "exit") == 0) {
            should_run = 0;
            continue;
        } else if (strcmp(args[0], "cd") == 0) {
            if (argc < 2) {
                fprintf(stderr, "cd: missing argument\n");
            } else {
                if (chdir(args[1]) != 0) {
                    perror("cd failed");
                }
            }
            continue;
        } else if (strcmp(args[0], "my_ls") == 0) {
            my_listFiles();
            continue;
        } else if (strcmp(args[0], "my_create") == 0) {
            if (argc < 2) {
                fprintf(stderr, "my_create: missing filename\n");
            } else {
                my_createFile(args[1]);
            }
            continue;
        } else if (strcmp(args[0], "my_delete") == 0) {
            if (argc < 2) {
                fprintf(stderr, "my_delete: missing filename\n");
            } else {
                my_deleteFile(args[1]);
            }
            continue;
        } else if (strcmp(args[0], "my_ps") == 0) {
            my_listProcesses();
            continue;
        } else if (strcmp(args[0], "my_kill") == 0) {
            if (argc < 2) {
                fprintf(stderr, "my_kill: missing PID\n");
            } else {
                my_killProcess(args[1]);
            }
            continue;
        } else if (strcmp(args[0], "my_meminfo") == 0) {
            my_memoryInfo();
            continue;
        } else if (strcmp(args[0], "my_sysinfo") == 0) {
            my_systemInfo();
            continue;
        } else if (strcmp(args[0], "history") == 0) {
            show_history();
            continue;
        } else if (strcmp(args[0], "jobs") == 0) {
            list_jobs();
            continue;
        } else if (strcmp(args[0], "fg") == 0) {
            if (argc < 2) {
                fprintf(stderr, "fg: missing job number\n");
            } else {
                bring_job_to_foreground(atoi(args[1]));
            }
            continue;
        } else if (strcmp(args[0], "bg") == 0) {
            if (argc < 2) {
                fprintf(stderr, "bg: missing job number\n");
            } else {
                continue_job_in_background(atoi(args[1]));
            }
            continue;
        }

        
        executeCommand(args, background, input);
    }

    return 0;
}
