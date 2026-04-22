---
title: "TP1 — Sistemas Operacionais: Syscall getcnt no xv6-riscv"
author: "Elissandro Caetano Júnior. Henrique Castro Caetano e Vitor Fagundes"
date: "2026"
geometry: margin=2.5cm
fontsize: 11pt
---

# Introdução

Este relatório documenta a implementação da syscall `getcnt` no sistema operacional xv6-riscv. O objetivo é permitir que programas em espaço de usuário consultem quantas vezes cada syscall foi invocada desde o boot do sistema.

A declaração em espaço de usuário é:

```c
int getcnt(int syscall_number);
```

---

# Modificação 1 — Estrutura de dados para contagem (`kernel/syscall.c` e `kernel/syscall.h`)

## Diff

```diff
diff --git a/kernel/syscall.h b/kernel/syscall.h
index 3dd926d..0ba6646 100644
--- a/kernel/syscall.h
+++ b/kernel/syscall.h
@@ -20,3 +20,5 @@
 #define SYS_link   19
 #define SYS_mkdir  20
 #define SYS_close  21
+#define SYS_getcnt 22
+#define NSCALL     23
```

```diff
diff --git a/kernel/syscall.c b/kernel/syscall.c
index 076d965..a024cbd 100644
--- a/kernel/syscall.c
+++ b/kernel/syscall.c
@@ -79,6 +79,9 @@ argstr(int n, char *buf, int max)
   return fetchstr(addr, buf, max);
 }
 
+// Counts how many times each syscall has been invoked (indexed by syscall number).
+uint64 syscall_cnt[NSCALL];
+
```

## Discussão

A estrutura de dados escolhida é um **array global de inteiros de 64 bits** (`uint64 syscall_cnt[NSCALL]`), onde cada posição corresponde ao número de uma syscall. O índice do array é o próprio número da syscall (definido em `syscall.h`), tornando a consulta O(1).

A constante `NSCALL` (valor 23) é definida em `syscall.h` e representa o tamanho necessário para acomodar todas as syscalls existentes (1 a 22). O índice 0 não é usado, pois syscalls são numeradas a partir de 1.

Por ser uma variável global do kernel, o array é inicializado automaticamente com zeros pelo loader ELF e persiste durante toda a execução do sistema, acumulando contagens desde o boot.

---

# Modificação 2 — Atualização da estrutura de dados no dispatcher (`kernel/syscall.c`)

## Diff

```diff
diff --git a/kernel/syscall.c b/kernel/syscall.c
@@ -136,8 +141,7 @@ syscall(void)
 
   num = p->trapframe->a7;
   if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
-    // Use num to lookup the system call function for num, call it,
-    // and store its return value in p->trapframe->a0
+    syscall_cnt[num]++;
     p->trapframe->a0 = syscalls[num]();
   } else {
```

## Discussão

O ponto de atualização do contador é a função `syscall()` em `kernel/syscall.c`, que é o **dispatcher central** de todas as syscalls. Toda vez que um processo faz uma syscall, o processador gera uma trap, que eventualmente chama esta função.

O número da syscall é lido do registrador `a7` do trapframe (`p->trapframe->a7`), seguindo a convenção de chamada RISC-V. Antes de despachar para o handler específico, incrementamos `syscall_cnt[num]++`.

Esta posição garante que **toda syscall válida seja contabilizada exatamente uma vez**, independente de qual handler for chamado. Syscalls inválidas (número fora do intervalo ou sem handler) caem no `else` e não são contadas.

---

# Modificação 3 — Implementação do handler `sys_getcnt` (`kernel/sysproc.c`)

## Diff

```diff
diff --git a/kernel/sysproc.c b/kernel/sysproc.c
index 419e727..062e94b 100644
--- a/kernel/sysproc.c
+++ b/kernel/sysproc.c
@@ -6,6 +6,9 @@
 #include "spinlock.h"
 #include "proc.h"
 #include "vm.h"
+#include "syscall.h"
+
+extern uint64 syscall_cnt[];
 
@@ -95,6 +98,16 @@ sys_kill(void)
   return kkill(pid);
 }
 
+uint64
+sys_getcnt(void)
+{
+  int num;
+  argint(0, &num);
+  if(num <= 0 || num >= NSCALL)
+    return -1;
+  return syscall_cnt[num];
+}
+
```

