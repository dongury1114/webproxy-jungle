/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"


int main() {
  char *buf, *p, *method;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;
  
  // QUERY_STRING에서 두 개의 인자를 추출
  // QUERY_STRING은 URI에서 클라이언트가 보낸, 인자 즉, id=HTML&name=egoing 부분이다.
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&'); // buf 문자열에서 '&'를 가리키는 포인터를 반환
    *p = '\0'; // buf 문자열에서 '&'를 '\0'으로 바꿈
    strcpy(arg1, buf); // buf 문자열에서 \0 전까지의 문자열을 arg1에 넣음
    strcpy(arg2, p + 1); // buf 문자열에서 \0 뒤로의 문자열을 arg2에 넣음
    n1 = atoi(arg1);// arg1에서 오로지 숫자만 뽑아서 int형으로 바꿈
    n2 = atoi(arg2);  // arg2에서 오로지 숫자만 뽑아서 int형으로 바꿈.
  }
  
  method = getenv("REQUEST_METHOD");
    
  // content에 응답 본체를 담음
  sprintf(content, "QUERY_STRING=%s", buf); // content에 QUERY_STRING=buf를 저장
  sprintf(content, "Welcome to add.com: [Dynamic Content(adder.c)] ");
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);   // 인자를 처리
  sprintf(content, "%sThanks for visiting!\r\n", content);
  
  // 응답을 출력
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content)); // content의 길이를 출력 
  printf("Content-type: text/html\r\n\r\n");
  
  if (strcasecmp(method, "HEAD") != 0) {
    printf("%s", content);
  }
  fflush(stdout);  // 응답 본체를 클라이언트에 출력, 출력 버퍼를 지움
  
  exit(0);
}
/* $end adder */