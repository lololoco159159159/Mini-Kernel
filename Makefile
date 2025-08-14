# Makefile para o Trabalho Prático de Sistemas Operacionais
# Mini-Kernel Multithread com Escalonamento

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread
TARGET = trabSO

# Arquivos fonte
SOURCES = main.c scheduler.c queue.c log.c
HEADERS = structures.h scheduler.h queue.h log.h

# Arquivos objeto
OBJECTS = $(SOURCES:.c=.o)

# Target padrão
all: monoprocessador

# Compilação para sistema monoprocessador
monoprocessador: CFLAGS += -DMONO
monoprocessador: $(TARGET)

# Compilação para sistema multiprocessador
multiprocessador: CFLAGS += -DMULTI
multiprocessador: $(TARGET)

# Compilação do executável
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# Compilação dos arquivos objeto
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Target para debug (com símbolos de debug)
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Target para compilação com otimização
release: CFLAGS += -O2 -DNDEBUG
release: $(TARGET)

# Limpeza dos arquivos gerados
clean:
	rm -f $(OBJECTS) $(TARGET) log_execucao_minikernel.txt

# Limpeza completa (inclui arquivos de backup)
distclean: clean
	rm -f *~ *.bak core

# Target para executar testes com valgrind
valgrind: debug
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET)

# Target para verificar memória com diferentes ferramentas
memcheck: debug
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./$(TARGET)

# Target para análise estática do código
static-analysis:
	cppcheck --enable=all --std=c99 $(SOURCES)

# Target de ajuda
help:
	@echo "Targets disponíveis:"
	@echo "  monoprocessador  - Compila versão para sistema monoprocessador"
	@echo "  multiprocessador - Compila versão para sistema multiprocessador"
	@echo "  debug           - Compila com símbolos de debug"
	@echo "  release         - Compila com otimizações"
	@echo "  clean           - Remove arquivos gerados"
	@echo "  distclean       - Remove todos os arquivos temporários"
	@echo "  valgrind        - Executa com valgrind para verificar memória"
	@echo "  memcheck        - Executa verificação detalhada de memória"
	@echo "  static-analysis - Executa análise estática do código"
	@echo "  help            - Mostra esta ajuda"

# Declara targets que não geram arquivos
.PHONY: all monoprocessador multiprocessador debug release clean distclean valgrind memcheck static-analysis help

# Informações sobre dependências
main.o: main.c structures.h scheduler.h queue.h log.h
scheduler.o: scheduler.c scheduler.h structures.h queue.h log.h
queue.o: queue.c queue.h structures.h
log.o: log.c log.h structures.h
