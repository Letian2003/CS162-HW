#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue; // Only used by poolserver
int num_threads; // Only used by poolserver
int server_port; // Default value: 8000
char* server_files_directory;
char* server_proxy_hostname;
int server_proxy_port;

#define FORKSERVER 1
/*
 * Serves the contents the file stored at `path` to the client socket `fd`.
 * It is the caller's reponsibility to ensure that the file stored at `path` exists.
 */
void serve_file(int fd, char* path) {

  /* TODO: PART 2 */
  /* PART 2 BEGIN */

    struct stat statbuf;
    stat(path, &statbuf);
    int Content_Length = statbuf.st_size;
    char length[16];
    snprintf(length,16,"%d",Content_Length);

    http_start_response(fd, 200);
    http_send_header(fd, "Content-Type", http_get_mime_type(path));
    http_send_header(fd, "Content-Length", length); // TODO: change this line too
    http_end_headers(fd);


    char buf[BUFSIZ];
    memset(buf, 0, BUFSIZ);
    int filefd = open(path,O_RDONLY);

    int len;
    while((len = read(filefd,buf,BUFSIZ))>0) {
        write(fd,buf,len);
    }

    close(filefd);
    close(fd);


  /* PART 2 END */
}

void serve_directory(int fd, char* path) {
    http_start_response(fd, 200);
    http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
    http_end_headers(fd);

    /* TODO: PART 3 */
    /* PART 3 BEGIN */

    // TODO: Open the directory (Hint: opendir() may be useful here)

    /**
      * TODO: For each entry in the directory (Hint: look at the usage of  () ),
      * send a string containing a properly formatted HTML. (Hint: the http_format_href()
      * function in libhttp.c may be useful here)
      */

    char* buffer = malloc(strlen(path)+64);

    http_format_index(buffer,path);
    struct stat statbuf;

    if(stat(buffer,&statbuf)==0){
        serve_file(fd,buffer);
        return;
    }
    else{
        DIR *dir = opendir(path);
        if(dir==NULL){
            perror("opendir");
            exit(-1);
        }
        struct dirent* Dirent = (struct dirent*)malloc(sizeof(struct dirent));
        if(Dirent == NULL){
            perror("malloc dirent");
            exit(-1);
        }

        while((Dirent = readdir(dir))!=NULL){
            http_format_href(buffer,path,Dirent->d_name);
            int buffer_len = strlen(buffer);
            write(fd,buffer,buffer_len);
            write(fd,"\n",1);
        }
        closedir(dir);
        close(fd);
    }
    

    /* PART 3 END */
}

#ifdef THREADSERVER
    struct thread_server_args{
        void (*request_handler)(int);
        int fd;
    };


static void* thread_server(void *args){
    struct thread_server_args* arg = (struct thread_server_args*) args;
    int fd = arg->fd;
    void (*request_handler)(int) = arg -> request_handler;
    request_handler(fd);
}
#endif
/*
 * Reads an HTTP request from client socket (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 *
 *   Closes the client socket (fd) when finished.
 */

void handle_files_request(int fd) {

  struct http_request* request = http_request_parse(fd);


  if (request == NULL || request->path[0] != '/') {
    http_start_response(fd, 400);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return;
  }

  if (strstr(request->path, "..") != NULL) {
    http_start_response(fd, 403);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return;
  }


  /* Remove beginning `./` */
  char* path = malloc(2 + strlen(request->path) + 1);
  path[0] = '.';
  path[1] = '/';
  memcpy(path + 2, request->path, strlen(request->path) + 1);

  /*
   * TODO: PART 2 is to serve files. If the file given by `path` exists,
   * call serve_file() on it. Else, serve a 404 Not Found error below.
   * The `stat()` syscall will be useful here.
   *
   * TODO: PART 3 is to serve both files and directories. You will need to
   * determine when to call serve_file() or serve_directory() depending
   * on `path`. Make your edits below here in this function.
   */

  /* PART 2 & 3 BEGIN */

  /* part 2 */
    struct stat statbuf;


    if(stat(path,&statbuf)) {
        http_start_response(fd, 404);
        http_send_header(fd, "Content-Type", "text/html");
        http_end_headers(fd);
        close(fd);
        return;
    }

    if(S_ISDIR(statbuf.st_mode)) {
        /* directory */
        serve_directory(fd,path);
    }
    else{
        /* file */
        serve_file(fd,path);
    }
    


  /* PART 2 & 3 END */

  close(fd);
  return;
}

