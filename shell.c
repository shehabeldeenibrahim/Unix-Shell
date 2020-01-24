#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#define _GNU_SOURCE
#define MAX_LINE 80 /* The maximum length command */
int parse(char* in, char* args[MAX_LINE/2 + 1]);
int checkForRedirection(char* args[MAX_LINE/2 + 1]);
int checkForPipe(char* args2[MAX_LINE/2 + 1]);
int main(void)
{
  char in[MAX_LINE/2 + 1]; /* command line arguments */
  int should_run = 1; /* flag to determine when to exit program */
  char* args[MAX_LINE/2 + 1]; /* array of strings of instruction*/
  char* args2[MAX_LINE/2 + 1]; /* array of strings of instruction IF THERE'S A PIPE*/
  int last_string; /* index of last string in array of params*/

  bool concurrent = false; /* flag to determince if the parent and child 
                                execute concurrently*/
  bool start = true;
  char previous[MAX_LINE/2 + 1]; /*string having the previous instruction*/

  int directive; /* position of < or >*/
  int stdoutCopy;
  int stdinCopy;
  bool dir_out_before = false;
  bool dir_in_before = false;
  int pipe_process;
  strcpy(previous , "No command in history \n");

  while (should_run)
  {
    concurrent = false;
    if(dir_out_before)
    { 
      if(dup2(stdoutCopy,1) < 0) {}    // Change stdout back from the clone
        close(stdoutCopy);                      // Close the   
      dir_out_before = false;
    }
    if(dir_in_before)
    { 
      if(dup2(stdinCopy,0) < 0) {}    // Change stdout back from the clone
        close(stdinCopy);                      // Close the   
      dir_in_before = false;
    }

    printf("osh>");
    fflush(stdout); 

    fgets(in,MAX_LINE,stdin);


    /*if at first instruction and no history keep asking for input*/
    while(start && in[0] == '!' && in[1] == '!')
    {
      printf("%s", previous);
      printf("osh>");
      fgets(in,MAX_LINE,stdin);
    }

    /*check if !! repeat previous instruction*/
    if(in[0] != '!')
    {
      strcpy(previous , in);
      start = false;
    }
    else if(in[0] == '!'&& in[1] == '!')
    {
      strcpy(in , previous);
      printf("%s", in);
    }

    /*parsing input and getting index of the last argument*/
    last_string =  parse(in, args);

     /*check if exit command*/
    if(strcmp(args[0], "exit\n") == 0)
      return 0;

    /*Nulling rest of the arguments*/
    for(int k= last_string+1; k<(MAX_LINE/2 + 1); k++)
    {
      args[k] = NULL;
    }
    
    /*removing endline char from each argument*/
    for(int j= 0; j<last_string+1; j++)
    {
      int len = strlen(args[j]);
      char temp = args[j][len-1];
      if(temp == '\n')
        args[j][len - 1] = '\0';
    }

    
    /*check if concurrent*/
    if(args[last_string][0] == '&')
    {
      concurrent = true;
      args[last_string] = NULL;
    }

    /*check if !!*/
    if(strcmp(args[0], "!!") == 0)
    {
      
      args[last_string] = NULL;
    }

    directive = checkForRedirection(args);
    pipe_process = checkForPipe(args);

    /*if > or < exist*/
    if(directive >= 0)
    { 
      

      if(strcmp(args[directive], ">")==0)/*output to file*/
      {
          if(access(args[directive+1], F_OK) == -1 )
          {
            FILE *fp;
            // file doesn't exists
            fp = fopen(args[directive+1], "w");
            fputs("" ,fp);
            fclose( fp );
          } 

        int file_desc = open(args[directive+1], O_WRONLY | O_APPEND);
        if(file_desc < 0) 
          printf("Error opening the file\n"); 
        stdoutCopy = dup(1);                // Clone stdout to a new descriptor
        if(dup2(file_desc, 1) < 0) {}         // Change stdout to file
        close(file_desc);                            // stdout is still valid
        dir_out_before = true;
      }     
      else if(strcmp(args[directive], "<")==0) /*input to file*/
      {
        if(access(args[directive+1], F_OK) == -1 )
          {
            FILE *fp;
            // file doesn't exists
            fp = fopen(args[directive+1], "w");
            fputs("" ,fp);
            fclose( fp );
          } 
        int in = open(args[directive+1], O_RDONLY);
        if(in < 0) 
          printf("Error opening the file\n"); 
        stdinCopy = dup(0);                // Clone stdout to a new descriptor
        if(dup2(in,0) < 0) {}         // Change stdout to file
        close(in);                            // stdout is still valid
        dir_in_before = true;
      }
      for(int a = directive; a < MAX_LINE/2 + 1; a++)
      {
        args[a] = NULL;
      }
    }

    pid_t pid;



    if(pipe_process < 0) /*if not pipe*/
    {  /* fork a child process */
       pid = fork();
       if (pid < 0)
        { 
          
          /* error occurred */
          fprintf(stderr, "Fork Failed");
          return 1;
        }

        else if (pid == 0)
        { 
          
          /* child process */
          if(execvp(args[0], args)<0)
          {
            printf("*** ERROR: exec failed\n");
            exit(0);
          }
        }
        else
        {
          /* parent process */
          /* parent will wait for the child to complete */
          int status1;
          if(concurrent == false)
            waitpid(pid, &status1, 0);
            
        }
    }
    else /*if pipe*/
    {
      /*copy second command to args2*/
      int c = 0;
      args[pipe_process] = NULL;
      for(int f = pipe_process+1; f <= last_string; f++)
      {
        args2[c] = args[f];
        args[f] = NULL;
        c++;
      }
      /*null the rest of args2*/
      for(int h = c; h < MAX_LINE/2 + 1; h++)
      {
        args2[h] = NULL;
      }
      int pipe_[2]; /*declare pipe*/
      /* fork a child process */
      pid = fork();
      pipe(pipe_); /*create pipe*/
      if (pid < 0)
      { 
        
        /* error occurred */
        fprintf(stderr, "Fork Failed");
        return 1;
      }

      else if (pid == 0)
      { 

        close(pipe_[0]);
        dup2(pipe_[1], STDIN_FILENO);
        /* child process */
        if(execvp(args[0], args)<0)
        {
          printf("*** ERROR: exec failed\n");
          exit(0);
        }
      }
      else
      {
        pid_t pid2 = fork();

        if(pid2==0)
        {
          close(pipe_[1]);
          dup2(pipe_[0], STDIN_FILENO);
          if(execvp(args2[0], args2)<0)
          {
            printf("*** ERROR: exec failed\n");
            exit(0);
          }
        }
        /* parent process */
        close(pipe_[0]);
        close(pipe_[1]);
        
        
        int status;
        if(concurrent==false)
	        waitpid(pid2, &status, 0);
          
      }

    }

   
   }
  
return 0;
}

int checkForRedirection(char* args[MAX_LINE/2 + 1])
{
  int i = 0;
  while(args[i] != NULL)
  {
    if(strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0)
      return i; /*return position of < or >*/
    i++;
  }
  return -1; /*no < or > in command*/
}

int checkForPipe(char* args2[MAX_LINE/2 + 1])
{
  int i = 0;
  while(args2[i] != NULL)
  {
    if(strcmp(args2[i], "|") == 0)
      return i; /*return position of < or >*/
    i++;
  }
  return -1; /*no < or > in command*/
}


int parse(char* in, char* args[MAX_LINE/2 + 1])
{
  
  const char s[2] = " ";
  char *token;
  int i = 0;
  //get the first token 
  token = strtok(in, s);
   
  //get the rest of the tokens
  while( token != NULL )
  {
    //strcpy(*args, token);
    args[i] = token;
    token = strtok(NULL, s);
    //args++;
    i++;
  }
  return i-1;
}