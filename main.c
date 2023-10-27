#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MaxLine 80
#define MaxArgc 80
#define MaxJob 5

// Struct to represent a job.
typedef struct {
    pid_t pid;
    int jobid;
    char status[MaxArgc];
    char cmdline[MaxLine];
} Job;

Job jobList[MaxJob] = {
    {-1, 0, "", ""},
    {-1, 0, "", ""},
    {-1, 0, "", ""},
    {-1, 0, "", ""},
    {-1, 0, "", ""}
};
pid_t fg_pid = -1;  // Foreground process ID.

// Signal handlers for Ctrl+Z, Ctrl+C, and child termination.
void handle_sigtstp(int sig);
void handle_sigint(int sig);
void handle_sigchld(int sig);

// Functions for executing a program, changing the directory, getting the 
// current directory, and job management.
void execute_program(char *input, int background);
void Command_cd(char *input);
char *Command_pwd(char *input);
void Command_jobs(char *input);
void Command_fg(char *input);
void Command_bg(char *input);
void Command_kill(char *input);
void Command_quit();

int main() {
  char *command, *arg;
  signal(SIGINT, handle_sigint);
  signal(SIGCHLD, handle_sigchld);
  signal(SIGTSTP, handle_sigtstp);
  printf("prompt > ");
  while (1) {
    char input[MaxLine];
    fgets(input, sizeof(input), stdin);
    size_t len = strlen(input);
    if (len > 0 & input[len - 1] == '\n') {
      input[len - 1] = '\0';
    }
    int background = 0;
    if (len > 1 & input[len - 3] == ' ' & input[len - 2] == '&') {
      input[len - 2] = '\0';
      background = 1;
    }

    if (strncmp(input, "cd ", 3) == 0) {
      Command_cd(input);

    } else if (strcmp(input, "pwd") == 0) {
      char *output = Command_pwd(input);
      printf("%s", output);
    } else if (strncmp(input, "jobs", 4) == 0) {
      Command_jobs(input);
    } else if (strncmp(input, "fg ", 3) == 0) {
      Command_fg(input);
    } else if (strncmp(input, "bg ", 3) == 0) {
      Command_bg(input);
    } else if (strncmp(input, "kill ", 5) == 0) {
      Command_kill(input);
    } else if (strcmp(input, "quit") == 0) {
      Command_quit();
    } else {
      execute_program(input, background);
    }
    printf("\nprompt > ");
  }
  return 0;
}

void handle_sigtstp(int sig) {
    if (fg_pid != -1) {
        kill(fg_pid, SIGTSTP);
        for (int i = 0; i < MaxJob; i++) {
            if (jobList[i].pid == fg_pid) {
                strcpy(jobList[i].status, "Stopped");
                break;
            }
        }
    }
}

void handle_sigint(int sig) {
    if (fg_pid != -1) {
        kill(fg_pid, SIGKILL);
    }
}

void handle_sigchld(int sig) {
    int status;
    pid_t child_pid;
    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MaxJob; i++) {
            if (jobList[i].pid == child_pid) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    strcpy(jobList[i].status, "");
                    jobList[i].pid = -1;
                    jobList[i].jobid = 0;
                    strcpy(jobList[i].cmdline, "");
                } else {
                    strcpy(jobList[i].status, "Unknown");
                }
                jobList[i].pid = -1;
            }
        }
    }
}

