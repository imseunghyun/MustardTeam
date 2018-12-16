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

char *EXIT_STRING = "exit"; // Ŭ���̾�Ʈ�� ���� ��û ���ڿ�
char *START_STRING = "----------------------CONNECTED TO  GAME SERVER----------------------\n"; 
char *WAIT_ANOTHER_STRING = "-------------------PLEASE WAIT FOR  ANOTHER CLIENT-------------------\n";  // �ٸ� Ŭ���̾�Ʈ ��ٸ���� �޼���
char *NOTI_FULL_STRING = "-------------------ALL CLIENT  CONNECTIONS COMPLETED-----------------\n";  // ��� Ŭ���̾�Ʈ ���� �Ϸ� �޼���
char *NOTI_START_STRING = "----------PLEASE MATCH THE CORRECT ANSWER BETWEEN 1 AND 200----------\n";  // 1~200 ������ ������ ���纸��� �޼���
char *WAIT_ANSWER_STRING = "---------------PLEASE WAIT FOR THE RESULT FOR A MOMENT---------------\n";  // �ٸ� Ŭ���̾�Ʈ�� ������ �����ϱ���� ��ٷ� �޶�� �޼���
char *WINNER_STRING = "---------------------CONGRATULATIONS , YOU WIN !---------------------\n"; // �¸� ���� �޼���
char *LOSER_STRING = "------------------------I'M SORRY.  YOU LOSE.------------------------\n"; // �й� ���� �޼���
char *ASK_MORE_STRING1 = "--------------WOULD YOU LIKE TO START A NEW GAME AGAIN?--------------\n";  // �� ���� ���� ���� �޼��� 1
char *ASK_MORE_STRING2 = "-----------------------(ENTER 'EXIT' to exit.)-----------------------\n";  // �� ���� ���� ���� �޼��� 2
int maxfdp1; // �ִ� ���Ϲ�ȣ+ 1
int num_chat = 0; // ä�� ������ ��
int clisock_list[MAX_SOCK]; // ä�ÿ� ������ ���Ϲ�ȣ ���
int listen_sock; // ������ ���� ����

void addClient(int s, struct sockaddr_in *newcliaddr); // ���ο� ä�� ������ ó��
int getMax(); // �ִ� ���Ϲ�ȣ ã��
void removeClient(int s); // ä�� Ż�� ó�� �Լ�
int tcp_listen(int host, int port, int backlog); // ���� ���� �� listen
void errquit(char *mesg) { perror(mesg); exit(1); } // ���� ó��
int make_answer();  // 1 ~ 200 ������ ������ ������ �����ϴ� �Լ�
int another_game_state(int me, int game_state[]);  // �ٸ� �÷��̾� ���� ������Ʈ ��ȯ�ϴ� �Լ�
void set_game_state(int game_state[], int value); // ���� ������Ʈ setter �Լ�
void set_result_buf(int answer, int me, char game_buf[][MAXLINE + 1], char result_buf[][MAXLINE + 1], int game_state[]);  // ���� ����� �����ϴ� �Լ�

