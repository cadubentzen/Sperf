#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <elf.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/time.h>


#define RED                "\x1b[31m"
#define GREEN              "\x1b[32m"
#define CLEAR              "\x1b[0m"
#define YELLOW             "\x1b[33m"

#define BUF_LENGTH          256

void* mapMemory(char* path)
{
  struct stat file_info;
  int fd;
  void* mem_map;

  printf(YELLOW "Mapping memory...\n" CLEAR);

  fd = open(path, O_RDONLY);
  if(fd == -1) {
    fprintf(stderr, RED "Failed to open: %s\n" CLEAR, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if(fstat(fd, &file_info) == -1) {
    fprintf(stderr, RED "Failed to get file information: %s\n" CLEAR, strerror(errno));
    exit(EXIT_FAILURE);
  }

  mem_map = mmap(NULL, file_info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if(mem_map == MAP_FAILED) {
    fprintf(stderr, RED "Failed to map memmory: %s\n" CLEAR, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if(close(fd) == -1) {
    fprintf(stderr, RED "Failed to close: %s\n" CLEAR, strerror(errno));
    exit(EXIT_FAILURE);
  }

  return mem_map;
};




Elf64_Addr lookupSymbol(Elf64_Ehdr* elf_hdr, Elf64_Shdr* section_hdr, char* symbol, uint8_t* file_begin)
{
  int count;
  Elf64_Sym* dynsym_table;
  Elf64_Rela* rela_table;
  char* dynstr_table;
  char* shstr_table;
  int shstrtab_index, shname_index, dynsym_index, dynstr_index;

  /* section_hdr[count] = beginning of section header
   * section_hdr[count].sh_offset = beginning of section
   * sh_link when sh_type = SHT_RELA means that ...
   */
  for(count = 0; count < elf_hdr->e_shnum; count++) {
    // SHT_RELA -> or rela.dym, ou rela.plt
    if(section_hdr[count].sh_type == SHT_RELA) {
      // Índice que localiza a tabela de strings associada ao cabeçalho de seções
      shstrtab_index = elf_hdr->e_shstrndx;
      // Posição na memória da tabela de strings do cabeçalho de seções
      shstr_table = (char*) (file_begin + section_hdr[shstrtab_index].sh_offset);
      // Índice para a string que contém o nome da seção dentro da tabela de strings
      shname_index = section_hdr[count].sh_name;

      // Se a seção é a rela.plt, podemos procurar pelo símbolo
      if(strcmp(&shstr_table[shname_index], ".rela.plt") != 0)
        continue;

      // Posição na memória da tabela de realocações
      rela_table = (Elf64_Rela*) (file_begin + section_hdr[count].sh_offset);
      // Índice da tabela de símbolos dinâmicos associada a esta tabela de realocações = dynsym
      dynsym_index = section_hdr[count].sh_link;
      // Posição na memória da tabela de símbolos dinâmicos
      dynsym_table = (Elf64_Sym*) (file_begin + section_hdr[dynsym_index].sh_offset);

      // Índice da tabela de strings associada à tabela de símbolos dinâmicos = dynstr
      dynstr_index = section_hdr[dynsym_index].sh_link;
      // Posição na memória desta tabela de strings
      dynstr_table = (char*) (file_begin + section_hdr[dynstr_index].sh_offset);

      int j;
      // Para j passando por toda dynsym
      for(j = 0; j < section_hdr[dynsym_index].sh_size/sizeof(Elf64_Sym); j++) {
        // Se houver índice em dynsym associado às entradas em rela.plt
        if(ELF64_R_SYM(rela_table->r_info) == j) {
          // Se o nome deste símbolo for o que estamos procurando
          // Retornar a posição na memória da entrada correspondente na tabela de realocação
          if(strcmp(&dynstr_table[dynsym_table->st_name], symbol) == 0)
            return rela_table->r_offset;
          rela_table++;
        }
        dynsym_table++;
      }
    }
  }
  return 0;
};




Elf64_Addr checkBinary(uint8_t* map, char* symbol)
{
  Elf64_Ehdr* elf_hdr;
  Elf64_Shdr* section_hdr;

  printf(YELLOW "Checking binary file...\n" CLEAR);

  if(map[0] != 0x7f || strncmp((char*) map+1, "ELF", 3)) {
    printf(RED "Error: " CLEAR "the file specified is not a ELF binary.\n");
    exit(EXIT_FAILURE);
  }

  elf_hdr = (Elf64_Ehdr*) map;
  if(elf_hdr->e_type != ET_EXEC) {
    printf(RED "Error: " CLEAR "the file specified is not a ELF executable.\n");
    exit(EXIT_FAILURE);
  }
  if(elf_hdr->e_shoff == 0 || elf_hdr->e_shnum == 0 || elf_hdr->e_shstrndx == SHN_UNDEF) {
    printf(RED "Error: " CLEAR "section header table or section name string table were not found.\n");
    exit(EXIT_FAILURE);
  }
  else
    section_hdr = (Elf64_Shdr*)(map + elf_hdr->e_shoff);

  return lookupSymbol(elf_hdr, section_hdr, symbol, map);
};




void adjustRegisters(pid_t pid, Elf64_Addr sym_addr, long orig_data, long trap)
{
  struct user_regs_struct registers;

  if(ptrace(PTRACE_GETREGS, pid, NULL, &registers) == -1) {
    fprintf(stderr, RED "Failed to get registers information: %s\n" CLEAR, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if(ptrace(PTRACE_POKEDATA, pid, sym_addr, orig_data) == -1) {
    fprintf(stderr, RED "Failed to poke data: %s\n" CLEAR, strerror(errno));
    exit(EXIT_FAILURE);
  }

  registers.rip = registers.rip - 1;
  if(ptrace(PTRACE_SETREGS, pid, NULL, &registers) == -1) {
    fprintf(stderr, RED "Failed to get registers information: %s\n" CLEAR, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if(ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL) == -1) {
    fprintf(stderr, RED "Failed to continue the tracing: %s\n" CLEAR, strerror(errno));
    exit(EXIT_FAILURE);
  }
  wait(NULL);

  if(ptrace(PTRACE_POKEDATA, pid, sym_addr, trap) == -1) {
    fprintf(stderr, RED "Failed to poke data: %s\n" CLEAR, strerror(errno));
    exit(EXIT_FAILURE);
  }

};




int main(int argc, char** argv)
{
  int status;
  /* The 'trap' is the opcode '0xcc', which means that, at that address, the execution must stop */
  long orig_data, plt_address, trap;
  pid_t pid_child;
  Elf64_Addr sym_addr;
  void* mapped_mem;
  struct timeval tv;
  double before, after;

  if(argc < 3) {
    printf("Usage: memlay <breakpoint> <program>\n");
    exit(EXIT_SUCCESS);
  }

  char* sym_name = (char*) malloc(strlen(argv[1]));
  char* prog_path = (char*) malloc(strlen(argv[2]));

  strcpy(prog_path, argv[2]);
  mapped_mem = mapMemory(prog_path);

  strcpy(sym_name, argv[1]);
  sym_addr = checkBinary((uint8_t*) mapped_mem, sym_name);
  if(sym_addr == 0) {
    fprintf(stderr, RED "Error: " CLEAR "symbol not found\n");
    exit(EXIT_FAILURE);
  }
  else
    printf(GREEN "Symbol found\n" CLEAR);
    
  printf(YELLOW "Breakpoint at %lx\n" CLEAR, sym_addr);

  pid_child = fork();
  if(pid_child == -1) {
    fprintf(stderr, RED "Failed to fork: %s\n" CLEAR, strerror(errno));
    exit(EXIT_FAILURE);
  }
  if (pid_child == 0) {
    if(ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
      fprintf(stderr, RED "Failed to start tracing: %s\n" CLEAR, strerror(errno));
      exit(EXIT_FAILURE);
    }
    if(execv(prog_path, argv + 2) == -1) {
      fprintf(stderr, RED "Failed to start the tracee: %s\n" CLEAR, strerror(errno));
      exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
  }

  wait(&status);
  printf(YELLOW "Beginning of the analysis of process %d (%s)\n" CLEAR, pid_child, argv[2]);

  // O endereço do símbolo dado por "lookup_symbol" é um endereço na GOT
  // Não podemos colocar o trap na GOT, pois ela está no segmento de dados
  // Porém, os bytes em GOT correspondem aos endereços na PLT, que, por sua vez, estão no segmento de texto
  errno = 0;
  plt_address = ptrace(PTRACE_PEEKDATA, pid_child, sym_addr, NULL);
  orig_data = ptrace(PTRACE_PEEKDATA, pid_child, plt_address, NULL);
  if(orig_data == -1) {
    if(errno != 0) {
      fprintf(stderr, RED "Failed to peek data: %s\n" CLEAR, strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  trap = (orig_data & ~0xff) | 0xcc;
  if(ptrace(PTRACE_POKEDATA, pid_child, plt_address, trap) == -1) {
    fprintf(stderr, RED "Failed to poke data: %s\n" CLEAR, strerror(errno));
    exit(EXIT_FAILURE);
  }

  while(1) {
    if(ptrace(PTRACE_CONT, pid_child, NULL, NULL) == -1) {
      fprintf(stderr, RED "Failed to continue the tracing: %s\n" CLEAR, strerror(errno));
      exit(EXIT_FAILURE);
    }
    wait(&status);

    if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
      printf(YELLOW "Hit the breakpoint %lx" CLEAR "\n", sym_addr);

      gettimeofday(&tv, NULL);
      before = (double) tv.tv_sec + (double) tv.tv_usec/1000000.0;

      adjustRegisters(pid_child, plt_address, orig_data, trap);
    }

    if(WIFEXITED(status)) {
      gettimeofday(&tv, NULL);
      after = (double) tv.tv_sec + (double) tv.tv_usec/1000000.0;
      printf("Time elapsed = %.4f\n", after - before);

      printf(YELLOW "\nCompleted tracing pid %d\n" CLEAR, pid_child);
      break;
    }
  }

  return 0;
}
