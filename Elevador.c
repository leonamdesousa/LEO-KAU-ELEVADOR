#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_PEDIDOS 100
#define CAPACIDADE_MAX 11
#define TEMPO_PORTA 5
#define ANDAR_MAX 10

typedef struct {
    int andar_atual;
    bool subindo; // true = subindo, false = descendo
    int pessoas;

    int fila_ativa[MAX_PEDIDOS];
    int tam_ativa;

    int fila_pendente[MAX_PEDIDOS];
    int tam_pendente;

    bool esperando_pedido;

    // Novas variáveis para controle de entrada/saída de pessoas
    int pessoas_entrando;
    int pessoas_saindo;
    bool tem_entrada_saida; // flag para indicar se há atualização pendente
} Elevador;

Elevador elevador;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_pedido = PTHREAD_COND_INITIALIZER;
bool terminar = false;

// ... (funções ordenar_fila, remover_pedido, andar_ja_pedido, adicionar_pedido permanecem iguais, só adiciono verificação de limites)

void adicionar_pedido(Elevador* e, int andar) {
    pthread_mutex_lock(&mutex);
    if (andar < 0 || andar > ANDAR_MAX) {
        printf("Andar inválido. Máximo: %d\n", ANDAR_MAX);
        pthread_mutex_unlock(&mutex);
        return;
    }
    if (andar == e->andar_atual) {
        printf("Elevador já está no andar %d\n", andar);
        pthread_mutex_unlock(&mutex);
        return;
    }
    if (andar_ja_pedido(e, andar)) {
        printf("Andar %d já foi pedido\n", andar);
        pthread_mutex_unlock(&mutex);
        return;
    }

    if (e->subindo) {
        if (andar > e->andar_atual) {
            if (e->tam_ativa < MAX_PEDIDOS) {
                e->fila_ativa[e->tam_ativa++] = andar;
                ordenar_fila(e);
            } else {
                printf("Fila ativa cheia, não foi possível adicionar o pedido.\n");
            }
        } else {
            if (e->tam_pendente < MAX_PEDIDOS) {
                e->fila_pendente[e->tam_pendente++] = andar;
            } else {
                printf("Fila pendente cheia, não foi possível adicionar o pedido.\n");
            }
        }
    } else {
        if (andar < e->andar_atual) {
            if (e->tam_ativa < MAX_PEDIDOS) {
                e->fila_ativa[e->tam_ativa++] = andar;
                ordenar_fila(e);
            } else {
                printf("Fila ativa cheia, não foi possível adicionar o pedido.\n");
            }
        } else {
            if (e->tam_pendente < MAX_PEDIDOS) {
                e->fila_pendente[e->tam_pendente++] = andar;
            } else {
                printf("Fila pendente cheia, não foi possível adicionar o pedido.\n");
            }
        }
    }
    e->esperando_pedido = false;
    pthread_cond_signal(&cond_pedido);
    pthread_mutex_unlock(&mutex);
}

// Função para informar entrada/saída de pessoas no andar atual (chamada pelo menu principal)
void atualizar_entrada_saida() {
    pthread_mutex_lock(&mutex);
    printf("No andar %d, quantas pessoas saíram? ", elevador.andar_atual);
    int sairam;
    scanf("%d", &sairam);
    if (sairam < 0) sairam = 0;
    if (sairam > elevador.pessoas) sairam = elevador.pessoas;

    printf("No andar %d, quantas pessoas entraram? (0 para continuar sem espera) ", elevador.andar_atual);
    int entraram;
    scanf("%d", &entraram);
    if (entraram < 0) entraram = 0;
    if (elevador.pessoas - sairam + entraram > CAPACIDADE_MAX) {
        printf("[Elevador] Capacidade máxima atingida! Apenas %d pessoas podem entrar.\n",
               CAPACIDADE_MAX - (elevador.pessoas - sairam));
        entraram = CAPACIDADE_MAX - (elevador.pessoas - sairam);
    }

    elevador.pessoas_saindo = sairam;
    elevador.pessoas_entrando = entraram;
    elevador.tem_entrada_saida = true;
    pthread_cond_signal(&cond_pedido);
    pthread_mutex_unlock(&mutex);
}

