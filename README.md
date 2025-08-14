# Mini-Kernel Multithread com Escalonamento de Processos

**Trabalho Prático - Sistemas Operacionais (INF15980)**  
**Universidade Federal do Espírito Santo - Departamento de Informática**

## Descrição do Projeto

Este projeto implementa um sistema de escalonamento de processos multithread em linguagem C, simulando diferentes políticas de escalonamento (FCFS, Round-Robin e Prioridade Preemptiva) em um ambiente concorrente. O sistema utiliza threads POSIX (pthread) para representar e simular a execução de múltiplos fluxos de execução por processo.

## Objetivos

- Implementar escalonamento e sincronização de processos e threads
- Controlar concorrência com mutexes e variáveis de condição
- Simular estruturas de controle de processos (BCP) e threads (TCB)
- Implementar políticas de escalonamento reais: FCFS, RR e Prioridade
- Suportar execução em sistemas mono e multiprocessador

## Arquitetura do Sistema

### Estruturas Principais

- **PCB (Process Control Block)**: Representa cada processo com PID, duração, prioridade, threads, etc.
- **TCB (Thread Control Block)**: Controla threads individuais dentro de processos
- **ReadyQueue**: Fila de processos prontos para execução
- **SystemState**: Estado global do sistema incluindo escalonador e logs

### Módulos

1. **main.c**: Programa principal, leitura de entrada e coordenação
2. **scheduler.c/h**: Implementação das políticas de escalonamento
3. **queue.c/h**: Gerenciamento da fila de processos prontos
4. **log.c/h**: Sistema de logging thread-safe
5. **structures.h**: Definições de estruturas e tipos

## Compilação e Execução

### Pré-requisitos

- GCC com suporte a C99
- Biblioteca pthread
- Sistema Linux/Unix

### Compilação

```bash
# Para sistema monoprocessador
make monoprocessador

# Para sistema multiprocessador
make multiprocessador

# Compilação com debug
make debug

# Limpeza
make clean
```

### Execução

```bash
./trabSO <arquivo_entrada>
```

### Exemplo de Uso

```bash
# Compilar versão monoprocessador
make monoprocessador

# Executar com arquivo de entrada
./trabSO entradas/1.txt

# Log será salvo em: log_execucao_minikernel.txt
```

## Formato de Entrada

```
3                    # Número de processos
1000 3 2 0          # Processo 1: duração=1000ms, prioridade=3, threads=2, chegada=0ms
2000 1 1 200        # Processo 2: duração=2000ms, prioridade=1, threads=1, chegada=200ms
1500 2 1 100        # Processo 3: duração=1500ms, prioridade=2, threads=1, chegada=100ms
1                    # Política: 1=FCFS, 2=RR, 3=Prioridade
```

## Saída Esperada

O sistema gera um arquivo `log_execucao_minikernel.txt` com eventos de execução:

```
[FCFS] Executando processo PID 1
[FCFS] Processo PID 1 finalizado
[FCFS] Executando processo PID 3
[FCFS] Processo PID 3 finalizado
[FCFS] Executando processo PID 2
[FCFS] Processo PID 2 finalizado
Escalonador terminou execução de todos processos
```

## 🔧 Estado Atual do Desenvolvimento

### ✅ Implementado

- [x] Estrutura completa do projeto
- [x] Makefile com targets para mono/multiprocessador
- [x] Definições de estruturas (PCB, TCB, ReadyQueue)
- [x] Sistema de fila de processos prontos
- [x] Sistema de logging thread-safe
- [x] Esqueleto do escalonador
- [x] Leitura de arquivo de entrada
- [x] Compilação sem warnings

### 🚧 Em Desenvolvimento

- [ ] Lógica completa dos algoritmos de escalonamento
- [ ] Sincronização entre threads de processo
- [ ] Simulação de tempo de execução
- [ ] Implementação específica para multiprocessador
- [ ] Testes com casos de entrada

### 📋 Próximos Passos

1. **Implementar lógica de escalonamento detalhada**
   - Completar FCFS, Round-Robin e Prioridade
   - Adicionar preempção correta
   
2. **Implementar execução de threads**
   - Sincronização com variáveis de condição
   - Simulação de tempo de execução
   
3. **Testar com casos de entrada**
   - Verificar saída conforme especificação
   - Corrigir bugs e ajustar timing

4. **Implementar versão multiprocessador**
   - Duas CPUs independentes
   - Balanceamento de carga

## 🛠️ Ferramentas de Desenvolvimento

```bash
# Verificação de memória
make valgrind

# Análise estática
make static-analysis

# Compilação com debug
make debug

# Ajuda completa
make help
```

## Políticas de Escalonamento

### 1. FCFS (First Come First Served)
- Execução em ordem de chegada
- Não preemptivo
- Processo executa até completar

### 2. Round-Robin (RR)
- Quantum fixo de 500ms
- Preempção por tempo
- Processo volta ao final da fila

### 3. Prioridade Preemptiva
- Menor número = maior prioridade
- Preempção por prioridade
- Processos de maior prioridade interrompem execução

## Estrutura de Arquivos

```
.
├── README.md                          # Este arquivo
├── Makefile                          # Sistema de compilação
├── main.c                            # Programa principal
├── scheduler.c/h                     # Escalonador
├── queue.c/h                         # Fila de processos
├── log.c/h                           # Sistema de log
├── structures.h                      # Estruturas principais
├── entradas/                         # Casos de teste
│   ├── 1.txt
│   ├── 2.txt
│   └── ...
└── saidas/                           # Saídas esperadas
    ├── mono/
    └── multi/
```

## Notas de Implementação

### Sincronização
- Uso de mutexes para proteção de recursos compartilhados
- Variáveis de condição para coordenação entre threads
- Evita deadlocks e condições de corrida

### Gerenciamento de Memória
- Alocação dinâmica para estruturas
- Limpeza adequada de recursos
- Verificação com valgrind

### Portabilidade
- Código compatível com sistemas Linux/Unix
- Uso de padrões POSIX
- Compilação com flags rigorosas

## Debugging e Testes

Para depuração, compile com:
```bash
make debug
valgrind --leak-check=full ./trabSO entrada.txt
```

## Licença

Este projeto é desenvolvido para fins acadêmicos como parte do curso de Sistemas Operacionais da UFES.

---

**Data de Última Atualização**: 13 de agosto de 2025  
**Status**: Em desenvolvimento - Estrutura base completa
