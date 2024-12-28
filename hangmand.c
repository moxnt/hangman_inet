#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_LENGHT 50
#define PORT "25565"
#define PENDING 3 // Allowed pending connections
#define TERMINATOR_LENGHT 1
#define SUCCESS 0

unsigned int reveal(char character, char *word, unsigned int word_lenght);
short unique_guess(char character, char *guesses);
char getchar_clear();
void init_hint(unsigned int lenght, char *hint);
void update_hint(unsigned int lenght, char *hint, char *word, char guess);
void *get_in_addr(struct sockaddr *sa);

// Dead process stuff, still need to figure it out
void sigchld_handler(int s) {
  int saved_errno = errno;

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  errno = saved_errno;
}

int main(int argc, char *argv[]) {
  // Socket stuff
  struct addrinfo hints, *server_info, *p;
  int socket_fd, new_fd;
  socklen_t sin_size;
  struct sockaddr_storage their_addr;
  char s[INET6_ADDRSTRLEN];
  int yes;

  memset(&hints, 0, sizeof hints);
  hints.ai_flags = AI_PASSIVE;     // This pcs IP
  hints.ai_family = AF_UNSPEC;     // IpvAny
  hints.ai_socktype = SOCK_STREAM; // Stream sockets

  if (getaddrinfo(NULL, PORT, &hints, &server_info) != 0) {
    perror("Can't get address info");
    return 1;
  }

  for (p = server_info; p != NULL; p = p->ai_next) {
    if ((socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
        -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) ==
        -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
      close(socket_fd);
      perror("server: bind");
      continue;
    }

    break;
  }
  freeaddrinfo(server_info); // all done with this structure

  if (p == NULL) {
    perror("server: socket");
    exit(1);
  }

  if (listen(socket_fd, PENDING) == -1) {
    perror("listen");
    exit(1);
  }

  // dead process stuff, won't work on windows
  struct sigaction sa;
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  // Game stuff
  FILE *file_ptr;
  char word_buffer[MAX_LENGHT];
  char guess_buffer[MAX_LENGHT] = "\0";
  char hint_buffer[MAX_LENGHT];
  char recv_buffer[10];
  char guess;
  unsigned int current_word_lenght = 0; // includes null terminator \0
  unsigned int revealed = 0;

  file_ptr = fopen("words.txt", "r");

  if (NULL == file_ptr) {
    fprintf(stderr, "Could not open file.\n");
    exit(1);
  }

  while (fgets(word_buffer, MAX_LENGHT, file_ptr)) {

    sin_size = sizeof their_addr;
    new_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &sin_size);

    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s);
    printf("server: got connection from %s\n", s);

    if (!fork()) {      // this is the child process
      close(socket_fd); // child doesn't need the listener
      // MAIN STUFF
      while (word_buffer[current_word_lenght] != '\0') {
        current_word_lenght += 1;
      }

      init_hint(current_word_lenght, hint_buffer);

      char message[current_word_lenght + 20];

      while (revealed < (current_word_lenght - TERMINATOR_LENGHT)) {

        printf("Guess the word: %s\n", hint_buffer);
        sprintf(message, "Guess: %s\n", hint_buffer);
        send(new_fd, message, current_word_lenght + 20, 0);
        recv(new_fd, recv_buffer, 5, 0);
        guess = recv_buffer[0];

        if (unique_guess(guess, guess_buffer) == SUCCESS) {
          revealed += reveal(guess, word_buffer, current_word_lenght);
          update_hint(current_word_lenght, hint_buffer, word_buffer, guess);
        }
        printf("Letters :%d\n", revealed);
      }
      // MAIN STUFF
      close(new_fd);
      exit(0);
    }
    //
    close(new_fd); // parent doesn't need this
    //

    current_word_lenght = 0;
    guess_buffer[0] = '\0';
    revealed = 0;
  }

  fclose(file_ptr);
  return 0;
}

void req_check() { return; }

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void update_hint(unsigned int lenght, char *hint, char *word, char guess) {
  for (int i = 0; i < (lenght - 1); i += 1) {
    if (word[i] == guess) {
      hint[i] = guess;
    }
  }
}

void init_hint(unsigned int lenght, char *hint) {
  for (int i = 0; i < (lenght - 1); i += 1) {
    hint[i] = '_';
  }
  hint[lenght - 1] = '\0';
}

unsigned int reveal(char character, char *word, unsigned int word_lenght) {
  unsigned int revealed = 0;
  for (int i = 0; i < (word_lenght - 1); i += 1) {
    if (word[i] == character) {
      revealed += 1;
    }
  }
  return revealed;
}

// TLDR: This function is unsafe.
// There better be a terminating character not at the end of buffer or you get
// UB, your fault. 0 - unique, 1 - not unique
short unique_guess(char character, char *guesses) {
  int i;
  for (i = 0; guesses[i] != '\0'; i += 1) {
    if (guesses[i] == character) {
      return 1;
    }
  }
  guesses[i] = character;
  guesses[i + 1] = '\0';
  return 0;
}

char getchar_clear() {
  int _, c;
  printf("Guess -> ");
  c = getchar();
  printf("\n");
  while ((_ = getchar()) != '\n' && _ != EOF) {
  }
  return c;
}