void execute_program(char *input, int background) {
  char *args[MaxArgc];  // Array to store tokenized command arguments.
  int i = 0;
  char *token = strtok(input, " ");

  // Tokenize the input string by spaces to get individual arguments.
  while (token) {
    args[i++] = token;
    token = strtok(NULL, " ");
  }
  args[i] = NULL;
  int len = i;

  // Count the number of active jobs.
  int count = 0;
  for (int i = 0; i < MaxJob; i++) {
    if (jobList[i].jobid != 0) {
      count++;
    }
  }
  // Check if the maximum number of processes has been reached.
  if (count == MaxJob) {
    perror("Error: max processes reached");
    return;
  } else {
    for (int i = 0; i < MaxJob; i++) {
      if (jobList[i].jobid == 0) {
        // Set the job's status and command line based on the mode
        // (foreground/background).
        if (background) {
          strcpy(jobList[i].status, "Background/Running");
          strcpy(jobList[i].cmdline, input);
          strcat(jobList[i].cmdline, " &");

        } else {
          strcpy(jobList[i].status, "Foreground/Running");
          strcpy(jobList[i].cmdline, input);
        }

        jobList[i].pid = fork();
        jobList[i].jobid = i + 1;
        if (jobList[i].pid < 0) {
          perror("Error in fork");
          return;
        } else if (jobList[i].pid == 0) {
          char *input_file = NULL;
          char *output_file = NULL;
          int append = 0;

          // Parse the arguments for input and output redirection.
          for (int i = 1; i < len; i++) {
            if (strcmp(args[i], "<") == 0 && i + 1 < len) {
              input_file = args[i + 1];
              i++;
            } else if (strcmp(args[i], ">") == 0 && i + 1 < len) {
              output_file = args[i + 1];
              i++;
            } else if (strcmp(args[i], ">>") == 0 && i + 1 < len) {
              output_file = args[i + 1];
              append = 1;
              i++;
            }
          }
          // Handle input redirection.
          if (input_file) {
            int input_fd = open(input_file, O_RDONLY);
            if (input_fd == -1) {
              perror("open");
              continue;
            }
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
          }
          // Handle output redirection.
          if (output_file) {
            int flags = O_WRONLY | O_CREAT;
            if (append) {
              flags |= O_APPEND;
            } else {
              flags |= O_TRUNC;
            }
            int output_fd = open(output_file, flags, 0666);
            if (output_fd == -1) {
              perror("open");
              continue;
            }
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
          }

          if (background) {
            signal(SIGTSTP, SIG_IGN);
            signal(SIGINT, SIG_IGN);
          } else {
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
          }
          if (strchr(args[0], '/')) {
            execv(args[0], args);
          } else {
            execvp(args[0], args);
          }
          // If the command doesn't include a path, try executing from the
          // current directory.
          if (!strchr(args[0], '/')) {
            char currentDirectoryCommand[MaxLine + 2];
            sprintf(currentDirectoryCommand, "./%s", args[0]);
            execv(currentDirectoryCommand, args);
            break;
          }
          perror("Error in exec");
          exit(EXIT_FAILURE);
        } else {
          // parent process.
          if (!background) {
            fg_pid = jobList[i].pid;
            int status;
            waitpid(fg_pid, &status, WUNTRACED);
            if (WIFEXITED(status)) {
              strcpy(jobList[i].status, "");
              jobList[i].pid = -1;
              jobList[i].jobid = 0;
              strcpy(jobList[i].cmdline, "");
            } else if (WIFSTOPPED(status)) {
              strcpy(jobList[i].status, "Stopped");
            }
            break;
          } else {
            printf("Background process executed");
            break;
          }
        }
      }
    }
  }
}

void Command_cd(char *input) {
    if (chdir(input + 3) != 0) {
        perror("Error changing directory");
    }
}

char *Command_pwd(char *input) {
    static char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return ("Error getting current directory");
    } else {
        return cwd;
    }
}

