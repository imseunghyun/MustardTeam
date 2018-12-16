#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define MAXLINE 511
#define MAX_SOCK 2 // socket number

char *EXIT_STRING = "exit"; // 클라이언트의 종료 요청 문자열
char *START_STRING = "----------------------CONNECTED TO  GAME SERVER----------------------\n"; 
char *WAIT_ANOTHER_STRING = "-------------------PLEASE WAIT FOR  ANOTHER CLIENT-------------------\n";  // 다른 클라이언트 기다리라는 메세지
char *NOTI_FULL_STRING = "-------------------ALL CLIENT  CONNECTIONS COMPLETED-----------------\n";  // 모든 클라이언트 접속 완료 메세지
char *NOTI_START_STRING = "----------PLEASE MATCH THE CORRECT ANSWER BETWEEN 1 AND 200----------\n";  // 1~200 사이의 정답을 맞춰보라는 메세지
char *WAIT_ANSWER_STRING = "---------------PLEASE WAIT FOR THE RESULT FOR A MOMENT---------------\n";  // 다른 클라이언트가 정답을 전송하기까지 기다려 달라는 메세지
char *WINNER_STRING = "---------------------CONGRATULATIONS , YOU WIN !---------------------\n"; // 승리 축하 메세지
char *LOSER_STRING = "------------------------I'M SORRY.  YOU LOSE.------------------------\n"; // 패배 위로 메세지
char *ASK_MORE_STRING1 = "--------------WOULD YOU LIKE TO START A NEW GAME AGAIN?--------------\n";  // 새 게임 시작 물음 메세지 1
char *ASK_MORE_STRING2 = "-----------------------(ENTER 'EXIT' to exit.)-----------------------\n";  // 새 게임 시작 물음 메세지 2
int maxfdp1; // 최대 소켓번호+ 1
int num_chat = 0; // 채팅 참가자 수
int clisock_list[MAX_SOCK]; // 채팅에 참가자 소켓번호 목록
int listen_sock; // 서버의 리슨 소켓

void addClient(int s, struct sockaddr_in *newcliaddr); // 새로운 채팅 참가자 처리
int getMax(); // 최대 소켓번호 찾기
void removeClient(int s); // 채팅 탈퇴 처리 함수
int tcp_listen(int host, int port, int backlog); // 소켓 생성 및 listen
void errquit(char *mesg) { perror(mesg); exit(1); } // 에러 처리
int make_answer();  // 1 ~ 200 까지의 랜덤한 정답을 생성하는 함수
int another_game_state(int me, int game_state[]);  // 다른 플레이어 게임 스테이트 반환하는 함수
void set_game_state(int game_state[], int value); // 게임 스테이트 setter 함수
void set_result_buf(int answer, int me, char game_buf[][MAXLINE + 1], char result_buf[][MAXLINE + 1], int game_state[]);  // 정답 결과를 저장하는 함수

