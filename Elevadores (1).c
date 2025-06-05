#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#define MAX_ANDARES 10             // Número máximo de andares do prédio
#define MAX_PEDIDOS 100            // Quantidade máxima de pedidos por elevador
#define NUM_ELEVADORES 3           // Número total de elevadores

// Estrutura que representa um elevador
typedef struct {
    int id;                                 // Identificador do elevador
    int current_floor;                      // Andar atual
    int pedidos_subida[MAX_PEDIDOS];       // Lista de andares para onde deve subir
    int total_pedidos_subida;              // Total de pedidos de subida
    int pedidos_descida[MAX_PEDIDOS];      // Lista de andares para onde deve descer
    int total_pedidos_descida;             // Total de pedidos de descida
    bool ativo;                             // Se o elevador está ativo
    pthread_mutex_t mutex;                 // Mutex para controle de acesso à estrutura
    pthread_cond_t cond;                   // Variável de condição para sinalização
} Elevador;

// Variável global que define se o sistema está rodando
bool running = true;

// Array com todos os elevadores do sistema
Elevador elevadores[NUM_ELEVADORES];

// Função para ordenar uma lista de andares em ordem crescente ou decrescente
void ordenar_lista(int *lista, int tamanho, bool crescente) {
    for (int i = 0; i < tamanho - 1; i++) {
        for (int j = 0; j < tamanho - i - 1; j++) {
            if ((crescente && lista[j] > lista[j + 1]) || (!crescente && lista[j] < lista[j + 1])) {
                int temp = lista[j];
                lista[j] = lista[j + 1];
                lista[j + 1] = temp;
            }
        }
    }
}

// Função para adicionar um pedido a um elevador
void adicionar_pedido_elevador(Elevador *elev, int andar_destino) {
    pthread_mutex_lock(&elev->mutex); // Bloqueia acesso à estrutura

    // Verifica se é um pedido de subida
    if (andar_destino > elev->current_floor && elev->total_pedidos_subida < MAX_PEDIDOS) {
        elev->pedidos_subida[elev->total_pedidos_subida++] = andar_destino;
    }
    // Verifica se é de descida
    else if (andar_destino < elev->current_floor && elev->total_pedidos_descida < MAX_PEDIDOS) {
        elev->pedidos_descida[elev->total_pedidos_descida++] = andar_destino;
    }
    // Se está no mesmo andar, não faz nada
    else if (andar_destino == elev->current_floor) {
        printf("Elevador %d já está no andar %d\n", elev->id, elev->current_floor);
    }

    pthread_cond_signal(&elev->cond); // Sinaliza que há novo pedido
    pthread_mutex_unlock(&elev->mutex); // Libera acesso
}

// Função para exibir o status atual do elevador
void print_status_elevador(Elevador *elev) {
    pthread_mutex_lock(&elev->mutex); // Bloqueia o mutex para leitura segura

    printf("[Elevador %d] Andar atual: %d | Pedidos Subida: ", elev->id, elev->current_floor);
    for (int i = 0; i < elev->total_pedidos_subida; i++) {
        printf("%d ", elev->pedidos_subida[i]);
    }

    printf("| Pedidos Descida: ");
    for (int i = 0; i < elev->total_pedidos_descida; i++) {
        printf("%d ", elev->pedidos_descida[i]);
    }

    printf("\n");

    pthread_mutex_unlock(&elev->mutex); // Libera o mutex
}

