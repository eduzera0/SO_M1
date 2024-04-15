#include <iostream>
#include <ctime>
#include <chrono>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>

using namespace std;
using namespace std::chrono;

struct Contador {
    int esteira1;
    int esteira2;
    float peso_total;
    pthread_mutex_t mutex;
};

struct EsteiraInfo {
    int id;
    float peso;
    float intervalo;
    Contador *contador;
};

//Valores dados no enunciado
const int INTERVALO_DISPLAY = 2;
const int TOTAL_UNIDADES = 500;
const float PESO_ESTEIRA_1 = 5.0;
const float PESO_ESTEIRA_2 = 2.0;

void *simular_esteira(void *arg) {
    EsteiraInfo *info = (EsteiraInfo *)arg;
    Contador *contador = (Contador *)info->contador;

    while (true) {
        usleep(info->intervalo * 1000000);

        pthread_mutex_lock(&contador->mutex);
        if (info->id == 1) {
            contador->esteira1++;
        } else {
            contador->esteira2++;
        }
        contador->peso_total += info->peso;

        if (contador->esteira1 + contador->esteira2 >= TOTAL_UNIDADES) {
            pthread_mutex_unlock(&contador->mutex);
            break;
        }

        pthread_mutex_unlock(&contador->mutex);
    }

    return nullptr;
}

void *atualiza_display(void *arg) {
    Contador *contador = (Contador *)arg;
    while (true) {
        sleep(INTERVALO_DISPLAY);

        pthread_mutex_lock(&contador->mutex);
        cout << "Esteira 1: " << contador->esteira1
            << " <--> Esteira 2: " << contador->esteira2
            << " <--> Peso Total: " << contador->peso_total
            << " Kg" << endl;
        pthread_mutex_unlock(&contador->mutex);
    }

    return nullptr;
}

// Função pipe_solution
void pipe_solution() {
    int shm_fd = shm_open("contador", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(Contador));
    Contador *contador = (Contador *)mmap(0, sizeof(Contador), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    contador->esteira1 = 0;
    contador->esteira2 = 0;
    contador->peso_total = 0;
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&contador->mutex, &mattr);

    EsteiraInfo info1{1, PESO_ESTEIRA_1, 2, contador};
    EsteiraInfo info2{2, PESO_ESTEIRA_2, 1, contador};

    int pipefd[2];
    pipe(pipefd);
    pid_t pid = fork();

    if (pid == 0) { // Processo filho
        close(pipefd[0]); // Fecha o lado de leitura do pipe
        simular_esteira((void *)&info2);
        close(pipefd[1]); // Fecha o lado de escrita do pipe
        exit(0);
    } else { // Processo pai
        pthread_t display_thread;
        pthread_create(&display_thread, nullptr, atualiza_display, (void *)contador);

        close(pipefd[1]); // Fecha o lado de escrita do pipe
        simular_esteira((void *)&info1);
        wait(NULL); // Aguarda o processo filho terminar

        pthread_cancel(display_thread);
        pthread_join(display_thread, nullptr);
    }

    munmap(contador, sizeof(Contador));
    shm_unlink("/esteira_contador");
}

// Função multithread_solution
void multithread_solution() {
    Contador *contador = new Contador;
    contador->esteira1 = 0;
    contador->esteira2 = 0;
    contador->peso_total = 0;
    pthread_mutex_init(&contador->mutex, nullptr);

    EsteiraInfo info1{1, PESO_ESTEIRA_1, 2, contador};
    EsteiraInfo info2{2, PESO_ESTEIRA_2, 1, contador};

    pthread_t esteira1_thread, esteira2_thread, display_thread;

    pthread_create(&esteira1_thread, nullptr, simular_esteira, (void *)&info1);
    pthread_create(&esteira2_thread, nullptr, simular_esteira, (void *)&info2);
    pthread_create(&display_thread, nullptr, atualiza_display, (void *)contador);

    pthread_join(esteira1_thread, nullptr);
    pthread_join(esteira2_thread, nullptr);

    pthread_cancel(display_thread);
    pthread_join(display_thread, nullptr);

    delete contador;
}

int main() {

    cout << "\n\n<---------SOLUÇÃO 1: IPC VIA PIPE--------->" << endl;
    pipe_solution();

    cout << "\n\n<---------SOLUÇÃO 2: MULTITHREAD--------->" << endl;
    multithread_solution();
  
    return 0;
}