void Command_jobs(char *input) {
  char *args[MaxArgc];
  int i = 0;
  char *token = strtok(input, " ");
  while (token) {
    args[i++] = token;
    token = strtok(NULL, " ");
  }
  args[i] = NULL;
  int len = i;

  char *input_file = NULL;
  char *output_file = NULL;
  int append = 0;

  for (int i = 1; i < len;
       i++) {  // Loop through the tokenized arguments to check for input/output
               // redirection operators.
    if (strcmp(args[i], "<") == 0 && i + 1 < len) {
      input_file = args[i + 1];
      i++;
    } else if (strcmp(args[i], ">") == 0 && i + 1 < len) {
      output_file = args[i + 1];
      i++;
    } else if (strcmp(args[i], ">>") == 0 && i + 1 < len) {
      output_file = args[i + 1];
      append = 1;
      i++;
    }
  }
  int original_stdin = dup(STDIN_FILENO);
  if (input_file) {
    int input_fd = open(input_file, O_RDONLY);
    if (input_fd == -1) {
      perror("open");
    }

    dup2(input_fd, STDIN_FILENO);
    close(input_fd);
  }

  int original_stdout = dup(STDOUT_FILENO);
  if (output_file) {
    int flags = O_WRONLY | O_CREAT;
    if (append) {
      flags |= O_APPEND;
    } else {
      flags |= O_TRUNC;
    }
    int output_fd = open(output_file, flags, 0666);
    if (output_fd == -1) {
      perror("open");
    }
    dup2(output_fd, STDOUT_FILENO);
    close(output_fd);
  }

  for (int i = 0; i < MaxJob; i++) {
    if (jobList[i].jobid == 0) {
      continue;
    }
    printf("[%d] (%d) <%s> <%s>\n", jobList[i].jobid, jobList[i].pid,
           jobList[i].status, jobList[i].cmdline);
  }
  dup2(original_stdin, STDIN_FILENO);
  dup2(original_stdout, STDOUT_FILENO);
}

void Command_fg(char *input) {
  pid_t target_pid = -1;
  int is_jid = 0;

  if (input[3] == '%') {
    is_jid = 1;
    int jid = atoi(input + 4);
    for (int i = 0; i < MaxJob; i++) {
      if (jobList[i].jobid == jid) {
        target_pid = jobList[i].pid;
        break;
      }
    }
  } else {
    target_pid = atoi(input + 3);
  }

  if (target_pid == -1) {
    printf("No such job\n");
    return;
  }

  for (int i = 0; i < MaxJob; i++) {
    if (jobList[i].pid == target_pid) {
      strcpy(jobList[i].status, "Foreground/Running");
      fg_pid = target_pid;
      kill(target_pid, SIGCONT);

      int status;
      waitpid(fg_pid, &status, WUNTRACED);

      if (WIFSTOPPED(status)) {
        strcpy(jobList[i].status, "Stopped");
        printf("\nProcess %d stopped.\n", fg_pid);
      } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
        strcpy(jobList[i].status, "");
        jobList[i].pid = -1;
        jobList[i].jobid = 0;
        strcpy(jobList[i].cmdline, "");
        fg_pid = -1;
      }
      return;
    }
  }
}

void Command_bg(char *input) {
  pid_t target_pid = -1;

  if (input[3] == '%') {
    int jid = atoi(input + 4);
    for (int i = 0; i < MaxJob; i++) {
      if (jobList[i].jobid == jid) {
        target_pid = jobList[i].pid;
        break;
      }
    }
  } else {
    target_pid = atoi(input + 3);
  }

  if (target_pid == -1) {
    printf("No such job\n");
    return;
  }

  for (int i = 0; i < MaxJob; i++) {
    if (jobList[i].pid == target_pid &&
        strcmp(jobList[i].status, "Stopped") == 0) {
      strcpy(jobList[i].status, "Background/Running");
      kill(target_pid, SIGCONT);
      return;
    }
  }
}

void Command_kill(char *input) {
  pid_t target_pid = -1;

  if (input[5] == '%') {
    int jid = atoi(input + 6);
    for (int i = 0; i < MaxJob; i++) {
      if (jobList[i].jobid == jid) {
        target_pid = jobList[i].pid;
        break;
      }
    }
  } else {
    target_pid = atoi(input + 5);
  }

  if (target_pid == -1) {
    printf("No such job\n");
    return;
  }
  for (int i = 0; i < MaxJob; i++) {
    if (jobList[i].pid == target_pid) {
      strcpy(jobList[i].status, "");
      jobList[i].pid = -1;
      jobList[i].jobid = 0;
      strcpy(jobList[i].cmdline, "");
      break;
    }
  }
  kill(target_pid, SIGKILL);
}

void Command_quit() {
  for (int i = 0; i < MaxJob; i++) {
    if (jobList[i].pid != -1) {
      kill(jobList[i].pid, SIGKILL);
    }
  }
  exit(0);
}
