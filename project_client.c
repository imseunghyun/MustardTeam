#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAXLINE 1000
#define NAME_LEN 20

char *EXIT_STRING = "exit";

int tcp_connect(int af, char *servip, unsigned short port); // 소켓 생성 및 서버 연결, 생성된 소켓 리턴
void errquit(char *mesg) { perror(mesg); exit(1); }

int main(int argc, char *argv[]) {
   //char bufall[MAXLINE + NAME_LEN], *bufmsg; // 이름 + 메시지를 위한 버퍼, bufall에서 메시지부분의 포인터
   char buf[MAXLINE + 1];
   int maxfdp1, s, namelen; // 최대 소켓 디스크립터, 소켓, 이름의 길이
   fd_set read_fds;

   if (argc != 4) {		// 명령인수 입력 오류처리
      printf("Usage : %s server_ip_address  port_number name\n", argv[0]);
      exit(0);
   }

   //sprintf(bufall, "[%s] : ", argv[3]); // bufall의 앞 부분에 이름을 저장
   //namelen = strlen(bufall);
   //bufmsg = bufall + namelen; // 메시지 시작 부분 지정
   s = tcp_connect(AF_INET, argv[1], atoi(argv[2]));
   if (s == -1)
      errquit("tcp_connect fail");
   puts("access to the server.");
   maxfdp1 = s + 1;
   FD_ZERO(&read_fds); // client에는 왜 while 밖에 있을까? -> 두개로 고정되어있으니까
   while (1) {
      FD_SET(0, &read_fds); // 키보드
      FD_SET(s, &read_fds);
      if (select(maxfdp1, &read_fds, NULL, NULL, NULL) < 0)
         errquit("select fail");
      if (FD_ISSET(s, &read_fds)) {		// server에서 오는 신호(게임시작, 승리 축하, 패배 위로 등의 메세지) 처리
         int nbyte;
         if ((nbyte = recv(s, buf, MAXLINE, 0)) > 0) {
            buf[nbyte] = 0;
            printf("%s\n", buf);
         }
      }
      if (FD_ISSET(0, &read_fds)) {		// client 입력 처리 (서버에 메세지 전송 , exit 입력시 게임종료) 처리부분
         if (fgets(buf, MAXLINE, stdin)) {
            if (send(s, buf, strlen(buf), 0) < 0)
               puts("Error : Write error on socket.");
            if (strstr(buf, EXIT_STRING) != NULL) {
               puts("Good bye.");
               close(s);
               exit(0);
            }
         }
      }
   }
}

int tcp_connect(int af, char *servip, unsigned short port) {
   struct sockaddr_in servaddr;
   int s;
   if ((s = socket(af, SOCK_STREAM, 0)) < 0)  // 소켓 생성
      return -1;
   // 채팅 서버의 소켓 주소 구조체 servaddr 초기화
   bzero((char *)&servaddr, sizeof(servaddr));
   servaddr.sin_family = af;
   inet_pton(AF_INET, servip, &servaddr.sin_addr);
   servaddr.sin_port = htons(port);
   // 연결요청
   if (connect(s, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
      return -1;
   return s;
}