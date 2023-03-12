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

int main(int argc,char** argv){
    int in = open("acc.txt",O_RDONLY);
    int out = open("out.txt",O_RDWR|O_TRUNC);
    close(in);

    printf("%d\n %d\n",fcntl(in,F_GETFL),fcntl(out,F_GETFL));

    char buf[512];
    int len=0;
    memset(buf,0,sizeof(buf));

    while((len = read(in,buf,512))>0){
        
        int offset=0;
        while(offset<len){
            int writelen = write(out,buf+offset,len-offset);
            offset+=writelen;
        }
    }

    close(in);
    close(out);

     close(in);
    close(out);

    return 0;
}