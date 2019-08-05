/*
**
** 	Operating Systems PLH301 Project
**
**
** 	sysexplorer.c
**
**	Author: Konstantinos Kostis
**	A.M.  : 2011030036
**
**
**
*/

/*

  To compile and run this program, use the following line:

	gcc cache.c hashtable.c linkedlist.c search_patt.c strutils.c sysexplorer.c -pthread -o sysexplorer

*/


/**
   The relevant header files
**/
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "strutils.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>
#include "search_patt.h"
#include "cache.h"



#define PORT 11880

#define TOP_HTML "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n<doctype !html><html><head><title>Leitourgika</title>\n<style>\nh1.dirlist { background-color: yellow; }\n\
</style>\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\
</head>\n<body>\n<h1 class=\"dirlist\">Directory listing: "
#define FORM_HTML "</h1><form method=\"POST\">Find pattern <input type=\"text\" name=\"pattern\" value=\"\" /><input type=\"submit\" value=\"Find\"/><input type=\"reset\" value=\"Clear\"/></form><table><thead><tr><th>Name</th><th>Type</th><th>Size</th></tr></thead><tbody>"
#define END_HTML "</tbody></table></body></html>"

#define ERROR404(x)strconcat("HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n<doctype !html><html><head><title>Leitourgika</title><style>body { background-color: #111 }h1 { font-size:1cm; text-align: center; color: black; text-shadow: 0 0 4mm white}</style></head><body><h1>404 ERROR \r\n",x," Does not exist or it's not a directory</h1></body></html>\r\n",NULL)

/**
  Useful Preprocessor macros
**/

/* Report an error and abort */
#define FATAL_ERROR(message)						\
  {									\
    fprintf(stderr,"In %s(%d) [function %s]: %s\n",			\
	    __FILE__, __LINE__, __FUNCTION__ , (message)  );		\
    abort();								\
  }									\

/* Report a posix error (similar to perror) and abort */
#define FATAL_PERROR(errcode) FATAL_ERROR(strerror(errcode))

/* check return code of command and abort on error with suitable
   message */
#define CHECK_ERRNO(command) { if((command) == -1) FATAL_PERROR(errno); }

/* Return a copy of obj (an lvalue) in a newly malloc'd area. */
#define OBJDUP(obj) memcpy(Malloc(sizeof(obj)), &(obj), sizeof(obj))

/* Copy to obj from src, using sizeof(obj). Note that obj is an
   lvalue but src is a pointer! */
#define COPY(obj, src) (void)memcpy(&(obj), (src), sizeof(obj))

/* Allocate a new object of the given type */
#define NEW(type) ((type *) Malloc(sizeof(type)))

/**
   Error-checking replacements for library functions.
**/

/*
  Allocate n bytes and return the pointer to the allocated memory.
  This function is a wrapper for malloc. However, where malloc returns
  NULL on error (e.g. out of memory), this function will print a
  diagnostic and abort().
 */
void* Malloc(size_t n)
{
  void* new = malloc(n);
  if(new==NULL) FATAL_ERROR("Out of memory.");
  return new;
}

/*
  This function calls close(fd), checking the return code.
  It repeats the call on EINTR, and returns any other errors to the
  user.

  It is a common but nevertheless serious programming error not to
  check the return code of close().
 */
int Close(int fd)
{
  int rc;
  do { rc=close(fd); } while(rc==EINTR);
  return rc;
}

/***
   Typedefs
***/

/* Type of function that can be ran in a new thread */
typedef void* (*thread_proc)(void *);

/**
   Code
**/
char *tohtml(char*,char*);
char *generateFileList(char*);

typedef struct tcpip_connection
{
  int connfd;			/* file descriptor of connection */
  struct sockaddr_in peer_addr;	/* peer address */
} tcpip_connection;

/* Type of function that handles connections in a new thread */
typedef void* (*connection_handler)(tcpip_connection*);


/*
  Create, configure and return the file descriptor for a listening
  tcp socket on the given port, on every interface of this host.
  The returned file descriptor can be used by accept to create new TCP
  connections.

  Parameters:
  listening_port - the port on which the returned socket will listen

  Return:
  A server socket, ready to be used by accept.

  Side effects:
  In case of error, the program will abort(), printing a diagnostic
  message.
 */
