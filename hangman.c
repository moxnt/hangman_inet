/*
** Client for hangmand
*/

#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <arpa/inet.h>

#define PORT "2020"
#define MAXDATASIZE 100 // max number of bytes we can get at once
//
char getchar_clear() {
  int _, c;
  printf("Guess -> ");
  c = getchar();
  printf("\n");
  while ((_ = getchar()) != '\n' && _ != EOF) {
  }
  return c;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
  int sockfd;
  int numbytes = 1;
  char buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];

  if (argc != 2) {
    fprintf(stderr, "usage: client hostname\n");
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "client failure: getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client failure: socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("client failure: connect");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client failiure: couldn't connect to anything\n");
    return 2;
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s,
            sizeof s);
  printf("client: connecting to %s\n", s);

  freeaddrinfo(servinfo); // all done with this structure
  while (numbytes > 0) {
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
      perror("recv");
      exit(1);
    }

    buf[numbytes] = '\0';

    printf("Guess the word -> %s'\n", buf);

    char message[5] = "a\0";
    message[0] = getchar_clear();

    send(sockfd, message, 5, 0);
  }

  close(sockfd);

  return 0;
}