int main(int argc, char *argv[]) {
   struct sockaddr_in cliaddr;
   char buf[MAXLINE + 1], game_buf[MAX_SOCK][MAXLINE + 1], result_buf[MAX_SOCK][MAXLINE + 1];
   int i, j, k, nbyte, game_nbyte[MAX_SOCK], accp_sock, addrlen = sizeof(struct sockaddr_in), game_state[MAX_SOCK] = { 0, }, winner_num = 0;
   int answer = make_answer(); // 정답
   fd_set read_fds; // 읽기를 감지할 fd_set 구조체

   if (argc != 2) {
      printf("Usage : %s port_number\n", argv[0]);
      exit(0);
   }
   listen_sock = tcp_listen(INADDR_ANY, atoi(argv[1]), 5);
   maxfdp1 = listen_sock + 1;
   while (1) {
      FD_ZERO(&read_fds);
      FD_SET(listen_sock, &read_fds);
      for (i = 0; i < num_chat; i++)
         FD_SET(clisock_list[i], &read_fds);
      puts("wait for client");
      if (select(maxfdp1, &read_fds, NULL, NULL, NULL) < 0)
         errquit("select fail");
      if (FD_ISSET(listen_sock, &read_fds)) { // 클라이언트 접속시 처리부분
         accp_sock = accept(listen_sock, (struct sockaddr*)&cliaddr, &addrlen);
         if (accp_sock == -1)
            errquit("accept fail");
         addClient(accp_sock, &cliaddr);	// 클라이언트 추가 메소드
         send(accp_sock, START_STRING, strlen(START_STRING), 0);	// 게임 시작 메세지를 접속한 클라이언트에게 전송
         printf("add user[%d].\n", num_chat);
         if (num_chat == MAX_SOCK) { // 모든 클라이언트에게 전원 접속 완료를 알리면서 정답을 맞춰보라는 메세지
            for (i = 0; i < num_chat; i++) {
               send(clisock_list[i], NOTI_FULL_STRING, strlen(NOTI_FULL_STRING), 0);
               send(clisock_list[i], NOTI_START_STRING, strlen(NOTI_START_STRING), 0);
            }
         }
      }
      if (num_chat < MAX_SOCK) {  // 모든 클라이언트 접속 미완료시
         for (i = 0; i < num_chat; i++) {
            if (FD_ISSET(clisock_list[i], &read_fds)) {
               nbyte = recv(clisock_list[i], buf, MAXLINE, 0);
               if (nbyte <= 0) {  // 메세지 오류
                  removeClient(i);	
                  continue;
               }
               buf[nbyte] = 0;
               if (strstr(buf, EXIT_STRING) != NULL) {	// "exit" 메세지를 받은경우
                  removeClient(i);
                  continue;
               }
               send(clisock_list[i], WAIT_ANOTHER_STRING, strlen(WAIT_ANOTHER_STRING), 0);	// 사용자들에게 대기 메세지 발송
            }
         }
      }
      else if (num_chat == MAX_SOCK) {  // 모든 클라이언트 접속시
         for (i = 0; i < num_chat; i++) {
            if (FD_ISSET(clisock_list[i], &read_fds)) {
               switch (game_state[i]) {  // another_game_state(i, game_state);
               case 0:  // 자신 준비 X
                  if (another_game_state(i, game_state) == 0) {  // 상대방 준비 X
                     game_state[i] = 1;
                     game_nbyte[i] = recv(clisock_list[i], game_buf[i], MAXLINE, 0);
                     set_result_buf(answer, i, game_buf, result_buf, game_state);  // 정답 결과를 저장하는 함수
                     if (game_nbyte[i] <= 0) {  // 메세지 오류
                        removeClient(i);
                        continue;
                     }
                     game_buf[i][game_nbyte[i]] = 0;
                     if (strstr(game_buf[i], EXIT_STRING) != NULL) {	//"exit" 메세지를 받은경우
                        removeClient(i);
                        continue;
                     }
                     send(clisock_list[i], WAIT_ANSWER_STRING, strlen(WAIT_ANSWER_STRING), 0);	// 사용자들에게 대기 메세지 발송
                  }
                  else {  // 상대방 준비 O
                     game_nbyte[i] = recv(clisock_list[i], game_buf[i], MAXLINE, 0);
                     set_result_buf(answer, i, game_buf, result_buf, game_state);  // 정답 결과를 저장하는 함수
                     if (game_nbyte[i] <= 0) {  // 메세지 오류
                        removeClient(i);
                        continue;
                     }
                     game_buf[i][game_nbyte[i]] = 0;
                     if (strstr(game_buf[i], EXIT_STRING) != NULL) {
                        removeClient(i);
                        continue;
                     }
                     for (k = 0; k < num_chat; k++)
                        if (game_state[k] == 2)
                           winner_num = (k + 1);
                     if (winner_num > 0) { // 정답자가 있으면
                        for (j = 0; j < num_chat; j++) { // 정답 공개
                           send(clisock_list[j], result_buf[j], strlen(result_buf[j]), 0);
                           printf("[%d]game_state : %d\n:", j, game_state[j]);
                           if (game_state[j] == 2) // 승자에게
                              send(clisock_list[j], WINNER_STRING, strlen(WINNER_STRING), 0);
                           else // 패자에게
                              send(clisock_list[j], LOSER_STRING, strlen(LOSER_STRING), 0);
                           send(clisock_list[j], ASK_MORE_STRING1, strlen(ASK_MORE_STRING1), 0);	// 더할지 메세지 발송
                           send(clisock_list[j], ASK_MORE_STRING2, strlen(ASK_MORE_STRING2), 0);	// 더할지 메세지 발송
                           printf("[%d] : %s\n", j, game_buf[j]);
                        }
                        set_game_state(game_state, 3);  // game_state 모두 3으로 초기화
                        answer = make_answer();
                     }
                     else { // 없으면
                        for (j = 0; j < num_chat; j++) { // 정답 공개
                           send(clisock_list[j], result_buf[j], strlen(result_buf[j]), 0);
                           printf("[%d] : %s\n", j, game_buf[j]);
                        }
                        set_game_state(game_state, 0);  // game_state 모두 0으로 초기화
                     }
                  }
                  break;
               case 1:  // 자신 준비 O
                  if (another_game_state(i, game_state) == 0) {  // 상대방 준비 X
                     game_state[i] = 1;
                     nbyte = recv(clisock_list[i], buf, MAXLINE, 0);
                     if (nbyte <= 0) {  // 메세지 오류
                        removeClient(i);
                        continue;
                     }
                     buf[nbyte] = 0;
                     if (strstr(buf, EXIT_STRING) != NULL) {
                        removeClient(i);
                        continue;
                     }
                     send(clisock_list[i], WAIT_ANSWER_STRING, strlen(WAIT_ANSWER_STRING), 0);
                  }
                  else {  // 상대방 준비 O // 모든 사용자가 준비 완료 상태 !!
                     nbyte = recv(clisock_list[i], buf, MAXLINE, 0);
                     if (nbyte <= 0) {  // 메세지 오류
                        removeClient(i);
                        continue;
                     }
                     buf[nbyte] = 0;
                     if (strstr(buf, EXIT_STRING) != NULL) {
                        removeClient(i);
                        continue;
                     }
                     for (k = 0; k < num_chat; k++)
                        if (game_state[k] == 2)
                           winner_num = (k + 1);
                     if (winner_num > 0) { // 정답자가 있으면
                        for (j = 0; j < num_chat; j++) { // 정답 공개
                           send(clisock_list[j], result_buf[j], strlen(result_buf[j]), 0);
                           if (game_state[j] == 2) // 승자에게
                              send(clisock_list[j], WINNER_STRING, strlen(WINNER_STRING), 0);
                           else // 패자에게
                              send(clisock_list[j], LOSER_STRING, strlen(LOSER_STRING), 0);
                           send(clisock_list[j], ASK_MORE_STRING1, strlen(ASK_MORE_STRING1), 0);
                           send(clisock_list[j], ASK_MORE_STRING2, strlen(ASK_MORE_STRING2), 0);
                           printf("[%d] : %s\n", j, game_buf[j]);
                        }
                        set_game_state(game_state, 3);  // game_state 모두 3으로 초기화
                        answer = make_answer();
                     }
                     else { // 없으면
                        for (j = 0; j < num_chat; j++) { // 정답 공개
                           send(clisock_list[j], result_buf[j], strlen(result_buf[j]), 0);
                           printf("[%d] : %s\n", j, game_buf[j]);
                        }
                        set_game_state(game_state, 0);  // game_state 모두 0으로 초기화
                     }
                  }
                  break;
               case 3:  // 원 모어 게임?
                  nbyte = recv(clisock_list[i], buf, MAXLINE, 0);
                  if (nbyte <= 0) {
                     removeClient(i);
                     continue;
                  }
                  buf[nbyte] = 0;
                  if (strstr(buf, EXIT_STRING) != NULL) {
                     removeClient(i);
                     continue;
                  }
                  send(clisock_list[i], WAIT_ANOTHER_STRING, strlen(WAIT_ANOTHER_STRING), 0);
                  game_state[i] = 0;
                  winner_num = 0;
                  break;
               }
            }
         }
      }
   }
   return  0;
}