// Função que define o comportamento da thread de cada elevador
void *elevador_thread(void *arg) {
    Elevador *elev = (Elevador *)arg; // Converte argumento para ponteiro de elevador

    while (running) {
        pthread_mutex_lock(&elev->mutex);

        // Espera até que haja pedidos ou o programa finalize
        while (elev->total_pedidos_subida == 0 && elev->total_pedidos_descida == 0 && running) {
            pthread_cond_wait(&elev->cond, &elev->mutex); // Aguarda novo pedido
        }

        if (!running) {
            pthread_mutex_unlock(&elev->mutex);
            break;
        }

        // Processa pedidos de subida
        if (elev->total_pedidos_subida > 0) {
            ordenar_lista(elev->pedidos_subida, elev->total_pedidos_subida, true); // Ordena crescente
            int prox = elev->pedidos_subida[0]; // Primeiro da fila

            printf("[Elevador %d] Indo para o andar %d (subida)\n", elev->id, prox);
            pthread_mutex_unlock(&elev->mutex);
            sleep(abs(elev->current_floor - prox)); // Simula tempo de deslocamento
            pthread_mutex_lock(&elev->mutex);

            elev->current_floor = prox; // Atualiza andar atual

            // Remove o pedido atendido da lista
            for (int i = 1; i < elev->total_pedidos_subida; i++) {
                elev->pedidos_subida[i - 1] = elev->pedidos_subida[i];
            }
            elev->total_pedidos_subida--; // Atualiza quantidade
        }

        // Processa pedidos de descida
        else if (elev->total_pedidos_descida > 0) {
            ordenar_lista(elev->pedidos_descida, elev->total_pedidos_descida, false); // Ordena decrescente
            int prox = elev->pedidos_descida[0];

            printf("[Elevador %d] Indo para o andar %d (descida)\n", elev->id, prox);
            pthread_mutex_unlock(&elev->mutex);
            sleep(abs(elev->current_floor - prox)); // Simula tempo
            pthread_mutex_lock(&elev->mutex);

            elev->current_floor = prox;

            for (int i = 1; i < elev->total_pedidos_descida; i++) {
                elev->pedidos_descida[i - 1] = elev->pedidos_descida[i];
            }
            elev->total_pedidos_descida--;
        }

        pthread_mutex_unlock(&elev->mutex);
    }

    return NULL;
}

// Função que gerencia a entrada do usuário
void *gerenciar_entrada(void *arg) {
    char input[100];

    while (running) {
        printf("\nDigite comando (formato: elevador andar OU 'status' OU 'sair'): ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            continue;
        }

        // Remove quebra de linha
        input[strcspn(input, "\n")] = 0;

        // Comando para sair
        if (strcmp(input, "sair") == 0) {
            running = false;
            for (int i = 0; i < NUM_ELEVADORES; i++) {
                pthread_cond_signal(&elevadores[i].cond); // Acorda todos os elevadores
            }
            break;
        }

        // Comando para exibir status
        if (strcmp(input, "status") == 0) {
            for (int i = 0; i < NUM_ELEVADORES; i++) {
                print_status_elevador(&elevadores[i]);
            }
            continue;
        }

        // Processa comandos do tipo "id andar"
        int elevador_id, andar;
        if (sscanf(input, "%d %d", &elevador_id, &andar) == 2) {
            if (elevador_id < 0 || elevador_id >= NUM_ELEVADORES || andar < 0 || andar >= MAX_ANDARES) {
                printf("Entrada inválida. Use: elevador(0-2) andar(0-10)\n");
                continue;
            }

            adicionar_pedido_elevador(&elevadores[elevador_id], andar);
        } else {
            printf("Comando inválido. Use: elevador(0-2) andar(0-10), 'status' ou 'sair'\n");
        }
    }

    return NULL;
}

// Função principal (main)
int main() {
    pthread_t threads[NUM_ELEVADORES]; // Array de threads dos elevadores
    pthread_t entrada_thread;         // Thread para entrada do usuário

    // Inicializa elevadores
    for (int i = 0; i < NUM_ELEVADORES; i++) {
        elevadores[i].id = i;
        elevadores[i].current_floor = 0;
        elevadores[i].total_pedidos_subida = 0;
        elevadores[i].total_pedidos_descida = 0;
        elevadores[i].ativo = true;
        pthread_mutex_init(&elevadores[i].mutex, NULL); // Inicializa mutex
        pthread_cond_init(&elevadores[i].cond, NULL);   // Inicializa variável de condição
        pthread_create(&threads[i], NULL, elevador_thread, &elevadores[i]); // Cria a thread
    }

    pthread_create(&entrada_thread, NULL, gerenciar_entrada, NULL); // Thread da entrada do usuário

    // Aguarda o término da thread de entrada
    pthread_join(entrada_thread, NULL);

    // Aguarda o término de todas as threads dos elevadores
    for (int i = 0; i < NUM_ELEVADORES; i++) {
        pthread_join(threads[i], NULL);
        pthread_mutex_destroy(&elevadores[i].mutex); // Destroi mutex
        pthread_cond_destroy(&elevadores[i].cond);   // Destroi variável de condição
    }

    printf("Sistema encerrado.\n");
    return 0;
}
