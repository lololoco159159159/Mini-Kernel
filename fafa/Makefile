# Makefile para o Mini-Kernel
CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -pthread -Iinclude
TARGET = trabSO

# Diretórios
SRCDIR = src
INCDIR = include
OBJDIR = obj

# Arquivos fonte modulares
SOURCES = $(SRCDIR)/main.c $(SRCDIR)/pcb.c $(SRCDIR)/tcb.c $(SRCDIR)/ready_queue.c $(SRCDIR)/scheduler.c $(SRCDIR)/logger.c $(SRCDIR)/process_manager.c
OBJECTS = $(OBJDIR)/main.o $(OBJDIR)/pcb.o $(OBJDIR)/tcb.o $(OBJDIR)/ready_queue.o $(OBJDIR)/scheduler.o $(OBJDIR)/logger.o $(OBJDIR)/process_manager.o

# Regra padrão
all: monoprocessador

# Versão monoprocessador
monoprocessador: clean-obj $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# Versão multiprocessador
multiprocessador: obj clean-obj
	$(CC) $(CFLAGS) -DMULTIPROCESSADOR -c $(SRCDIR)/main.c -o $(OBJDIR)/main.o
	$(CC) $(CFLAGS) -DMULTIPROCESSADOR -c $(SRCDIR)/pcb.c -o $(OBJDIR)/pcb.o
	$(CC) $(CFLAGS) -DMULTIPROCESSADOR -c $(SRCDIR)/tcb.c -o $(OBJDIR)/tcb.o
	$(CC) $(CFLAGS) -DMULTIPROCESSADOR -c $(SRCDIR)/ready_queue.c -o $(OBJDIR)/ready_queue.o
	$(CC) $(CFLAGS) -DMULTIPROCESSADOR -c $(SRCDIR)/scheduler.c -o $(OBJDIR)/scheduler.o
	$(CC) $(CFLAGS) -DMULTIPROCESSADOR -c $(SRCDIR)/logger.c -o $(OBJDIR)/logger.o
	$(CC) $(CFLAGS) -DMULTIPROCESSADOR -c $(SRCDIR)/process_manager.c -o $(OBJDIR)/process_manager.o
	$(CC) $(CFLAGS) -DMULTIPROCESSADOR -o $(TARGET) $(OBJECTS)

# Compilação de arquivos objeto
$(OBJDIR)/%.o: $(SRCDIR)/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

# Garante que o diretório obj existe
obj:
	mkdir -p $(OBJDIR)

# Limpeza apenas dos objetos
clean-obj:
	rm -f $(OBJECTS)

# Limpeza
clean:
	rm -f $(OBJECTS) $(TARGET) log_execucao_minikernel.txt

# Teste com um arquivo de entrada
test: monoprocessador
	./$(TARGET) entradas/1.txt

# Teste multiprocessador
test-multi: multiprocessador
	./$(TARGET) entradas/1.txt

# Verificação de vazamento de memória
valgrind: monoprocessador
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET) entradas/1.txt

# Dependências dos headers
$(OBJDIR)/main.o: $(SRCDIR)/main.c $(INCDIR)/scheduler.h $(INCDIR)/process_manager.h $(INCDIR)/logger.h
$(OBJDIR)/pcb.o: $(SRCDIR)/pcb.c $(INCDIR)/pcb.h
$(OBJDIR)/tcb.o: $(SRCDIR)/tcb.c $(INCDIR)/tcb.h $(INCDIR)/pcb.h
$(OBJDIR)/ready_queue.o: $(SRCDIR)/ready_queue.c $(INCDIR)/ready_queue.h $(INCDIR)/pcb.h
$(OBJDIR)/scheduler.o: $(SRCDIR)/scheduler.c $(INCDIR)/scheduler.h $(INCDIR)/pcb.h $(INCDIR)/ready_queue.h $(INCDIR)/logger.h $(INCDIR)/process_manager.h
$(OBJDIR)/logger.o: $(SRCDIR)/logger.c $(INCDIR)/logger.h
$(OBJDIR)/process_manager.o: $(SRCDIR)/process_manager.c $(INCDIR)/process_manager.h $(INCDIR)/pcb.h $(INCDIR)/tcb.h $(INCDIR)/scheduler.h $(INCDIR)/logger.h

.PHONY: all monoprocessador multiprocessador clean clean-obj test test-multi valgrind