int create_server_socket(int listening_port)
{
  int server_socket;	/* the file descriptor of the listening socket
			   */
  struct sockaddr_in listening_address; /* the network address of the
					   listening socket */
  int rc;		/* used to call setsockopt */


  /* create the listening socket, as an INET (internet) socket with
     TCP (stream) data transfer */
  CHECK_ERRNO(server_socket=socket(AF_INET, SOCK_STREAM, 0));
  if(server_socket!=-1){
	  fprintf(stdout,"The socket with ID=[%d] created succesfully\n",server_socket);
  }

  /* we need to set a socket option (Google for it, or ask in class) */
  rc = 1;
  CHECK_ERRNO(setsockopt(server_socket,
			 SOL_SOCKET, SO_REUSEADDR,
			 &rc, sizeof(int)));

  /* Prepare address for a call to bind.
   The specified address will be the INADDR_ANY address (meaning "every
   address on this machine" and the port.
  */
  memset(&listening_address, 0, sizeof(listening_address));
  listening_address.sin_family      = AF_INET;
  listening_address.sin_addr.s_addr = htonl(INADDR_ANY);
  listening_address.sin_port        = htons(listening_port);

  /* Bind listening socket to the listening address */
  CHECK_ERRNO(bind(server_socket,
		   (struct sockaddr*) &listening_address,
		   sizeof(listening_address)));

  /* Make server_socket a listening socket (aka server socket) */
  CHECK_ERRNO(listen(server_socket, 15));

  return server_socket;
}


/*
  Repeatedly accept connections on server_socket (a listening socket)
  and call the handler on each new connection. When server_socket is
  closed, exit.

  Parameters:
  server_socket - the socket used by accept
  handler       - a pointer to the handler function to call for each
                  new connection.

  Returns:
  nothing
 */
void server(int server_socket, connection_handler handler)
{
  int new_connection;	/* the file descriptor of new connections */
  struct sockaddr_in peer_address; /* The network address of a
				      connecting peer, as returned
				      by accept */
  socklen_t peer_addrlen;	/* Length of peer_address */
  tcpip_connection conn;	/* Object to pass to handler */

  /* Now we can accept connections on server_socket */
  while(1) {

    /* accept a new connection */
    peer_addrlen = sizeof(peer_address);
    fprintf(stdout, "Accepting connections...\n");
    new_connection = accept(server_socket,
			    (struct sockaddr*) &peer_address,
			    &peer_addrlen);

    /* check return value */
    if(new_connection==-1) {
      if( errno==EINTR		   /* Call interrupted by signal */
	  || errno==ECONNABORTED   /* connection was aborted */
	  || errno==EMFILE	   /* per-process limit of open fds */
	  || errno==ENFILE	   /* system-wide limit of open fds */
	  || errno==ENOBUFS	   /* no space for socket buffers */
	  || errno==ENOMEM	   /* no space for socket buffers */
	  || errno==EPERM	   /* Firewall blocked the connection */
	  )
	continue; /* we failed with this connection, retry with the
		     next one! */

      if(errno == EBADF)
	break;			/* return, the server_socket is closed */

      /* on all other errors, abort */
      FATAL_PERROR(errno);
    }

    /* ok, we have a valid connection */
    assert(new_connection>=0);
    conn.connfd = new_connection;
    COPY(conn.peer_addr, &peer_address);

    /* call the handler */
    handler(&conn);

  } /* end while */

}


/**

   Main program.

   Here, we create a server thread, accepting connections.

   Then we invoke repeatedly a client routine which connects
   to the server (in the same process!!), gets a message from
   the server, prints it, and quits.

   Once the clients have finished, we make the server quit as well,
   by closing the listening socket and sending a signal to the
   server thread.

   The purpose of this program is to exhibit some techiques that
   may be used in the class project.

 **/


/**
  Server functions.
**/


/*
  This function is called from server_handler to write a message
  to a socket.
*/
void write_message(int fd, const char* message)
{
  int pos, len,rc;

  pos = 0;
  len = strlen(message);

  while(pos!=len) {
  dowrite:
    rc = write(fd, message+pos, len-pos);
    if(rc<0) {
      /* ooh, an error has occurred */
      if(errno==EINTR) goto dowrite;

      /* Report the error and break */
      perror("server_handler");
      break;
    }

    /* No error */
    pos+=rc;
  }
}

/*
  This handler implements the server, running in its own thread.
  These handlers are created by threaded_server_handler
*/
void* server_handler(tcpip_connection* conn)
{

  tcpip_connection C;
  sigset_t sset;

  /* Make a local copy of conn and delete it. */
  COPY(C, conn);
  free(conn);

  /* Print a message to the user */
  fprintf(stdout,"Server: Connected to client %s:%d\n",
	 inet_ntoa(C.peer_addr.sin_addr), ntohs(C.peer_addr.sin_port));

  /* Make sure we block the SIGPIPE signal, else we may crash if the
     client closes the connection early. */
  sigemptyset(&sset);
  sigaddset(&sset, SIGPIPE);
  pthread_sigmask(SIG_BLOCK, &sset, NULL);

  /* Now write a message to the socket */


  receive_message(C.connfd);

  /* Done, close the socket */
  (void) Close(C.connfd);	/* we do not handle the return value
				   here */
  return NULL;
}


