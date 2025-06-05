#include <stdio.h>      // Biblioteca para mostrar mensagens no terminal
#include <stdlib.h>     // Biblioteca para funções como números aleatórios
#include <pthread.h>    // Permite criar várias tarefas rodando ao mesmo tempo (threads)
#include <unistd.h>     // Permite usar funções como sleep(), que faz o programa esperar
#include <stdbool.h>    // Permite usar o tipo "bool", que representa verdadeiro/falso
#include <string.h>     // Permite manipular textos

#define MAX_ANDARES 11          // Número total de andares no prédio (de 0 a 10)
#define NUM_ELEVADORES 3        // Número de elevadores disponíveis

// Estrutura que representa um elevador
typedef struct {
    int id;                     // Identificador do elevador (1, 2 ou 3)
    int current_floor;          // Em qual andar o elevador está agora
    int pedidos_subida[MAX_ANDARES];   // Lista de andares para onde o elevador deve subir
    int pedidos_descida[MAX_ANDARES];  // Lista de andares para onde o elevador deve descer
    int total_pedidos_subida;   // Total de pedidos de subida
    int total_pedidos_descida;  // Total de pedidos de descida
    bool ativo;                 // Diz se o elevador está ativo ou não
    pthread_mutex_t mutex;     // Trava para evitar que dois processos usem o elevador ao mesmo tempo
    pthread_cond_t cond;       // Condição para o elevador saber quando deve agir
} Elevador;

Elevador elevadores[NUM_ELEVADORES];  // Vetor com os três elevadores
bool running = true;                  // Controla se o sistema está em execução

// Função para ordenar pedidos de andares (ordem crescente ou decrescente)
void ordenar(int *arr, int n, bool crescente) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if ((crescente && arr[j] > arr[j + 1]) || (!crescente && arr[j] < arr[j + 1])) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

// Verifica se um pedido já existe para não repetir
bool pedido_existe(int *pedidos, int total, int andar) {
    for (int i = 0; i < total; i++) {
        if (pedidos[i] == andar) {
            return true;
        }
    }
    return false;
}

// Adiciona um novo pedido ao elevador (subida ou descida)
void adicionar_pedido(Elevador *e, int andar) {
    pthread_mutex_lock(&e->mutex); // Trava o elevador para alterações seguras

    if (andar > e->current_floor) {
        if (!pedido_existe(e->pedidos_subida, e->total_pedidos_subida, andar)) {
            e->pedidos_subida[e->total_pedidos_subida++] = andar;
            ordenar(e->pedidos_subida, e->total_pedidos_subida, true); // Organiza em ordem crescente
        }
    } else if (andar < e->current_floor) {
        if (!pedido_existe(e->pedidos_descida, e->total_pedidos_descida, andar)) {
            e->pedidos_descida[e->total_pedidos_descida++] = andar;
            ordenar(e->pedidos_descida, e->total_pedidos_descida, false); // Organiza em ordem decrescente
        }
    }

    pthread_cond_signal(&e->cond); // Avisa o elevador que há novos pedidos
    pthread_mutex_unlock(&e->mutex); // Libera o elevador
}

// Mostra o status de um elevador no terminal
void imprimir_status(Elevador *e) {
    pthread_mutex_lock(&e->mutex);
    printf("\nElevador %d no andar %d\n", e->id, e->current_floor);
    printf("Pedidos de subida (%d): ", e->total_pedidos_subida);
    for (int i = 0; i < e->total_pedidos_subida; i++) {
        printf("%d ", e->pedidos_subida[i]);
    }
    printf("\nPedidos de descida (%d): ", e->total_pedidos_descida);
    for (int i = 0; i < e->total_pedidos_descida; i++) {
        printf("%d ", e->pedidos_descida[i]);
    }
    printf("\n");
    pthread_mutex_unlock(&e->mutex);
}

// Simula a parada no andar e permite novos pedidos
void abrir_porta(Elevador *e) {
    printf("Elevador %d abrindo portas no andar %d\n", e->id, e->current_floor);
    sleep(1); // Espera 1 segundo para simular parada

    int novo_pedido;
    printf("Digite o andar desejado (ou -1 para nenhum): ");
    scanf("%d", &novo_pedido);

    if (novo_pedido >= 0 && novo_pedido < MAX_ANDARES) {
        adicionar_pedido(e, novo_pedido); // Adiciona novo pedido
    }

    printf("Elevador %d fechando portas\n", e->id);
}

