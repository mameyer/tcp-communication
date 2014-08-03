#include <semaphore.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

// Einkapselung der POSIX-Semaphoren.

class Sema {
    sem_t sema;
    enum { RCODE_OK = 0 };
    static void check(int rcode, char *s) {
	if (rcode != 0) {
	    perror(s);
	    exit(255);
	}
    }

public:
  Sema(int initial_count, int shared = 1) {
    check(sem_init(&sema, shared, initial_count), (char *)"sem_init");
  }
  
  ~Sema(){ 
    check(sem_destroy(&sema), (char *)"sem_destroy");
  } 
  
  void P() {
    check(sem_wait(&sema), (char *)"sem_wait"); 
  }
  
  void V() {
    check(sem_post(&sema), (char *)"sem_post"); 
  }

};
