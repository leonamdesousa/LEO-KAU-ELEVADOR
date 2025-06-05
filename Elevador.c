#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_ANDARES 10
#define CAPACIDADE_ELEVADOR 5

int fila_pedidos[100];
int total_pedidos = 0;
int pessoas_no_elevador = 0;
int andar_atual = 0;
bool portas_abertas = false;
bool running = true;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void status_elevador() {
    printf("\n--- Status do Elevador ---\n");
    printf("Andar atual: %d\n", andar_atual);
    printf("Pessoas no elevador: %d\n", pessoas_no_elevador);
    printf("Pedidos na fila: ");
    if (total_pedidos == 0) {
        printf("Nenhum");
    } else {
        for (int i = 0; i < total_pedidos; i++) {
            printf("%d ", fila_pedidos[i]);
        }
    }
    printf("\n--------------------------\n");
}

void abrir_portas() {
    portas_abertas = true;
    printf("[Elevador] Chegou ao andar %d. Abrindo portas por 3 segundos...\n", andar_atual);
    pessoas_no_elevador++;
    sleep(3);
    status_elevador();
    int novos_andares[100];
    int novos = 0;
    printf("Você pode adicionar novos andares durante a parada (digite andares separados por espaço, ou -1 para continuar):\n");
    while (1) {
        char linha[100];
        fgets(linha, sizeof(linha), stdin);
        char *ptr = linha;
        int num;
        while (sscanf(ptr, "%d", &num) == 1) {
            if (num == -1) goto fim_input;
            if (num >= 0 && num <= MAX_ANDARES) {
                novos_andares[novos++] = num;
            }
            while (*ptr && *ptr != ' ') ptr++;
            while (*ptr == ' ') ptr++;
        }
    }
fim_input:
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < novos; i++) {
        fila_pedidos[total_pedidos++] = novos_andares[i];
        printf("[Pedido] Pedido para andar %d registrado.\n", novos_andares[i]);
    }
    pthread_mutex_unlock(&mutex);
    printf("[Elevador] Portas fechadas.\n");
    portas_abertas = false;
}

void* elevador_thread(void* arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        if (!running && total_pedidos == 0) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        if (total_pedidos == 0) {
            pthread_mutex_unlock(&mutex);
            printf("\n[Elevador] Sem pedidos, aguardando...\n");
            status_elevador();
            sleep(2);
            continue;
        }

        int destino = fila_pedidos[0];
        for (int i = 0; i < total_pedidos - 1; i++) {
            fila_pedidos[i] = fila_pedidos[i + 1];
        }
        total_pedidos--;
        pthread_mutex_unlock(&mutex);

        if (andar_atual < destino) {
            for (int i = andar_atual + 1; i <= destino; i++) {
                andar_atual = i;
                printf("[Elevador] Subindo para andar %d...\n", i);
                sleep(1);
            }
        } else if (andar_atual > destino) {
            for (int i = andar_atual - 1; i >= destino; i--) {
                andar_atual = i;
                printf("[Elevador] Descendo para andar %d...\n", i);
                sleep(1);
            }
        }

        abrir_portas();
    }

    printf("[Elevador] Sistema finalizado.\n");
    return NULL;
}

void registrar_pedidos_iniciais() {
    while (1) {
        status_elevador();
        printf("Digite o(s) andar(es) desejado(s) (0 a %d), separados por espaço, ou -1 para sair:\n", MAX_ANDARES);
        char linha[100];
        fgets(linha, sizeof(linha), stdin);
        char *ptr = linha;
        int num;
        bool sair = false;
        pthread_mutex_lock(&mutex);
        while (sscanf(ptr, "%d", &num) == 1) {
            if (num == -1) {
                running = false;
                sair = true;
                break;
            }
            if (num >= 0 && num <= MAX_ANDARES) {
                fila_pedidos[total_pedidos++] = num;
                printf("[Pedido] Pedido para andar %d registrado.\n", num);
            }
            while (*ptr && *ptr != ' ') ptr++;
            while (*ptr == ' ') ptr++;
        }
        pthread_mutex_unlock(&mutex);
        if (sair) break;
    }
}

int main() {
    pthread_t elevador;

    pthread_create(&elevador, NULL, elevador_thread, NULL);
    registrar_pedidos_iniciais();
    pthread_join(elevador, NULL);

    return 0;
}
