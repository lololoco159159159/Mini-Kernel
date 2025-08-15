# Mini-Kernel Multithread com Escalonamento de Processos

**Trabalho Prático - Sistemas Operacionais (INF15980)**  
**Universidade Federal do Espírito Santo - Departamento de Informática**

## Descrição do Projeto

Este projeto implementa um sistema de escalonamento de processos multithread em linguagem C, simulando diferentes políticas de escalonamento (FCFS, Round-Robin e Prioridade Preemptiva) em um ambiente concorrente. O sistema utiliza threads POSIX (pthread) para representar e simular a execução de múltiplos fluxos de execução por processo.

## 📋 Status de Desenvolvimento

### ✅ Concluído (Passos 1-5)

- **✅ Passo 1**: Estrutura de projeto e Makefile
- **✅ Passo 2**: Estruturas PCB, TCB e enums  
- **✅ Passo 3**: Fila de prontos (ready queue)
- **✅ Passo 4**: Sistema de log thread-safe
- **✅ Passo 5**: Leitura de entrada e inicialização de PCBs

### 🚧 Em Desenvolvimento (Passos 6-9)

- **🔧 Passo 6**: Thread geradora de processos (versão simplificada implementada)
- **🔧 Passo 7**: Threads de execução de processo  
- **🔧 Passo 8**: Escalonador (FCFS básico funcionando)
- **⏳ Passo 9**: Finalização e cleanup

## 🏗️ Arquitetura do Sistema

### Estruturas Principais

- **PCB (Process Control Block)**: Representa cada processo com PID, duração, prioridade, threads, etc.
- **TCB (Thread Control Block)**: Controla threads individuais dentro de processos
- **ReadyQueue**: Fila de processos prontos para execução (thread-safe)
- **SystemState**: Estado global do sistema incluindo escalonador e logs

### Módulos Implementados

1. **main.c**: ✅ Programa principal, leitura de entrada e coordenação
2. **scheduler.c/h**: ✅ Implementação básica do escalonador FCFS
3. **queue.c/h**: ✅ Gerenciamento completo da fila de processos prontos
4. **log.c/h**: ✅ Sistema de logging thread-safe com buffer global
5. **structures.h**: ✅ Definições completas de estruturas e tipos

## 🔧 Compilação e Execução

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

# Limpar arquivos compilados
make clean
```

### Execução

```bash
# Executar o mini-kernel
./trabSO <arquivo_entrada>

# Exemplo
./trabSO entradas/1.txt
./trabSO test_entrada.txt
```

### Formato do Arquivo de Entrada

```
<numero_de_processos>
<duracao_processo_1>
<prioridade_processo_1>
<num_threads_processo_1>
<tempo_chegada_processo_1>
<duracao_processo_2>
<prioridade_processo_2>
<num_threads_processo_2>
<tempo_chegada_processo_2>
...
<politica_escalonamento>
```

**Políticas de Escalonamento:**
- `1` = FCFS (First Come First Served)
- `2` = Round Robin
- `3` = Prioridade Preemptiva

## 📝 Sistema de Log

O sistema gera automaticamente um arquivo `log_execucao_minikernel.txt` contendo:

- Início da simulação e configurações
- Criação e inicialização de processos
- Eventos de escalonamento
- Execução e finalização de processos
- Estatísticas finais da simulação

**Exemplo de log:**
```
=== INICIO DA SIMULACAO DO MINI-KERNEL ===
Sistema de log inicializado - Buffer: 10000 bytes
Numero de processos a serem criados: 2
PCB criado - PID: 1, Duracao: 1000ms, Prioridade: 1, Threads: 1, Chegada: 0ms
PCB criado - PID: 2, Duracao: 2000ms, Prioridade: 2, Threads: 2, Chegada: 500ms
Politica de escalonamento: FCFS (1)
[FCFS] Executando processo PID 1
[FCFS] Processo PID 1 finalizado
=== FIM DA SIMULACAO ===
```

```

## 🧪 Testes e Validação

### Arquivos de Teste Incluídos

- **entradas/1.txt** a **entradas/7.txt**: Casos de teste pré-definidos
- **test_entrada.txt**: Caso de teste simples para desenvolvimento
- **test_simple.txt**: Teste mínimo com 1 processo

### Testes Unitários

```bash
# Testar apenas o sistema de fila
gcc -o test_queue test_queue.c queue.c structures.h
./test_queue

# Testar apenas a leitura de entrada (Passo 5)
gcc -Wall -Wextra -std=c99 -pthread -DMONO -o test_passo5 test_passo5.c scheduler.c queue.c log.c
./test_passo5 test_entrada.txt
```

