#include "minet_socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <cstring>

#define BUFSIZE 1024
#define FILENAMESIZE 100

int handle_connection(int);
int writenbytes(int,char *,int);
int readnbytes(int,char *,int);

int main(int argc,char *argv[])
{
  int server_port;
  int sock,sock2;
  struct sockaddr_in sa,sa2;
  int rc,i;
  fd_set readlist;
  fd_set connections;
  int maxfd;

  /* parse command line args */
  if (argc != 3)
  {
    fprintf(stderr, "usage: http_server1 k|u port\n");
    exit(-1);
  }
  server_port = atoi(argv[2]);
  if (server_port < 1500)
  {
    fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
    exit(-1);
  }

  /* initialize and make socket */
  minet_init(MINET_KERNEL);
  sock = minet_socket(SOCK_STREAM);
  if(sock < 0)
  {
      fprintf(stderr, "Could not create socket\n");
      exit(-1);
  }

  /* set server address*/
  memset(&sa,0,sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = INADDR_ANY;
  sa.sin_port = htons(server_port);

  /* bind listening socket */
  rc = minet_bind(sock, (struct sockaddr_in *)&sa);
  if (rc < 0)
  {
    fprintf(stderr, "Could not bind\n");
    exit(-1);
  }

  /* start listening */
  rc = minet_listen(sock,1);
  if(rc < 0)
  {
    fprintf(stderr, "Could not listen\n");
    exit(-1);
  }


  FD_ZERO(&connections);
  FD_ZERO(&readlist);
  maxfd = sock;
  /* connection handling loop */
  while(1)
  {
    // /* create read list */
    readlist = connections;
    FD_SET(sock,&readlist);
    // /* do a select */
    rc = minet_select(maxfd+1,&readlist,NULL,NULL,NULL);
    if(rc < 0)
    {
        fprintf(stderr, "No select\n");
        exit(-1);
    }
    // /* process sockets that are ready */
    for (i = 0; i < maxfd+1; ++i)
    {
      /* for the accept socket, add accepted connection to connections */
      if (i == sock && FD_ISSET(i,&readlist))
      {
        sock2 = minet_accept(sock, (struct sockaddr_in *)&sa2);
        if(sock2>=0)
        {
          FD_SET(sock2,&connections);
          if(sock2 > maxfd)
          {
            maxfd = sock2;
          }
        }
        
      }
       /* for a connection socket, handle the connection */
      else if (i != sock && FD_ISSET(i,&readlist))
      {
        rc = handle_connection(i);
        FD_CLR(i,&connections);
      }
    }

    
  }
}

int handle_connection(int sock2)
{
  char filename[FILENAMESIZE+1];
  FILE * file_obj;
  int rc;
  int fd;
  struct stat filestat;
  char buf[BUFSIZE+1];
  char *headers;
  char *endheaders;
  char *bptr;
  int datalen=0;
  char *ok_response_f = "HTTP/1.0 200 OK\r\n"\
                      "Content-type: text/plain\r\n"\
                      "Content-length: %d \r\n\r\n";
  char ok_response[100];
  char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                         "Content-type: text/html\r\n\r\n"\
                         "<html><body bgColor=black text=white>\n"\
                         "<h2>404 FILE NOT FOUND</h2>\n"
                         "</body></html>\n";
  bool ok=true;
  char * bufout;

  

/* first read loop -- get request and headers*/
  datalen = 0;
  bptr = buf;
  memset(buf,0,BUFSIZE);
  while ((rc = minet_read(sock2,bptr,BUFSIZE))>0)
  {
    bptr += rc;
    datalen += rc;
    if(strcmp(bptr-4,"\r\n\r\n")==0)
    {
      break;
    }

  }
  if(datalen == 0)
  {
    fprintf(stderr, "Could not read\n");
    exit(-1);
  }
  bptr = buf;

  


  /* parse request to get file name */
  /* Assumption: this is a GET request and filename contains no spaces*/
  int count = 0;
  int filecount = 0;
  memset(filename,0,sizeof(filename));
  while(buf[count] != ' ')
  {
    count++;
  }
  count++;
  while(buf[count] != ' ')
  {
    if(buf[count]!='/')
    {
      filename[filecount]=buf[count];
      filecount++;
    }
    count++;
  }

  /* try opening the file */
  fd = stat(filename,&filestat);
  datalen = filestat.st_size;
  
  if(fd==-1)
  {
    ok = false;
  }
  else
  {
    ok = true;

  }

    

  /* send response */
  if (ok)
  {
    /* send headers */
    sprintf(ok_response, ok_response_f, datalen);
    rc = writenbytes(sock2,ok_response,strlen(ok_response));
    if(rc < 0)
    {
      fprintf(stderr, "Could not write\n");
      exit(-1);
    }

    file_obj = fopen(filename,"rb");
    if(file_obj == NULL)
    {

    }
    bufout = (char*) malloc (sizeof(char)*datalen);
    rc = fread(bufout,1,datalen,file_obj);
    fclose(file_obj);
    rc = writenbytes(sock2,bufout,datalen);
    free(bufout);
    if(rc < 0)
    {
      fprintf(stderr, "Could not write\n");
      exit(-1);
    }

    /* send file */
  }
  else // send error response
  {
    rc = minet_write(sock2,notok_response,strlen(notok_response));
    if(rc < 0)
    {
      fprintf(stderr, "Could not write\n");
      exit(-1);
    }
  }

  /* close socket and free space */
  minet_close(sock2);

  if (ok)
    return 0;
  else
    return -1;
}

int readnbytes(int fd,char *buf,int size)
{
  int rc = 0;
  int totalread = 0;
  while ((rc = minet_read(fd,buf+totalread,size-totalread)) > 0)
    totalread += rc;

  if (rc < 0)
  {
    return -1;
  }
  else
    return totalread;
}

int writenbytes(int fd,char *str,int size)
{
  int rc = 0;
  int totalwritten =0;
  while ((rc = minet_write(fd,str+totalwritten,size-totalwritten)) > 0)
    totalwritten += rc;

  if (rc < 0)
    return -1;
  else
    return totalwritten;
}