void addClient(int s, struct sockaddr_in *newcliaddr) { // 새로운 채팅 참가자 처리
   char buf[20];
   inet_ntop(AF_INET, &newcliaddr->sin_addr, buf, sizeof(buf));
   printf("new client : %s\n", buf);
   clisock_list[num_chat] = s; // 채팅 클라이언트 목록에 추가
   num_chat++;
   maxfdp1 = getmax() + 1;  // 추가
}

void removeClient(int s) { // 채팅 탈퇴 처리
   close(clisock_list[s]);
   if (s != num_chat - 1) // 마지막 놈이 아니면
      clisock_list[s] = clisock_list[num_chat - 1]; // 마지막 놈의 값을 지워질 놈에 써라 -> 중간 빈칸지우기 -> send 오류 방지
   num_chat--;
   maxfdp1 = getmax() + 1; // 추가
   printf("one of users is disconnected. current number of users = %d\n", num_chat);
}

int getmax() { // 최대 소켓번호 찾기
      // Minimum 소켓번호는 가장 먼저 생성된 listen_ sock
   int max = listen_sock;
   int i;
   for (i = 0; i < num_chat; i++)
      if (clisock_list[i] > max)
         max = clisock_list[i];
   return max;
}

int tcp_listen(int host, int port, int backlog) { // listen 소켓 생성 및 listen
   int sd;
   struct sockaddr_in servaddr;

   sd = socket(AF_INET, SOCK_STREAM, 0);
   if (sd == -1) {
      perror("socket fail");
      exit(1);
   }
   // servaddr 구조체의 내용 세팅
   bzero((char *)&servaddr, sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr = htonl(host);
   servaddr.sin_port = htons(port);
   if (bind(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
      perror("bind fail");
      exit(1);
   }
   // 클라이언트로부터 연결요청을 기다림
   listen(sd, backlog);
   return sd;
}

int make_answer() {  // 1 ~ 200 까지의 랜덤한 정답을 생성하는 함수
   int answer;
   srand(time(NULL)); // seed값
   answer = (rand() % 200) + 1;
   return answer;
}

int another_game_state(int me, int game_state[]) {  // 다른 플레이어 게임 스테이트 반환하는 함수
   int i, result = 1;
   for (i = 0; i < MAX_SOCK; i++) {
      if (i != me)
         result *= game_state[i];
   }
   return result;  // 하나라도 0 이면 0
}

void set_game_state(int game_state[], int value) { // 게임 스테이트를 0으로 초기화
   int i;
   for (i = 0; i < MAX_SOCK; i++) {
      game_state[i] = value;
   }
}

void set_result_buf(int answer, int me, char game_buf[][MAXLINE + 1], char result_buf[][MAXLINE + 1], int game_state[]) {  // 정답 결과를 저장하는 함수
   int client_answer;
   if (strlen(game_buf[me]) > 4)
      strcpy(result_buf[me], "---------------PLEASE ENTER A NUMBER BETWEEN 1 AND 200---------------\n");
   else {
      client_answer = atoi(game_buf[me]);
      printf("[%d]clinet_answer : %d\n", me, client_answer);
      if (client_answer < 1 || client_answer > 200)
         strcpy(result_buf[me], "---------------PLEASE ENTER A NUMBER BETWEEN 1 AND 200---------------\n");
      else {
         if (answer > client_answer)  // 정답이 클때
            strcpy(result_buf[me], "---------------------------------U P---------------------------------\n");
         else if (answer < client_answer)  // 정답이 작을때
            strcpy(result_buf[me], "-------------------------------D O W N-------------------------------\n");
         else if (answer == client_answer) { // 정답일때
            strcpy(result_buf[me], "-----------------------T H A T ' S   R I G H T-----------------------\n");
            game_state[me] = 2;
         }
      }
   }
}