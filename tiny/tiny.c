/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

// 한 개의 HTTP 트랜잭션을 처리, rio_readlineb함수를 사용해서 요청 라인을 읽는다, Tiny는 GET 메소드만 지원, 다른 메소드(ex, POST)를 요청하면 에러를 보내고, main 루틴으로 돌아옴, 연결을 닫고 다음 연결 요청을 기다림

void doit(int fd) 
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  if (!Rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest
      return;
  printf("Request headers : \n");     // 923p 기준
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  // 클라이언트가 GET 아닌 다른 메소드를 요청하면, 에러메시지
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented",
              "Tiny does not implement this method");
    return;
  }

  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);

  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", 
              "Tiny couldn't find this file");
    return;
}
  //정적 컨텐츠라면
  if (is_static) { /* Serve static content */
  // 보통 파일이고 읽기 권한이 있는가 검증
  if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
    clienterror(fd, filename, "403", "Forbidden",
                "Tiny couldn't read the file");
    return;
  }
  // 해당하면 클라이언트에게 제공
  serve_static(fd, filename, sbuf.st_size);
  }

  // 동적 컨텐츠라면
  else { /* Serve dynamic content */
    // 보통 파일이고 읽기 권한이 있는가 검증
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", 
                  "Tiny couldn't run the CGI program");
      return;
    }
    // 해당하면 클라이언트에게 제공
    serve_dynamic(fd, filename, cgiargs);
  }
}

// 에러 처리
//clienterror 함수는 HTTP 응답을 응답 라인에 적절한 상태 코드와 상태메시지와 함께 클라이언트에 보내며, 브라우저 사용자에게 에러를 설명하는 응답 본체에 HTML 파일을 함께보낸다
// HTML 응답은 본체에서 컨텐츠의 크기와 타입을 나타내야한다
void clienterror(int fd, char *cause, char *errnum,
                char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

//???
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

//parse_uri
// URI 분석
// 정적 컨텐츠은 현재 디렉토리, 실행파일의 홈 디렉토리는 "/cgi-bin"
// 동적 컨텐츠는 스트링 cgi-bin을 포함하는 모든 URI, 기본 파일 이름은 ./home.html

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  // Static content

  if (!strstr(uri, "cgi-bin")) {
    strcpy(cgiargs, "");                // cgiargs(인자스트링) 안에 "" 넣어서 비우기
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    return 1;
  }

  // Dynamic content
  else {
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");      
    strcpy(filename, ".");
    strcat(filename, uri); // 매핑된 가상메모리 주소를 반환, 메모리 누수를 피하는데 중요
    return 0;
  }
}

// get_filetype - Derive file type from filename
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  // 11.07 연습문제, 동영상 mp4 파일 설정
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mpg");
  else 
    strcpy(filetype, "text/plain");
}

// 정적 컨텐츠 클라이언트에게 제공
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  // 파일 타입 검사
  get_filetype(filename, filetype);

  // 응답라인 응답헤더 보내기
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  //11.09 연습문제
  /* Send response body to client */
  // 요청파일을 오픈해서 식별자 srcfd에 가져온다.
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = (char*)Malloc(filesize);
  Rio_readn(fd, srcp, filesize);
  Close(srcfd);
  // 실제파일을 클라이언트에게 전송한다.
  // Rio_writen함수는 주소 srcp에서 시작하는 filesize바이트를 클라이언트 연결 식별자로 복사
  Rio_writen(fd, srcp, filesize);
  free(srcp);
}


// 동적 컨텐츠 클라이언트에게 제공
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // Child
  // 새로운 자식 프로세스를 fork한다.
  if (Fork() == 0) {
    /* Real server would set all CGI vars here */
    // cgiargs 환경변수들로 초기화
    setenv("QUERY)STRING", cgiargs, 1);
    // Redirect stdout to client
    // 자식은 자신의 연결파일 식별자로 재지정
    Dup2(fd, STDOUT_FILENO);
    // Run CGI program
    Execve(filename, emptylist, environ);
  }
  // Parent waits for and reaps child
  Wait(NULL);
}