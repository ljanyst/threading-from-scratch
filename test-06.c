
#include <tb.h>
#include <string.h>

//------------------------------------------------------------------------------
// Signal handler, we don't print anything here because tbprint takes a lock
//------------------------------------------------------------------------------
tbthread_key_t key;
extern int tb_pid;
void sigusr_handler(int sig, siginfo_t *si, void *ctx)
{
  int *sig_status = (int *)tbthread_getspecific(key);
  if (sig == SIGUSR1 && si->si_pid == tb_pid && si->si_code == SI_TKILL)
    *sig_status = 1;
}

//------------------------------------------------------------------------------
// Thread function
//------------------------------------------------------------------------------
void *thread_func(void *arg)
{
  tbthread_t self = tbthread_self();
  int sig_status = 0;
  tbthread_setspecific(key, &sig_status);
  for(int i = 0; i < 5; ++i) {
    tbprint("[thread 0x%llx] Hello! Signal status: %d\n", self, sig_status);
    tbsleep(1);
  }
  return arg;
}

//------------------------------------------------------------------------------
// Start the show
//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
  tbthread_init();

  //----------------------------------------------------------------------------
  // Install the signal handler
  //----------------------------------------------------------------------------
  struct sigaction sa;
  int st = 0;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = (__sighandler_t)sigusr_handler;
  sa.sa_flags = SA_SIGINFO;
  st = tbsigaction(SIGUSR1, &sa, 0);
  if(st) {
    tbprint("Unable to install sigaction handler: %s\n", tbstrerror(-st));
    goto exit;
  }

  //----------------------------------------------------------------------------
  // Spawn the threads
  //----------------------------------------------------------------------------
  tbthread_key_create(&key, 0);
  tbthread_t       thread[5];
  tbthread_attr_t  attr;
  tbthread_attr_init(&attr);

  tbprint("[thread main] Spawning threads. PID: %d\n", tb_pid);
  for(int i = 0; i < 5; ++i) {
    st = tbthread_create(&thread[i], &attr, thread_func, 0);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
  }

  tbprint("[thread main] Threads spawned successfully\n");

  //----------------------------------------------------------------------------
  // Sending signal to threads
  //----------------------------------------------------------------------------
  for(int i = 0; i < 5; ++i) {
    tbprint("[thread main] Sending SIGUSR1 to thread #%d\n", i);
    SYSCALL3(__NR_tgkill, tb_pid, thread[i]->tid, SIGUSR1);
  }

  //----------------------------------------------------------------------------
  // Joining threads
  //----------------------------------------------------------------------------
  for(int i = 0; i < 5; ++i) {
    void *ret;
    st = tbthread_join(thread[i], &ret);
    if(st != 0) {
      tbprint("Failed to join thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
  }

exit:
  tbthread_key_delete(key);
  tbthread_finit();
  return 0;
}