### Exemplo de Execução Completa

```bash
$ make monoprocessador
$ ./trabSO test_entrada.txt
$ cat log_execucao_minikernel.txt
```

## 🏭 Implementação Técnica

### Características Implementadas

1. **Thread Safety**: Todas as operações críticas protegidas por mutexes
2. **Gerenciamento de Memória**: Cleanup adequado de recursos
3. **Logging Robusto**: Buffer global thread-safe sem deadlocks
4. **Tratamento de Erros**: Validação completa de entrada
5. **Timeouts de Segurança**: Prevenção de loops infinitos

### Políticas de Escalonamento

#### ✅ FCFS (First Come First Served)
- Implementado e testado
- Processos executam por ordem de chegada
- Simples e funcional

#### 🚧 Round Robin
- Em desenvolvimento
- Quantum configurável
- Preempção por tempo

#### 🚧 Prioridade Preemptiva  
- Em desenvolvimento
- Prioridade 1 = maior, 5 = menor
- Preempção por prioridade

## 📁 Estrutura do Projeto

```
SO/trab/
├── main.c                    # ✅ Programa principal
├── scheduler.c/h             # ✅ Escalonador (FCFS funcionando)
├── queue.c/h                 # ✅ Fila de prontos thread-safe
├── log.c/h                   # ✅ Sistema de log robusto
├── structures.h              # ✅ Estruturas PCB, TCB, etc.
├── Makefile                  # ✅ Build system completo
├── test_queue.c              # ✅ Testes unitários da fila
├── test_passo5.c             # ✅ Teste isolado da leitura
├── test_entrada.txt          # ✅ Arquivo de teste
├── entradas/                 # 📁 Casos de teste
│   ├── 1.txt ... 7.txt      # ✅ Entradas pré-definidas
├── saidas/                   # 📁 Saídas esperadas
│   ├── mono/                 # 📁 Saídas monoprocessador
│   └── multi/                # 📁 Saídas multiprocessador
├── obj/                      # 📁 Arquivos objeto (auto-gerado)
├── passo_a_passo.txt         # 📋 Planejamento do desenvolvimento
├── QUEUE_DOCS.md             # 📚 Documentação da fila
└── README.md                 # 📖 Este arquivo
```

## 🛠️ Próximos Passos de Desenvolvimento

1. **Implementar threads reais de processo** (Passo 7)
2. **Completar Round Robin** (Passo 8)  
3. **Implementar Prioridade Preemptiva** (Passo 8)
4. **Suporte multiprocessador** (Passo 8)
5. **Finalização robusta** (Passo 9)

## 🐛 Problemas Resolvidos

- ✅ **Deadlock no sistema de log**: Corrigido mutex dentro de mutex
- ✅ **Loop infinito nas threads**: Implementada versão simplificada funcional
- ✅ **Timeout no escalonador**: Adicionados limites de segurança
- ✅ **Validação de entrada**: Tratamento robusto de erros

## 👥 Desenvolvimento

**Status**: ✅ Funcional para escalonamento FCFS básico  
**Última atualização**: 14 de agosto de 2025  
**Compilado e testado**: Linux (GCC 9.4.0+)  
**Versão atual**: 1.0.0-beta (Passos 1-5 completos)

### 🎯 Funcionalidades Testadas e Validadas

- ✅ Compilação limpa sem warnings
- ✅ Leitura robusta de arquivos de entrada  
- ✅ Inicialização completa de PCBs e TCBs
- ✅ Sistema de log thread-safe funcionando
- ✅ Fila de prontos com operações concorrentes
- ✅ Escalonamento FCFS básico operacional
- ✅ Cleanup adequado de recursos
- ✅ Geração automática de log detalhado

### � Exemplo de Execução Rápida

```bash
# Clone ou acesse o diretório do projeto
cd SO/trab

# Compile
make monoprocessador

# Execute  
./trabSO test_entrada.txt

# Veja o resultado
cat log_execucao_minikernel.txt
```

**Resultado esperado**: Log completo da simulação em `log_execucao_minikernel.txt` sem erros.

## �📄 Licença

Este é um trabalho acadêmico desenvolvido para a disciplina de Sistemas Operacionais da UFES.

---

**Nota**: Este projeto está em desenvolvimento ativo. Os passos 1-5 estão completamente implementados e testados. Os próximos passos (6-9) estão sendo desenvolvidos incrementalmente.
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