/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target_fd. HTTP requests from the client (fd) should be sent to the
 * proxy target (target_fd), and HTTP responses from the proxy target (target_fd)
 * should be sent to the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 *
 *   Closes client socket (fd) and proxy target fd (target_fd) when finished.
 */
struct proxy_args {
    int fd_in;
    int fd_out;
};

static void* proxy_thread(void *args){
    struct proxy_args* arg = (struct proxy_args*) args;
    int fd_in = arg->fd_in;
    int fd_out = arg->fd_out;
    printf("<proxy_thread> in = %d , out = %d\n",fd_in,fd_out);

    char buffer[BUFSIZ];
    memset(buffer,0,sizeof(buffer));
    int read_length=0;

    while((read_length = read(fd_in,buffer,BUFSIZ)) >= 0 ){
        printf("<1> read_length = %d\n",read_length);
        fprintf(stderr,"%s\n",buffer);
        write(fd_out,buffer,read_length);
    }
}
    

void handle_proxy_request(int fd) {
    //printf("<proxy>\n");
    /*
    * The code below does a DNS lookup of server_proxy_hostname and
    * opens a connection to it. Please do not modify.
    */
    struct sockaddr_in target_address;
    memset(&target_address, 0, sizeof(target_address));
    target_address.sin_family = AF_INET;
    target_address.sin_port = htons(server_proxy_port);

    // Use DNS to resolve the proxy target's IP address
    struct hostent* target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);
    // Create an IPv4 TCP socket to communicate with the proxy target.
    int target_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (target_fd == -1) {
        fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
        close(fd);
        exit(errno);
    }

    if (target_dns_entry == NULL) {
        fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
        close(target_fd);
        close(fd);
        exit(ENXIO);
    }
    char* dns_address = target_dns_entry->h_addr_list[0];

    // Connect to the proxy target.
    memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
    int connection_status =
        connect(target_fd, (struct sockaddr*)&target_address, sizeof(target_address));

    if (connection_status < 0) {
        /* Dummy request parsing, just to be compliant. */
        http_request_parse(fd);

        http_start_response(fd, 502);
        http_send_header(fd, "Content-Type", "text/html");
        http_end_headers(fd);
        close(target_fd);
        close(fd);
        return;
    }

    /* TODO: PART 4 */
    /* PART 4 BEGIN */
    const int thread_num = 2;
    //printf("<>> fd = %d , target_fd = %d\n",fd,target_fd);
    /**/
        /*
        if(fcntl(fd,F_GETFL)<0 || fcntl(target_fd,F_GETFL)<0){
            break;
        }
        */
    pthread_t threads[thread_num];
    for(int t=0;t<thread_num;t++){
        struct proxy_args* args = malloc(sizeof(struct proxy_args));
        if(t==0){
            args->fd_in = fd;
            args->fd_out = target_fd;
        }
        else{
            args->fd_in = target_fd;
            args->fd_out = fd;
        }
        if(pthread_create(&threads[t],NULL,proxy_thread,args)){
            perror("pthread_create");
            exit(-1);
        }
    }

    for(int t=0;t<thread_num;t++){
        pthread_join(threads[t],NULL);
    }
    /* mutiple request unsolved! */
    

    close(fd);
    close(target_fd);

    /* PART 4 END */
}

#ifdef POOLSERVER
/*
 * All worker threads will run this function until the server shutsdown.
 * Each thread should block until a new request has been received.
 * When the server accepts a new connection, a thread should be dispatched
 * to send a response to the client.
 */
void* handle_clients(void* void_request_handler) {
  void (*request_handler)(int) = (void (*)(int))void_request_handler;
  /* (Valgrind) Detach so thread frees its memory on completion, since we won't
   * be joining on it. */
  pthread_detach(pthread_self());

  /* TODO: PART 7 */
  /* PART 7 BEGIN */

  /* PART 7 END */
}

