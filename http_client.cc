#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <cstring>

#define BUFSIZE 1024

int write_n_bytes(int fd, char * buf, int count);

int main(int argc, char * argv[]) {
    char * server_name = NULL;
    int server_port = 0;
    char * server_path = NULL;

    int sock = 0;
    int rc = -1;
    // int datalen = 0;
    bool ok = true;
    struct sockaddr_in sa;
    // FILE * wheretoprint = stdout;
    struct hostent * site = NULL;
    // char * req = NULL;

    char buf[BUFSIZE + 1];
    char * bptr = NULL;
    // char * bptr2 = NULL;
    char * endheaders = NULL;
   
    // struct timeval timeout;
    // fd_set set;

    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];



    /* initialize minet */
    if (toupper(*(argv[1])) == 'K') { 
	minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') { 
	minet_init(MINET_USER);
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    /* create socket */
    sock = minet_socket(SOCK_STREAM);
    if(sock < 0)
    {
        fprintf(stderr, "Could not create socket\n");
        exit(-1);
    }

    // Do DNS lookup
    /* Hint: use gethostbyname() */
    site = gethostbyname(server_name);
    if(site == NULL)
    {
        fprintf(stderr, "Could not find host name\n");
        exit(-1);
    }

    /* set address */
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = *(unsigned long *)site->h_addr_list[0];
    sa.sin_port = htons(server_port);


    /* connect socket */
    if(minet_connect(sock, (struct sockaddr_in*) &sa) < 0)
    {
        fprintf(stderr, "Could not connect\n");
        exit(-1);
    }

    /* send request */
    char req[80]= "GET /";
    strcat(req,server_path);
    strcat(req," HTTP/1.0\r\n\r\n");

    // if(write(sock,req,strlen(req))<=0)
    if(minet_write(sock,req,strlen(req))<0)
    {
        fprintf(stderr, "Could not write\n");
        exit(-1);
    }


    /* wait till socket can be read */
    /* Hint: use select(), and ignore timeout for now. */
    bptr = buf;
    memset(buf,0,BUFSIZE);
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock,&readfds);
    int maxfd = sock;

    rc = minet_select(maxfd+1,&readfds,NULL,NULL,NULL);
    if(rc < 0)
    {
        fprintf(stderr, "No select\n");
        exit(-1);
    }
    /* first read loop -- read headers */
    if(FD_ISSET(sock,&readfds))
    {
        rc = minet_read(sock, buf, BUFSIZE);
        if(rc<0)
        {
            fprintf(stderr, "Could not read\n");
            exit(-1);
        }
    }

    /* examine return code */   
    //Skip "HTTP/1.0"
    //remove the '\0'
    // Normal reply has return code 200
    int count = 0;
    int codecount = 0;
    char code[4];
    memset(code,0,sizeof(code));
    while(buf[count] != ' ')
    {
    count++;
    }
    count++;
    while(codecount < 3)
    {
      code[codecount]=buf[count];
      codecount++;
      count++;
    }

    // printf("%s\n", buf);
    /* print first part of response */
    if(strcmp(code,"200")==0)
    {
        ok = true;
        endheaders = strstr(buf,"\r\n\r\n");
        if(endheaders != NULL)
        {
            endheaders += 4;
            if(endheaders != NULL)
            {
                fprintf(stdout,"%s", endheaders);
            }
        }
    }
    else
    {
        ok = false;
        fprintf(stderr, "%s", buf);
    }

    
    

    /* second read loop -- print out the rest of the response */
    if(ok)
    {

        FD_SET(sock,&readfds);
        while(1)
        {
            memset(buf,0,BUFSIZE);
            rc = minet_select(maxfd+1,&readfds,NULL,NULL,NULL);
            if(rc < 0)
            {
                fprintf(stderr, "No select\n");
                exit(-1);
            }
            if(FD_ISSET(sock,&readfds))
            {
                rc = minet_read(sock, buf, BUFSIZE);
                if(rc<0)
                {
                    fprintf(stderr, "Could not read\n");
                    exit(-1);
                }
                else if(rc>0)
                {
                    fprintf(stdout,"%s", buf);
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }

    }
    

    /*close socket and deinitialize */
    minet_close(sock);
    minet_deinit();

    if (ok) {
	return 0;
    } else {
	return -1;
    }
}

int write_n_bytes(int fd, char * buf, int count) {
    int rc = 0;
    int totalwritten = 0;

    while ((rc = minet_write(fd, buf + totalwritten, count - totalwritten)) > 0) {
	totalwritten += rc;
    }
    
    if (rc < 0) {
	return -1;
    } else {
	return totalwritten;
    }
}