// Thread do elevador
void* thread_elevador(void* arg) {
    Elevador* e = (Elevador*)arg;

    while (!terminar) {
        pthread_mutex_lock(&mutex);

        // Espera por pedidos
        while (e->tam_ativa == 0 && e->tam_pendente == 0 && !terminar) {
            printf("[Elevador] Sem pedidos, aguardando...\n");
            e->esperando_pedido = true;
            pthread_cond_wait(&cond_pedido, &mutex);
            e->esperando_pedido = false;
        }

        if (terminar) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // Se não há pedidos ativos, ativa os pendentes trocando a direção
        if (e->tam_ativa == 0 && e->tam_pendente > 0) {
            e->subindo = !e->subindo;
            for (int i = 0; i < e->tam_pendente; i++) {
                e->fila_ativa[i] = e->fila_pendente[i];
            }
            e->tam_ativa = e->tam_pendente;
            e->tam_pendente = 0;
            ordenar_fila(e);
        }

        pthread_mutex_unlock(&mutex);

        pthread_mutex_lock(&mutex);
        int prox_andar = -1;
        if (e->subindo) {
            for (int i = 0; i < e->tam_ativa; i++) {
                if (e->fila_ativa[i] >= e->andar_atual) {
                    prox_andar = e->fila_ativa[i];
                    break;
                }
            }
        } else {
            for (int i = 0; i < e->tam_ativa; i++) {
                if (e->fila_ativa[i] <= e->andar_atual) {
                    prox_andar = e->fila_ativa[i];
                    break;
                }
            }
        }
        pthread_mutex_unlock(&mutex);

        if (prox_andar == -1) {
            pthread_mutex_lock(&mutex);
            e->esperando_pedido = true;
            pthread_cond_wait(&cond_pedido, &mutex);
            e->esperando_pedido = false;
            pthread_mutex_unlock(&mutex);
            continue;
        }

        // Move elevador 1 andar por vez
        pthread_mutex_lock(&mutex);
        if (e->andar_atual < prox_andar) {
            e->andar_atual++;
            printf("[Elevador] Subindo para andar %d\n", e->andar_atual);
            pthread_mutex_unlock(&mutex);
            sleep(1);
            continue;
        } else if (e->andar_atual > prox_andar) {
            e->andar_atual--;
            printf("[Elevador] Descendo para andar %d\n", e->andar_atual);
            pthread_mutex_unlock(&mutex);
            sleep(1);
            continue;
        }
        pthread_mutex_unlock(&mutex);

        // Elevador chegou ao andar pedido
        pthread_mutex_lock(&mutex);
        printf("[Elevador] Chegou no andar %d. Abrindo portas por %d segundos...\n", e->andar_atual, TEMPO_PORTA);
        pthread_mutex_unlock(&mutex);

        sleep(TEMPO_PORTA);

        // Agora espera o usuário informar entrada/saída via menu (flag)
        pthread_mutex_lock(&mutex);
        while (!e->tem_entrada_saida && !terminar) {
            printf("[Elevador] Aguardando atualização de entrada/saída no andar %d...\n", e->andar_atual);
            e->esperando_pedido = true;
            pthread_cond_wait(&cond_pedido, &mutex);
            e->esperando_pedido = false;
        }

        if (terminar) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // Atualiza o número de pessoas dentro do elevador
        e->pessoas -= e->pessoas_saindo;
        if (e->pessoas < 0) e->pessoas = 0;
        e->pessoas += e->pessoas_entrando;
        if (e->pessoas > CAPACIDADE_MAX) e->pessoas = CAPACIDADE_MAX;

        printf("[Elevador] Pessoas no elevador agora: %d\n", e->pessoas);

        e->pessoas_saindo = 0;
        e->pessoas_entrando = 0;
        e->tem_entrada_saida = false;

        // Remove pedido do andar atual da fila ativa
        for (int i = 0; i < e->tam_ativa; i++) {
            if (e->fila_ativa[i] == e->andar_atual) {
                remover_pedido(e, i);
                break;
            }
        }

        // Se ninguém entrou, não espera e segue para próximo andar
        if (e->pessoas_entrando == 0) {
            printf("[Elevador] Ninguém entrou, seguindo para próximo andar...\n");
            pthread_mutex_unlock(&mutex);
            sleep(1);
            continue;
        }

        // Caso tenha entrado gente, espera novos pedidos antes de seguir
        printf("[Elevador] Aguardando novos pedidos antes de seguir...\n");
        e->esperando_pedido = true;
        pthread_cond_wait(&cond_pedido, &mutex);
        e->esperando_pedido = false;
        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}
int main() {
    // Inicializações
    elevador.andar_atual = 0;
    elevador.subindo = true;
    elevador.pessoas = 0;
    elevador.tam_ativa = 0;
    elevador.tam_pendente = 0;
    elevador.esperando_pedido = false;
    elevador.pessoas_entrando = 0;
    elevador.pessoas_saindo = 0;
    elevador.tem_entrada_saida = false;

    pthread_t tid;
    pthread_create(&tid, NULL, thread_elevador, &elevador);

    while (!terminar) {
        printf("=== Menu do elevador ===\n");
        printf("1 - Adicionar pedidos\n");
        printf("2 - Mostrar status\n");
        printf("3 - Atualizar entrada/saída de pessoas (no andar atual)\n");
        printf("0 - Sair\n");
        printf("Escolha: ");

        int opcao;
        if(scanf("%d", &opcao) != 1){
            printf("Entrada inválida.\n");
            while(getchar() != '\n'); // limpa buffer stdin
            continue;
        }

        if (opcao == 1) {
            printf("Digite andares separados por espaço (0 para terminar): ");
            while (1) {
                int andar;
                if(scanf("%d", &andar) != 1){
                    printf("Entrada inválida.\n");
                    while(getchar() != '\n');
                    break;
                }
                if (andar == 0) break;
                adicionar_pedido(&elevador, andar);
            }
        } else if (opcao == 2) {
            imprimir_status(&elevador);
        } else if (opcao == 3) {
            atualizar_entrada_saida();
        } else if (opcao == 0) {
            pthread_mutex_lock(&mutex);
            terminar = true;
            pthread_cond_signal(&cond_pedido);
            pthread_mutex_unlock(&mutex);
            break;
        } else {
            printf("Opção inválida.\n");
        }
    }

    pthread_join(tid, NULL);

    printf("Programa encerrado.\n");
    return 0;
}