/*
 * Creates `num_threads` amount of threads. Initializes the work queue.
 */
void init_thread_pool(int num_threads, void (*request_handler)(int)) {

  /* TODO: PART 7 */
  /* PART 7 BEGIN */

  /* PART 7 END */
}
#endif

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int* socket_number, void (*request_handler)(int)) {
    

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  // Creates a socket for IPv4 and TCP.
  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option, sizeof(socket_option)) ==
      -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  // Setup arguments for bind()
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  /*
   * TODO: PART 1
   *
   * Given the socket created above, call bind() to give it
   * an address and a port. Then, call listen() with the socket.
   * An appropriate size of the backlog is 1024, though you may
   * play around with this value during performance testing.
   */

  /* PART 1 BEGIN */

    
  if(bind(*socket_number,(struct sockaddr*)&server_address,sizeof(server_address))){
    perror("bind");
    exit(errno);
  } 

  if(listen(*socket_number, 1024)){
    perror("listen");
    exit(errno);
  }

  /* PART 1 END */
  printf("Listening on port %d...\n", server_port);

#ifdef POOLSERVER
  /*
   * The thread pool is initialized *before* the server
   * begins accepting client connections.
   */
  init_thread_pool(num_threads, request_handler);
#endif

  while (1) {
    client_socket_number = accept(*socket_number, (struct sockaddr*)&client_address,
                                  (socklen_t*)&client_address_length);
    
    if (client_socket_number < 0) {
        perror("Error accepting socket");
        continue;
    }
#ifdef BASICSERVER
    /*
     * This is a single-process, single-threaded HTTP server.
     * When a client connection has been accepted, the main
     * process sends a response to the client. During this
     * time, the server does not listen and accept connections.
     * Only after a response has been sent to the client can
     * the server accept a new connection.
     */
    request_handler(client_socket_number);

#elif FORKSERVER
    /*
     * TODO: PART 5
     *
     * When a client connection has been accepted, a new
     * process is spawned. This child process will send
     * a response to the client. Afterwards, the child
     * process should exit. During this time, the parent
     * process should continue listening and accepting
     * connections.
     */

    /* PART 5 BEGIN */
    int cpid = fork();
    if(cpid == 0){
        /* child */
        request_handler(client_socket_number);
        printf("Accepted connection from %s on port %d\n", inet_ntoa(client_address.sin_addr),
           client_address.sin_port);
        exit(-1);
    }

    /* PART 5 END */

#elif THREADSERVER
    /*
     * TODO: PART 6
     *
     * When a client connection has been accepted, a new
     * thread is created. This thread will send a response
     * to the client. The main thread should continue
     * listening and accepting connections. The main
     * thread will NOT be joining with the new thread.
     */

    /* PART 6 BEGIN */
    pthread_t thread;
    struct thread_server_args* args =(struct thread_server_args*) malloc(sizeof(struct thread_server_args));
    args->request_handler = request_handler;
    args->fd = client_socket_number; 
    if(pthread_create(&thread,NULL,thread_server,(void*)args)){
        perror("pthread_create");
        exit(-1);
    }

    /* PART 6 END */
#elif POOLSERVER
    /*
     * TODO: PART 7
     *
     * When a client connection has been accepted, add the
     * client's socket number to the work queue. A thread
     * in the thread pool will send a response to the client.
     */

    /* PART 7 BEGIN */

    /* PART 7 END */
#endif
    
    

    printf("Accepted connection from %s on port %d\n", inet_ntoa(client_address.sin_addr),
           client_address.sin_port);
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0)
    perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char* USAGE =
    "Usage: ./httpserver --files some_directory/ [--port 8000 --num-threads 5]\n"
    "       ./httpserver --proxy inst.eecs.berkeley.edu:80 [--port 8000 --num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {
  signal(SIGINT, signal_callback_handler);
  signal(SIGPIPE, SIG_IGN);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char* proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char* colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char* server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char* num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

#ifdef POOLSERVER
  if (num_threads < 1) {
    fprintf(stderr, "Please specify \"--num-threads [N]\"\n");
    exit_with_usage();
  }
#endif

  chdir(server_files_directory);
  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