## Discussão

O handler `sys_getcnt` é implementado em `kernel/sysproc.c`, seguindo o padrão das demais syscalls do xv6. O parâmetro é lido com `argint(0, &num)`, que extrai o primeiro argumento inteiro do trapframe do processo (registrador `a0`).

A validação verifica se o número está no intervalo `[1, NSCALL-1]`, retornando `-1` para entradas inválidas. O `extern uint64 syscall_cnt[]` é necessário para acessar o array definido em `syscall.c` a partir de `sysproc.c`.

O `#include "syscall.h"` foi adicionado para que a constante `NSCALL` esteja disponível no bounds check, evitando o uso de literais mágicos.

---

# Modificação 4 — Interface de espaço de usuário (`user/usys.pl` e `user/user.h`)

## Diff

```diff
diff --git a/user/usys.pl b/user/usys.pl
--- a/user/usys.pl
+++ b/user/usys.pl
@@ -42,3 +42,4 @@ entry("getpid");
 entry("sbrk");
 entry("pause");
 entry("uptime");
+entry("getcnt");
```

```diff
diff --git a/user/user.h b/user/user.h
--- a/user/user.h
+++ b/user/user.h
@@ -24,6 +24,7 @@ int getpid(void);
 char* sys_sbrk(int,int);
 int pause(int);
 int uptime(void);
+int getcnt(int);
```

## Discussão

O arquivo `usys.pl` é um script Perl que gera automaticamente o arquivo assembly `usys.S` durante a compilação. Cada chamada `entry("getcnt")` gera um stub assembly que:

1. Carrega o número da syscall (`SYS_getcnt = 22`) no registrador `a7`
2. Executa a instrução `ecall`, transferindo o controle para o kernel
3. Retorna com o valor que o kernel depositou em `a0`

A declaração em `user/user.h` expõe a função para todos os programas de espaço de usuário que incluírem esse header, permitindo chamá-la como uma função C normal.

---

# Testes

## Programa de teste automatizado (`user/testcnt.c`)

Foi criado o programa `testcnt` para validar o comportamento da syscall:

```c
#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

int main(void) {
  printf("getpid antes: %d\n", getcnt(SYS_getpid));
  getpid(); getpid(); getpid();
  printf("getpid depois: %d\n", getcnt(SYS_getpid));

  printf("write antes: %d\n", getcnt(SYS_write));
  printf("write depois: %d\n", getcnt(SYS_write));

  printf("getcnt invalido (-1): %d\n", getcnt(0));
  printf("getcnt invalido (-1): %d\n", getcnt(99));

  exit(0);
}
```

**Saída obtida:**

```
$ testcnt
getpid antes: 0
getpid depois: 3
write antes: 52
write depois: 68
getcnt invalido (-1): -1
getcnt invalido (-1): -1
```

- A diferença de 3 em `getpid` confirma que as 3 chamadas foram contadas corretamente.
- A diferença em `write` reflete as chamadas internas geradas pelos `printf`.
- Os valores inválidos `0` e `99` retornam `-1` conforme esperado.

## Programa `getcnt` (linha de comando)

Foi criado o programa `getcnt` que recebe o número da syscall como argumento:

```c
int main(int argc, char *argv[]) {
  int num = atoi(argv[1]);
  int cnt = getcnt(num);
  if(cnt < 0) { fprintf(2, "getcnt: invalid syscall number %d\n", num); exit(1); }
  printf("syscall %d has been called %d times\n", num, cnt);
  exit(0);
}
```

**Saída obtida no xv6:**

```
$ ./getcnt 1
syscall 1 has been called 2 times
$ ./getcnt 22
syscall 22 has been called 2 times
$ ./getcnt 22
syscall 22 has been called 3 times
$ ./getcnt 22
syscall 22 has been called 4 times
```

O contador de `getcnt` (syscall 22) incrementa a cada execução, demonstrando que a própria chamada a `getcnt` é contabilizada. O comportamento é consistente com o esperado.
