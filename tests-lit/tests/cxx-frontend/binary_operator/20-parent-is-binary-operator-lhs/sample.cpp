extern "C" {
extern int printf(const char *, ...);
}

int sum_times() {
  for (int i = 0; i < 10; i = i + 1) {
    if (i < 0) {
      return 0;
    }
  }
  return 1;
}

int main() {
  if (sum_times() == 1) {
    printf("NORMAL\n");
    return 0;
  } else {
    printf("MUTATED\n");
    return 1;
  }
}

// clang-format off

/**
RUN: %CLANG_EXEC -Xclang -load -Xclang %mull_frontend_cxx -Xclang -add-plugin -Xclang mull-cxx-frontend -Xclang -plugin-arg-mull-cxx-frontend -Xclang mutators=cxx_add_to_sub %s -o %s.exe

RUN: %s.exe | %FILECHECK_EXEC %s --dump-input=fail --strict-whitespace --match-full-lines --check-prefix=STANDALONE_WITHOUT_MUTATION
RUN: (env "cxx_add_to_sub:%s:6:33"=1 %s.exe || true) | %FILECHECK_EXEC %s --dump-input=fail --strict-whitespace --match-full-lines --check-prefix=STANDALONE_WITH_MUTATION

STANDALONE_WITHOUT_MUTATION:NORMAL
STANDALONE_WITH_MUTATION:MUTATED

RUN: %mull_runner %s.exe -ide-reporter-show-killed | %FILECHECK_EXEC %s --dump-input=fail --strict-whitespace --match-full-lines --check-prefix=MULL_RUNNER

MULL_RUNNER:[info] Killed mutants (1/1):
MULL_RUNNER:{{.*}}sample.cpp:6:33: warning: Killed: Replaced + with - [cxx_add_to_sub]
*/
