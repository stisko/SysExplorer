#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "linkedlist.h"
#include "strutils.h"
#include <unistd.h>
#include <sys/stat.h>
#include <wait.h>
#include <libgen.h>

/**
 *
 * Description: sunartisi pou psaxnei to pattern kai gurnaei ena string kwdikopoiimeno se html
 * 				me ta stoixeia pou proekupsan apo to pattern gia eisagwgi sti selida
 *
 * Arguments: 	char *pattern to pattern pou to pirame apo to POST message
 * 				char *absolute_path to path tis diadromis
 *
 * Returns:		char* ena string kwdikopoimeno se html me ta apotelesmata tis anazitisis
 */
char *search_patt(char *pattern, char *absolute_path){
   int fds[2];
   char buffer[4096],test[50];
   List list;
   ListNode p;
   FILE *fil;
   pid_t  pid;
   struct stat st_struct;
   List_init(&list);
   int i=0;

   char *final;

   if(pipe(fds) == -1){
      perror("pipe creation failed");
      exit(1);
   }

   switch (pid=fork()){

    case 0://child
       close(fds[0]);
       dup2(fds[1], STDOUT_FILENO);
       execl("/usr/bin/find","find",strconcat(absolute_path,NULL), "-name", pattern, "-print",NULL);
       perror("find failed");

       abort();
       break;

      case -1: //fork failure
       perror("fork failure");
       exit(1);

      default: //parent
    	fil = fdopen(fds[0], "r");
    	waitpid(pid, NULL, 0 );
       close(fds[1]); //close stdin so only can do stdout
       read(fds[0],buffer,4096);

       int frags= strsplit(buffer,"\n",&list);

       fprintf(stdout, "Found %d entries in directory %s matching the requested pattern\n", frags-1, absolute_path);
       char *html;
       char *size;
       for(i=0;i<frags-1;i++){
    	   lstat(List_at(&list,i),&st_struct);
    	   fprintf(stdout, "Name: %s Size: %d \n",basename(List_at(&list, i)), st_struct.st_size);
    	   if(S_ISDIR(st_struct.st_mode)){
    		   //fprintf(stdout, "It's a directory\n");

    		   html= strconcat("<tr><td><a href=\"",List_at(&list,i),"\">", basename(List_at(&list,i)) , "</a></td><td>Directory</td><td></td></tr>",NULL);
    	   }else if(S_ISREG(st_struct.st_mode)){
    		  // fprintf(stdout, "It's a file\n");

    	   	   sprintf(&size,"%llu",st_struct.st_size);
    	   	   html= strconcat("<tr><td>",basename(List_at(&list,i)),"</td><td>File</td><td>",&size,"</td></tr>",NULL);
    	   }else if(S_ISCHR(st_struct.st_mode)){
    		   html= strconcat("<tr><td>",basename(List_at(&list,i)),"</td><td>Character Device</td><td>",&size,"</td></tr>",NULL);
    	   }else if(S_ISBLK(st_struct.st_mode)){
    		   html= strconcat("<tr><td>",basename(List_at(&list,i)),"</td><td>Block special file</td><td>",&size,"</td></tr>",NULL);
    	   }else if(S_ISLNK(st_struct.st_mode)){
    		   html= strconcat("<tr><td>",basename(List_at(&list,i)),"</td><td>Symbolic Link</td><td>",&size,"</td></tr>",NULL);
    	   }else if(S_ISFIFO(st_struct.st_mode)){
    		   html= strconcat("<tr><td>",basename(List_at(&list,i)),"</td><td>FIFO Special File</td><td>",&size,"</td></tr>",NULL);
    	   }
    	   final=strconcat(final,"\n",html,NULL);
       }
       final= strconcat(final,"\n",NULL);
   }

   close(fds[1]);
   List_clear(&list);

   return final;
}