/*
  This is the handler given to the server, to start new
  connection threads.
 */
void* threaded_server_handler(tcpip_connection* conn)
{
  pthread_t server_handler_thread;
  tcpip_connection* arg;
  int rc;

  /* Make a copy of conn, to give to the new thread */
  arg = OBJDUP(*conn);

  /* Start the new thread */

  rc = pthread_create(&server_handler_thread, NULL,
		      (thread_proc) server_handler, arg);

  /* If thread was not started successfully, abort */
  if(rc) FATAL_PERROR(rc);
  fprintf(stdout, "Thread for handling the new connection created successfully");
  return NULL;
}


/*
  This global variable holds the server socket fd.
 */
int serverfd;

/* Signal handler for SIGURG, used to terminate the server.
   It does nothing itself, but the thread blocked in "accept" is
   resterted (returns EINTR, and then EBADF). */
void urg_handler(int i) { }

/* This is the thread_proc to call the server. It sets up signal
   handling for terminating the server.*/
void* server_thread(void* p)
{
	fprintf(stdout,"Starting a new server Thread...\n");
	struct sigaction sa;
	sa.sa_handler = urg_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	CHECK_ERRNO(sigaction(SIGURG, &sa, NULL));

	server(serverfd, threaded_server_handler);
	return NULL;
}

/*
 *  Description:	 Read a message from the given socket and print it to the standard output.
 *
 * 	Arguments:		int fd: to file descriptor
 *
 */

Cache cache;
void receive_message(int fd){

	List llist, frst_ln;

	List_init( &llist);
	List_init(&frst_ln);

	int frags;
	int frags_ln;
	char buffer[1000000];
	char *buffer2;
	int rc;


	struct stat fs_stat;

	struct dirent *dir;

	char *absolute_path;
	while(1) {
		if(List_empty(&llist)){

		}

  doread:
    	rc = read(fd, buffer, 1000000);
    	if(rc==-1) {
    		if(errno==EINTR) goto doread;
    		perror("client_handler");
    		break;
    	}

    	if(rc==0) {
    		/* All ok! */
    		break;
    	}

		/* Ok, we read something, so print it. */
		buffer[rc]='\0';  /* Make sure buffer is zero-terminated */

		frags= strsplit(buffer,"\r\n",&llist);
		buffer2=(char*) List_at(&llist,0);
		frags_ln= strsplit(buffer2," ", &frst_ln);
		int i;
		int rc;

	/*		GET REQUEST		*/
		if(buffer[0]=='G' && buffer[1]=='E' && buffer[2]=='T' && buffer[3]==' '){
			fprintf(stdout, "\nServer is getting a GET REQUEST\n");
			char *final2;
			absolute_path= (char*) List_at(&frst_ln,1);

			rc=cache_find(fd,&cache,absolute_path);
			if(rc!=0){	//an den vrethike i thelei update
				if((dir=opendir(absolute_path))==NULL){
					if(errno == ENOTDIR || errno == ENOENT){
						char *out;
						out = ERROR404(absolute_path);
						write_message(fd, out);
						free(out);
					}

				}else{	//200 OK
					final2=generateFileList(absolute_path);
					char *whole;
					whole=strconcat(TOP_HTML,absolute_path,FORM_HTML,final2,END_HTML,"\n",NULL);
					if(rc==-1){	//NOT FOUND IN CACHE
						Page *page;
						if((page = malloc(sizeof(Page))) == NULL){
							fprintf(stderr, "Error allocating memory to Page\n");
							abort();
						}
						page->htmllen=strlen(whole);
						page->htmtext=whole;
						page->path=absolute_path;
						if(lstat(absolute_path,&fs_stat)!=0){
							//free all
							free(page->path);
							free(whole);
							free(page);
							perror(errno);
						}else{
							page->version=fs_stat.st_ctime;
							cache_insert(&cache,page);	//prosthetoume sti cache
						}
					}else{
						lstat(absolute_path,&fs_stat);
						cache_replace_pages(&cache,absolute_path,fs_stat.st_ctime, whole);
					}
					write_message(fd, whole);
				}
			}

			List_clear(&llist);
			List_clear(&frst_ln);

	/*		POST REQUEST	*/
		}else if(buffer[0]=='P' && buffer[1]=='O' && buffer[2]=='S'&& buffer[3]=='T'&& buffer[4]==' '){
			fprintf(stdout,"\nServer is getting a POST REQUEST\n");
			char *pattern;
			List lst_ln;
			List_init(&lst_ln);
			strsplit(List_back(&llist),"=",&lst_ln);

			absolute_path=List_at(&frst_ln,1);
			pattern=List_at(&lst_ln,1);
			fprintf(stdout, "The pattern to be found is %s\n", pattern);
			List_clear(&lst_ln);

			char *form_patt;
			form_patt= search_patt(pattern,absolute_path);
			char *whole=strconcat(TOP_HTML,absolute_path,FORM_HTML,form_patt, END_HTML,"\n",NULL);
			write_message(fd, whole);

		List_clear(&llist);
		List_clear(&frst_ln);
		}

	}
  /* We are done! */
	if(rc==0) puts("\n"); /* If all went well, print 2 newlines */
}



