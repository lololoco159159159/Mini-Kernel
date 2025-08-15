# Makefile para o Trabalho Prático de Sistemas Operacionais
# Mini-Kernel Multithread com Escalonamento

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread
TARGET = trabSO

# Diretórios
OBJDIR = obj
SRCDIR = .

# Arquivos fonte
SOURCES = main.c scheduler.c queue.c log.c
HEADERS = structures.h scheduler.h queue.h log.h

# Arquivos objeto (na pasta obj/)
OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)

# Target padrão
all: monoprocessador

# Compilação para sistema monoprocessador
monoprocessador: CFLAGS += -DMONO
monoprocessador: $(TARGET)

# Compilação para sistema multiprocessador
multiprocessador: CFLAGS += -DMULTI
multiprocessador: $(TARGET)

# Compilação do executável
$(TARGET): $(OBJDIR) $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# Cria o diretório obj se não existir
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compilação dos arquivos objeto
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Target para debug (com símbolos de debug)
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Target para compilação com otimização
release: CFLAGS += -O2 -DNDEBUG
release: $(TARGET)

# Limpeza dos arquivos gerados
clean:
	rm -rf $(OBJDIR) $(TARGET) log_execucao_minikernel.txt test_queue
	rm demo_log

# Limpeza completa (inclui arquivos de backup)
distclean: clean
	rm -f *~ *.bak core

# Target para executar testes com valgrind
valgrind: debug
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET)

# Target para verificar memória com diferentes ferramentas
memcheck: debug
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./$(TARGET)

# Target para testar a fila de processos
test-queue: CFLAGS += -g -DDEBUG
test-queue: $(OBJDIR) $(OBJDIR)/queue.o
	$(CC) $(CFLAGS) -o test_queue test_queue.c $(OBJDIR)/queue.o
	./test_queue

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
	@echo "  test-queue      - Executa testes da fila de processos"
	@echo "  clean           - Remove pasta obj/ e arquivos gerados"
	@echo "  distclean       - Remove todos os arquivos temporários"
	@echo "  valgrind        - Executa com valgrind para verificar memória"
	@echo "  memcheck        - Executa verificação detalhada de memória"
	@echo "  static-analysis - Executa análise estática do código"
	@echo "  rebuild         - Limpa e recompila completamente"
	@echo "  help            - Mostra esta ajuda"
	@echo ""
	@echo "Estrutura de compilação:"
	@echo "  Arquivos .o são armazenados em: $(OBJDIR)/"
	@echo "  Executável gerado: $(TARGET)"

# Target para rebuild completo
rebuild: clean monoprocessador

# Target para mostrar estrutura do projeto
show-structure:
	@echo "=== ESTRUTURA DO PROJETO MINI-KERNEL ==="
	@echo "Arquivos fonte:"
	@for file in $(SOURCES); do echo "  $$file"; done
	@echo "Arquivos header:"
	@for file in $(HEADERS); do echo "  $$file"; done
	@echo "Diretório de objetos: $(OBJDIR)/"
	@echo "Executável: $(TARGET)"
	@if [ -d "$(OBJDIR)" ]; then \
		echo "Arquivos objeto compilados:"; \
		ls -la $(OBJDIR)/; \
	else \
		echo "Nenhum arquivo objeto compilado (execute make monoprocessador)"; \
	fi

# Declara targets que não geram arquivos
.PHONY: all monoprocessador multiprocessador debug release test-queue clean distclean valgrind memcheck static-analysis help

# Informações sobre dependências
main.o: main.c structures.h scheduler.h queue.h log.h
scheduler.o: scheduler.c scheduler.h structures.h queue.h log.h
queue.o: queue.c queue.h structures.h
log.o: log.c log.h structures.h