int main(int argc, char *argv[]) {
   struct sockaddr_in cliaddr;
   char buf[MAXLINE + 1], game_buf[MAX_SOCK][MAXLINE + 1], result_buf[MAX_SOCK][MAXLINE + 1];
   int i, j, k, nbyte, game_nbyte[MAX_SOCK], accp_sock, addrlen = sizeof(struct sockaddr_in), game_state[MAX_SOCK] = { 0, }, winner_num = 0;
   int answer = make_answer(); // ����
   fd_set read_fds; // �б⸦ ������ fd_set ����ü

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
      if (FD_ISSET(listen_sock, &read_fds)) { // Ŭ���̾�Ʈ ���ӽ� ó���κ�
         accp_sock = accept(listen_sock, (struct sockaddr*)&cliaddr, &addrlen);
         if (accp_sock == -1)
            errquit("accept fail");
         addClient(accp_sock, &cliaddr);	// Ŭ���̾�Ʈ �߰� �޼ҵ�
         send(accp_sock, START_STRING, strlen(START_STRING), 0);	// ���� ���� �޼����� ������ Ŭ���̾�Ʈ���� ����
         printf("add user[%d].\n", num_chat);
         if (num_chat == MAX_SOCK) { // ��� Ŭ���̾�Ʈ���� ���� ���� �ϷḦ �˸��鼭 ������ ���纸��� �޼���
            for (i = 0; i < num_chat; i++) {
               send(clisock_list[i], NOTI_FULL_STRING, strlen(NOTI_FULL_STRING), 0);
               send(clisock_list[i], NOTI_START_STRING, strlen(NOTI_START_STRING), 0);
            }
         }
      }
      if (num_chat < MAX_SOCK) {  // ��� Ŭ���̾�Ʈ ���� �̿Ϸ��
         for (i = 0; i < num_chat; i++) {
            if (FD_ISSET(clisock_list[i], &read_fds)) {
               nbyte = recv(clisock_list[i], buf, MAXLINE, 0);
               if (nbyte <= 0) {  // �޼��� ����
                  removeClient(i);	
                  continue;
               }
               buf[nbyte] = 0;
               if (strstr(buf, EXIT_STRING) != NULL) {	// "exit" �޼����� �������
                  removeClient(i);
                  continue;
               }
               send(clisock_list[i], WAIT_ANOTHER_STRING, strlen(WAIT_ANOTHER_STRING), 0);	// ����ڵ鿡�� ��� �޼��� �߼�
            }
         }
      }
      else if (num_chat == MAX_SOCK) {  // ��� Ŭ���̾�Ʈ ���ӽ�
         for (i = 0; i < num_chat; i++) {
            if (FD_ISSET(clisock_list[i], &read_fds)) {
               switch (game_state[i]) {  // another_game_state(i, game_state);
               case 0:  // �ڽ� �غ� X
                  if (another_game_state(i, game_state) == 0) {  // ���� �غ� X
                     game_state[i] = 1;
                     game_nbyte[i] = recv(clisock_list[i], game_buf[i], MAXLINE, 0);
                     set_result_buf(answer, i, game_buf, result_buf, game_state);  // ���� ����� �����ϴ� �Լ�
                     if (game_nbyte[i] <= 0) {  // �޼��� ����
                        removeClient(i);
                        continue;
                     }
                     game_buf[i][game_nbyte[i]] = 0;
                     if (strstr(game_buf[i], EXIT_STRING) != NULL) {	//"exit" �޼����� �������
                        removeClient(i);
                        continue;
                     }
                     send(clisock_list[i], WAIT_ANSWER_STRING, strlen(WAIT_ANSWER_STRING), 0);	// ����ڵ鿡�� ��� �޼��� �߼�
                  }
                  else {  // ���� �غ� O
                     game_nbyte[i] = recv(clisock_list[i], game_buf[i], MAXLINE, 0);
                     set_result_buf(answer, i, game_buf, result_buf, game_state);  // ���� ����� �����ϴ� �Լ�
                     if (game_nbyte[i] <= 0) {  // �޼��� ����
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
                     if (winner_num > 0) { // �����ڰ� ������
                        for (j = 0; j < num_chat; j++) { // ���� ����
                           send(clisock_list[j], result_buf[j], strlen(result_buf[j]), 0);
                           printf("[%d]game_state : %d\n:", j, game_state[j]);
                           if (game_state[j] == 2) // ���ڿ���
                              send(clisock_list[j], WINNER_STRING, strlen(WINNER_STRING), 0);
                           else // ���ڿ���
                              send(clisock_list[j], LOSER_STRING, strlen(LOSER_STRING), 0);
                           send(clisock_list[j], ASK_MORE_STRING1, strlen(ASK_MORE_STRING1), 0);	// ������ �޼��� �߼�
                           send(clisock_list[j], ASK_MORE_STRING2, strlen(ASK_MORE_STRING2), 0);	// ������ �޼��� �߼�
                           printf("[%d] : %s\n", j, game_buf[j]);
                        }
                        set_game_state(game_state, 3);  // game_state ��� 3���� �ʱ�ȭ
                        answer = make_answer();
                     }
                     else { // ������
                        for (j = 0; j < num_chat; j++) { // ���� ����
                           send(clisock_list[j], result_buf[j], strlen(result_buf[j]), 0);
                           printf("[%d] : %s\n", j, game_buf[j]);
                        }
                        set_game_state(game_state, 0);  // game_state ��� 0���� �ʱ�ȭ
                     }
                  }
                  break;
               case 1:  // �ڽ� �غ� O
                  if (another_game_state(i, game_state) == 0) {  // ���� �غ� X
                     game_state[i] = 1;
                     nbyte = recv(clisock_list[i], buf, MAXLINE, 0);
                     if (nbyte <= 0) {  // �޼��� ����
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
                  else {  // ���� �غ� O // ��� ����ڰ� �غ� �Ϸ� ���� !!
                     nbyte = recv(clisock_list[i], buf, MAXLINE, 0);
                     if (nbyte <= 0) {  // �޼��� ����
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
                     if (winner_num > 0) { // �����ڰ� ������
                        for (j = 0; j < num_chat; j++) { // ���� ����
                           send(clisock_list[j], result_buf[j], strlen(result_buf[j]), 0);
                           if (game_state[j] == 2) // ���ڿ���
                              send(clisock_list[j], WINNER_STRING, strlen(WINNER_STRING), 0);
                           else // ���ڿ���
                              send(clisock_list[j], LOSER_STRING, strlen(LOSER_STRING), 0);
                           send(clisock_list[j], ASK_MORE_STRING1, strlen(ASK_MORE_STRING1), 0);
                           send(clisock_list[j], ASK_MORE_STRING2, strlen(ASK_MORE_STRING2), 0);
                           printf("[%d] : %s\n", j, game_buf[j]);
                        }
                        set_game_state(game_state, 3);  // game_state ��� 3���� �ʱ�ȭ
                        answer = make_answer();
                     }
                     else { // ������
                        for (j = 0; j < num_chat; j++) { // ���� ����
                           send(clisock_list[j], result_buf[j], strlen(result_buf[j]), 0);
                           printf("[%d] : %s\n", j, game_buf[j]);
                        }
                        set_game_state(game_state, 0);  // game_state ��� 0���� �ʱ�ȭ
                     }
                  }
                  break;
               case 3:  // �� ��� ����?
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

void addClient(int s, struct sockaddr_in *newcliaddr) { // ���ο� ä�� ������ ó��
   char buf[20];
   inet_ntop(AF_INET, &newcliaddr->sin_addr, buf, sizeof(buf));
   printf("new client : %s\n", buf);
   clisock_list[num_chat] = s; // ä�� Ŭ���̾�Ʈ ��Ͽ� �߰�
   num_chat++;
   maxfdp1 = getmax() + 1;  // �߰�
}

void removeClient(int s) { // ä�� Ż�� ó��
   close(clisock_list[s]);
   if (s != num_chat - 1) // ������ ���� �ƴϸ�
      clisock_list[s] = clisock_list[num_chat - 1]; // ������ ���� ���� ������ �� ��� -> �߰� ��ĭ����� -> send ���� ����
   num_chat--;
   maxfdp1 = getmax() + 1; // �߰�
   printf("one of users is disconnected. current number of users = %d\n", num_chat);
}

int getmax() { // �ִ� ���Ϲ�ȣ ã��
      // Minimum ���Ϲ�ȣ�� ���� ���� ������ listen_ sock
   int max = listen_sock;
   int i;
   for (i = 0; i < num_chat; i++)
      if (clisock_list[i] > max)
         max = clisock_list[i];
   return max;
}

int tcp_listen(int host, int port, int backlog) { // listen ���� ���� �� listen
   int sd;
   struct sockaddr_in servaddr;

   sd = socket(AF_INET, SOCK_STREAM, 0);
   if (sd == -1) {
      perror("socket fail");
      exit(1);
   }
   // servaddr ����ü�� ���� ����
   bzero((char *)&servaddr, sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr = htonl(host);
   servaddr.sin_port = htons(port);
   if (bind(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
      perror("bind fail");
      exit(1);
   }
   // Ŭ���̾�Ʈ�κ��� �����û�� ��ٸ�
   listen(sd, backlog);
   return sd;
}

int make_answer() {  // 1 ~ 200 ������ ������ ������ �����ϴ� �Լ�
   int answer;
   srand(time(NULL)); // seed��
   answer = (rand() % 200) + 1;
   return answer;
}

int another_game_state(int me, int game_state[]) {  // �ٸ� �÷��̾� ���� ������Ʈ ��ȯ�ϴ� �Լ�
   int i, result = 1;
   for (i = 0; i < MAX_SOCK; i++) {
      if (i != me)
         result *= game_state[i];
   }
   return result;  // �ϳ��� 0 �̸� 0
}

void set_game_state(int game_state[], int value) { // ���� ������Ʈ�� 0���� �ʱ�ȭ
   int i;
   for (i = 0; i < MAX_SOCK; i++) {
      game_state[i] = value;
   }
}

void set_result_buf(int answer, int me, char game_buf[][MAXLINE + 1], char result_buf[][MAXLINE + 1], int game_state[]) {  // ���� ����� �����ϴ� �Լ�
   int client_answer;
   if (strlen(game_buf[me]) > 4)
      strcpy(result_buf[me], "---------------PLEASE ENTER A NUMBER BETWEEN 1 AND 200---------------\n");
   else {
      client_answer = atoi(game_buf[me]);
      printf("[%d]clinet_answer : %d\n", me, client_answer);
      if (client_answer < 1 || client_answer > 200)
         strcpy(result_buf[me], "---------------PLEASE ENTER A NUMBER BETWEEN 1 AND 200---------------\n");
      else {
         if (answer > client_answer)  // ������ Ŭ��
            strcpy(result_buf[me], "---------------------------------U P---------------------------------\n");
         else if (answer < client_answer)  // ������ ������
            strcpy(result_buf[me], "-------------------------------D O W N-------------------------------\n");
         else if (answer == client_answer) { // �����϶�
            strcpy(result_buf[me], "-----------------------T H A T ' S   R I G H T-----------------------\n");
            game_state[me] = 2;
         }
      }
   }
}