/*
  You know ... main.
 */

int main()
{
  pthread_t server_tid;
  int rc;

  /* Create a server socket on port PORT */
  serverfd = create_server_socket(PORT);

  cache_create(&cache,100); //create cache

  /* Start a thread for the server */
  rc = pthread_create(&server_tid, NULL,
		      server_thread, NULL);


 getchar();

  /* Wait for server to exit */
  pthread_kill(server_tid, SIGURG);
  pthread_join(server_tid,NULL);
  return 0;
}


/* Description:	Pairnontas to orisma apo to xristi to ABSOLUTE_PATH tou directory kai diavazei ola ta
 * 				arxeia pou uparxoun ekei. Afou ta diavasei ta metatrpei ola se ena string kwdikopoime-
 * 				no se html
 *
 * Arguments: 	char *absolute_path
 *
 * Returns:		ena string kwdikopoiimeno se html me ta onomata twn arxeiwn
 */

char* generateFileList(char *absolute_path){
	DIR *d;
	struct dirent *dir;
	int i;

	d= opendir(absolute_path);
	if(d!=NULL){
		char *final;
		char *mid_html;
		bzero((char*)mid_html,0);
		fprintf(stdout,"The requested directory found: %s\n", absolute_path);
		while((dir= readdir(d))!=NULL){
			//metatropi twn stoixeiwn se html
			mid_html=tohtml(dir->d_name,absolute_path);
			final=strconcat(final,"\n",mid_html,NULL);
			i++;
		}
		final=strconcat(final,"\n",NULL);

		return final;
	}else{
		fprintf(stderr,"Failed to read directory at %s",absolute_path);
		return NULL;
	}
}

/**
 **
 **	Description:	metatrepei se kwdikopoiisi html ta onomata twn fakelwn kai arxeiwn mazi
 **					me ola ta stoixeia tous
 **
 **	Arguments:		char *name: to onoma tou stoixeiou
 **					char *absolute_path: to monopati sto disko gia to directory pou vrisketai to arxeio
 **
 **	Returns: 		char* : ena string kwdikopoiimeno se html
 **
 **/

char* tohtml(char *name, char *absolute_path){
	char *size;
	char *html;

	char *fullname;

	fullname=strconcat(absolute_path,"/",name,NULL);
	struct stat fs_stat;

	lstat(fullname,&fs_stat);

	if(S_ISDIR(fs_stat.st_mode)){
		html= strconcat("<tr><td><a href=\"",fullname,"\">", name , "</a></td><td>Directory</td><td></td></tr>",NULL);
	}else if(S_ISREG(fs_stat.st_mode)){
		sprintf(&size,"%llu",fs_stat.st_size);
		html= strconcat("<tr><td>",name,"</td><td>File</td><td>",&size,"</td></tr>",NULL);
	}else if(S_ISCHR(fs_stat.st_mode)){
		html= strconcat("<tr><td>",name,"</td><td>Character Device</td></tr>",NULL);
	}else if(S_ISBLK(fs_stat.st_mode)){
		html= strconcat("<tr><td>",name,"</td><td>Block special file</td></tr>",NULL);
	}else if(S_ISLNK(fs_stat.st_mode)){
		html= strconcat("<tr><td>",name,"</td><td>Symbolic Link</td></tr>",NULL);
	}else if(S_ISFIFO(fs_stat.st_mode)){
		html= strconcat("<tr><td>",name,"</td><td>FIFO Special File</td></tr>",NULL);
	}else if(S_ISSOCK(fs_stat.st_mode)){
		html= strconcat("<tr><td>",name,"</td><td>Socket File</td></tr>",NULL);
	}

	return html;
}