// Função que representa o comportamento de um elevador (roda em paralelo)
void* elevador_func(void* arg) {
    Elevador *e = (Elevador *)arg;

    while (running) {
        pthread_mutex_lock(&e->mutex);

        // Espera até ter pedidos
        while (e->total_pedidos_subida == 0 && e->total_pedidos_descida == 0 && running) {
            pthread_cond_wait(&e->cond, &e->mutex);
        }

        // Processa pedidos de subida
        while (e->total_pedidos_subida > 0) {
            if (e->current_floor < e->pedidos_subida[0]) {
                e->current_floor++;
                printf("Elevador %d subindo para o andar %d\n", e->id, e->current_floor);
                sleep(1);
            }

            if (e->current_floor == e->pedidos_subida[0]) {
                abrir_porta(e);
                for (int i = 1; i < e->total_pedidos_subida; i++) {
                    e->pedidos_subida[i - 1] = e->pedidos_subida[i];
                }
                e->total_pedidos_subida--;
            }
        }

        // Processa pedidos de descida
        while (e->total_pedidos_descida > 0) {
            if (e->current_floor > e->pedidos_descida[0]) {
                e->current_floor--;
                printf("Elevador %d descendo para o andar %d\n", e->id, e->current_floor);
                sleep(1);
            }

            if (e->current_floor == e->pedidos_descida[0]) {
                abrir_porta(e);
                for (int i = 1; i < e->total_pedidos_descida; i++) {
                    e->pedidos_descida[i - 1] = e->pedidos_descida[i];
                }
                e->total_pedidos_descida--;
            }
        }

        pthread_mutex_unlock(&e->mutex);
    }

    return NULL;
}

// Escolhe o elevador mais próximo do andar solicitado
int escolher_elevador(int andar) {
    int melhor_elevador = 0;
    int menor_distancia = abs(elevadores[0].current_floor - andar);

    for (int i = 1; i < NUM_ELEVADORES; i++) {
        int distancia = abs(elevadores[i].current_floor - andar);
        if (distancia < menor_distancia) {
            melhor_elevador = i;
            menor_distancia = distancia;
        }
    }

    return melhor_elevador;
}

// Thread que recebe comandos do usuário
void* terminal_func(void* arg) {
    while (running) {
        int andar;
        char comando[10];

        printf("\nDigite o andar desejado (0 a 10), 'status' ou -1 para sair: ");
        scanf("%s", comando);

        if (strcmp(comando, "status") == 0) {
            for (int i = 0; i < NUM_ELEVADORES; i++) {
                imprimir_status(&elevadores[i]);
            }
        } else {
            andar = atoi(comando);
            if (andar == -1) {
                running = false;
                for (int i = 0; i < NUM_ELEVADORES; i++) {
                    pthread_cond_signal(&elevadores[i].cond);
                }
                break;
            } else if (andar >= 0 && andar < MAX_ANDARES) {
                int elevador_id = escolher_elevador(andar);
                adicionar_pedido(&elevadores[elevador_id], andar);
            } else {
                printf("Andar inválido!\n");
            }
        }
    }

    return NULL;
}

// Função principal (ponto de entrada do programa)
int main() {
    pthread_t threads[NUM_ELEVADORES];
    pthread_t terminal;

    // Inicializa cada elevador
    for (int i = 0; i < NUM_ELEVADORES; i++) {
        elevadores[i].id = i + 1;
        elevadores[i].current_floor = 0;
        elevadores[i].total_pedidos_subida = 0;
        elevadores[i].total_pedidos_descida = 0;
        pthread_mutex_init(&elevadores[i].mutex, NULL);
        pthread_cond_init(&elevadores[i].cond, NULL);
        pthread_create(&threads[i], NULL, elevador_func, &elevadores[i]); // Cria a thread
    }

    // Cria a thread do terminal
    pthread_create(&terminal, NULL, terminal_func, NULL);

    // Espera o terminal encerrar
    pthread_join(terminal, NULL);

    // Espera todos os elevadores encerrarem
    for (int i = 0; i < NUM_ELEVADORES; i++) {
        pthread_join(threads[i], NULL);
        pthread_mutex_destroy(&elevadores[i].mutex);
        pthread_cond_destroy(&elevadores[i].cond);
    }

    printf("Sistema encerrado.\n");
    return 0;
}
