#include "ForkProcessSandbox.h"

#include "Logger.h"
#include "ExecutionResult.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <llvm/Support/FileSystem.h>

using namespace std::chrono;

static pid_t mullFork(const char *processName) {
//  static int childrenCount = 0;
//  childrenCount++;
  const pid_t pid = fork();
  if (pid == -1) {
    mull::Logger::error() << "Failed to create " << processName
//                            << " after creating " << childrenCount
//                            << " child processes\n";
                          << "\n";
    mull::Logger::error() << strerror(errno) << "\n";
    mull::Logger::error() << "Shutting down\n";
    exit(1);
  }
  return pid;
}

static std::string readFileAndUnlink(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    perror("fopen");
    printf("fopen: %s\n", filename);
    return std::string("Mull error: could not read output");
  }
  if (unlink(filename) == -1) {
    perror("unlink");
    printf("unlink: %s\n", filename);
  }

  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(sizeof(char) * (size + 1));
  fread(buffer, sizeof(char), size, file);
  buffer[size] = '\0';
  std::string output(buffer);
  free(buffer);
  fclose(file);

  return output;
}

void handle_alarm_signal(int signal, siginfo_t *info, void *context) {
  _exit(mull::ForkProcessSandbox::MullTimeoutCode);
}

void handle_timeout(long long timeoutMilliseconds) {
  struct sigaction action{};
  memset(&action, 0, sizeof(action));
  action.sa_sigaction = &handle_alarm_signal;
  if (sigaction(SIGALRM, &action, nullptr) != 0) {
    perror("sigaction");
    abort();
  }

  struct itimerval timer{};
  timer.it_value.tv_sec = timeoutMilliseconds / 1000;
  /// Cut off seconds, and convert what's left into microseconds
  timer.it_value.tv_usec = (timeoutMilliseconds % 1000) * 1000;

  /// Do not repeat
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;

  if (setitimer(ITIMER_REAL, &timer, nullptr) != 0) {
    perror("setitimer");
    abort();
  }
}

mull::ExecutionResult
mull::ForkProcessSandbox::run(std::function<ExecutionStatus (void)> function,
                              long long timeoutMilliseconds) {
  llvm::SmallString<32> stderrFilename;
  llvm::sys::fs::createUniqueFile("/tmp/mull.stderr.%%%%%%", stderrFilename);

  llvm::SmallString<32> stdoutFilename;
  llvm::sys::fs::createUniqueFile("/tmp/mull.stdout.%%%%%%", stdoutFilename);

  /// Creating a memory to be shared between child and parent.
  ExecutionStatus *sharedStatus = (ExecutionStatus *)mmap(nullptr,
                                                          sizeof(ExecutionStatus),
                                                          PROT_READ | PROT_WRITE,
                                                          MAP_SHARED | MAP_ANONYMOUS,
                                                          -1,
                                                          0);

  auto start = high_resolution_clock::now();
  const pid_t workerPID = mullFork("worker");
  if (workerPID == 0) {
    freopen(stderrFilename.c_str(), "w", stderr);
    freopen(stdoutFilename.c_str(), "w", stdout);

    handle_timeout(timeoutMilliseconds);

    *sharedStatus = function();

    fflush(stderr);
    fflush(stdout);
    _exit(MullExitCode);
  } else {
    int status = 0;
    pid_t pid = 0;
    while ( (pid = waitpid(workerPID, &status, 0)) == -1 ) {}

    auto elapsed = high_resolution_clock::now() - start;
    ExecutionResult result;
    result.runningTime = duration_cast<std::chrono::milliseconds>(elapsed).count();
    result.exitStatus = WEXITSTATUS(status);
    result.stderrOutput = readFileAndUnlink(stderrFilename.c_str());
    result.stdoutOutput = readFileAndUnlink(stdoutFilename.c_str());
    result.status = *sharedStatus;

    int munmapResult = munmap(sharedStatus, sizeof(ExecutionStatus));

    /// Check that mummap succeeds:
    /// "On success, munmap() returns 0, on failure -1, and errno is set (probably to EINVAL)."
    /// http://linux.die.net/man/2/munmap
    assert(munmapResult == 0);
    (void)munmapResult;

    if (WIFSIGNALED(status)) {
      result.status = Crashed;
    }

    else if (WIFEXITED(status) && WEXITSTATUS(status) == MullTimeoutCode) {
      result.status = Timedout;
    }

    else if (WIFEXITED(status) && WEXITSTATUS(status) != MullExitCode) {
      result.status = AbnormalExit;
    }

    return result;
  }
}

mull::ExecutionResult mull::NullProcessSandbox::run(std::function<ExecutionStatus (void)> function,
                                                    long long timeoutMilliseconds) {
  ExecutionResult result;
  result.status = function();
  return result;
}
