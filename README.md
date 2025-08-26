# Mini-Kernel - Sistema de Escalonamento de Processos

**Trabalho Prático - Sistemas Operacionais (INF15980)**  
**Universidade Federal do Espírito Santo**

## Resumo

Mini-kernel multithread em C que simula escalonamento de processos com três políticas: FCFS, Round-Robin e Prioridade Preemptiva. Implementa duas versões (mono e multiprocessador) usando threads POSIX para simular execução concorrente de processos com múltiplas threads.

## Características Principais

- **Estruturas**: PCB (Process Control Block) e TCB (Thread Control Block)
- **Sincronização**: Mutexes e variáveis de condição para controle de concorrência
- **Simulação**: Threads executam em blocos de 500ms simulando tempo real
- **Log**: Registro detalhado de eventos de escalonamento em arquivo
- **Fila de Prontos**: Gerenciamento thread-safe da ready queue

## Entrada

```
<num_processos>
<duracao> <prioridade> <num_threads> <tempo_chegada>
...
<politica>  // 1=FCFS, 2=RR, 3=Prioridade
```

## Saída

Arquivo `log_execucao_minikernel.txt` com eventos de execução:
```
[FCFS] Executando processo PID 1
[FCFS] Processo PID 1 finalizado
Escalonador terminou execução de todos processos
```

## Compilação

```bash
make monoprocessador   # Versão com 1 CPU
make multiprocessador  # Versão com 2 CPUs
```

## Execução

```bash
./trabSO arquivo_entrada.txt
```

## Algoritmos Implementados

1. **FCFS**: Execução por ordem de chegada (não-preemptivo)
2. **Round Robin**: Quantum de 500ms com preempção por tempo
3. **Prioridade**: Preempção baseada em prioridade (1=maior, 5=menor)

## Arquitetura

- **src/main.c**: Coordenação geral e criação de threads
- **src/scheduler.c**: Implementação dos algoritmos de escalonamento
- **src/queue.c**: Fila de processos prontos thread-safe
- **src/log.c**: Sistema de logging
- **lib/structures.h**: Definições de PCB, TCB e estados
- **lib/scheduler.h**: Interface do escalonador
- **lib/queue.h**: Interface da fila de processos
- **lib/log.h**: Interface do sistema de logging

## Estrutura do Projeto

```
Mini-Kernel/
├── Makefile                          # Sistema de compilação
├── README.md                         # Esta documentação
├── Especificação - TP SO - 2025_1.pdf # Especificação do trabalho
├── src/                              # Arquivos fonte (.c)
│   ├── main.c                        # Programa principal
│   ├── scheduler.c                   # Algoritmos de escalonamento
│   ├── queue.c                       # Fila de processos prontos
│   └── log.c                         # Sistema de logging
├── lib/                              # Headers (.h)
│   ├── structures.h                  # Estruturas PCB, TCB, SystemState
│   ├── scheduler.h                   # Interface do escalonador
│   ├── queue.h                       # Interface da fila
│   └── log.h                         # Interface de logging
├── obj/                              # Arquivos objeto (gerados)
├── casos_teste_v4/                   # Casos de teste oficiais
│   ├── entradas/                     # Arquivos de entrada
│   └── saidas/                       # Saídas esperadas
└── log_execucao_minikernel.txt       # Log de execução (gerado)
```

## Entrega

O projeto foi empacotado no arquivo `2022100892,2022101398.tar.gz` seguindo o padrão especificado:

```bash
# Extração e compilação (monoprocessador)
tar -xzvf 2022100892,2022101398.tar.gz
make monoprocessador
./trabSO entrada.txt

# Extração e compilação (multiprocessador)
tar -xzvf 2022100892,2022101398.tar.gz
make multiprocessador
./trabSO entrada.txt
```
