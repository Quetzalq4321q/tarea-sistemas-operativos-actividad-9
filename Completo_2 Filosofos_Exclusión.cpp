#include <algorithm> // Para std::min
#include <chrono> // Librería para trabajar con tiempo y duraciones
#include <cstdlib> // Para std::exit
#include <functional> // Para std::ref
#include <iostream> // Librería estándar para entrada y salida de datos
#include <mutex> // Librería para trabajar con mutex (exclusión mutua)
#include <random> // Librería para generar números aleatorios
#include <thread> // Librería para trabajar con hilos de ejecución
#include <utility> // Para std::swap
#include <vector> // Librería para trabajar con vectores

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Función para limitar el número máximo de fideos
int LimitFideMax(int fide) {
    const int maxFi = 2147483647;
    if (fide > maxFi) {
        std::cout << "\n-------------------------------------------" << std::endl;
        std::cout << "¡Se produjo un error!, el número de fideos excede la cantidad máxima" << std::endl;
        std::exit(1);
    }
    return std::min(fide, maxFi);
}

// Clase que representa a un filósofo
class Filo {
public:
    Filo(int id, int& fide, std::mutex& mutexFide) : id(id), fide(fide), mutexFide(mutexFide), estmag(0), timPensa(0) {}

// Sobrecarga del operador de llamada para utilizar el objeto como función
    void operator()() {
        while (true) {
            pensa();
            tomarTenedores();
            come();
            dejarTenedores();
            if (fide <= 0) {
                break;
            }
        }
    }

    // Método para obtener el valor actual del estómago
    int getEstmag() const {
        return estmag;
    }

    // Método para obtener el tiempo total de pensamiento
    int getTimPensa() const {
        return timPensa;
    }

public:
    // El filósofo está pensando
    void pensa() {
        int timp = getTimAlea();
        std::cout << "Filosofo N°" << id << " está pensando (" << timp << "s)" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(timp));
        timPensa += timp;
    }

    // El filósofo toma los tenedores
    void tomarTenedores() {
        int tenedorIzq = id - 1;
        int tenedorDer = id % 5;

        // Si el número del tenedor izquierdo es mayor al del derecho, se intercambian
        if (tenedorIzq > tenedorDer) {
            std::swap(tenedorIzq, tenedorDer);
        }

	// Accedemos a los mutex de los tenedores
        std::mutex& mutexTenedorIzq = tenedores[tenedorIzq];
        std::mutex& mutexTenedorDer = tenedores[tenedorDer];

	// Bloqueamos los mutex de los tenedores
        std::unique_lock<std::mutex> lockIzq(mutexTenedorIzq, std::defer_lock);
        std::unique_lock<std::mutex> lockDer(mutexTenedorDer, std::defer_lock);

        std::lock(lockIzq, lockDer);

        std::cout << "Filosofo N°" << id << " ha tomado los tenedores " << tenedorIzq + 1 << " y " << tenedorDer + 1 << std::endl;
    }

    // El filósofo está comiendo
    void come() {
        std::lock_guard<std::mutex> lock(mutexFide);

	// Si aún hay fideos disponibles
        if (fide > 0) {
            int porci = getTimAlea();

	    // Limitamos la cantidad de fideos a la cantidad disponible
            porci = std::min(porci, fide);
            fide -= porci;
            estmag += porci;
            std::cout << "Filosofo N° " << id << " está comiendo (" << porci << ")" << std::endl;
            std::cout << "Filosofo N° " << id << " incrementó el valor del estómago (" << estmag - porci << " + " << porci << " = " << estmag << ")" << std::endl;
        }
    }

    // El filósofo deja los tenedores
    void dejarTenedores() {
        int tenedorIzq = id - 1;
        int tenedorDer = id % 5;

	// Si el número del tenedor izquierdo es mayor al del derecho, se intercambian
        if (tenedorIzq > tenedorDer) {
            std::swap(tenedorIzq, tenedorDer);
        }

	// Accedemos a los mutex de los tenedores
        std::mutex& mutexTenedorIzq = tenedores[tenedorIzq];
        std::mutex& mutexTenedorDer = tenedores[tenedorDer];

	// Desbloqueamos los mutex de los tenedores
        mutexTenedorIzq.unlock();
        mutexTenedorDer.unlock();

        std::cout << "Filosofo N°" << id << " ha dejado los tenedores " << tenedorIzq + 1 << " y " << tenedorDer + 1 << std::endl;
    }

    // Método para generar un tiempo aleatorio de pensamiento
    int getTimAlea() const {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(1, 5);
        return dist(gen);
    }

    int id;
    int& fide;
    std::mutex& mutexFide;
    int estmag;
    int timPensa;
    static std::vector<std::mutex> tenedores;
};

std::vector<std::mutex> Filo::tenedores(5);

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    int fide;
    int numFilos;

    std::cout << "\n--------SIMULADOR DE FILOSOFOS COMENSALES--------" << std::endl;
    std::cout << "\nBienvenido Usuario, Escriba los datos pedidos:" << std::endl;
    std::cout << "\n(Recuerda que el máximo de fideos es 2,147,483,647)" << std::endl;
    std::cout << "\n- Cuantos Fideos Pondras: ";
    std::cin >> fide;
    fide = LimitFideMax(fide);
    std::cout << "\n- Escriba la cantidad de filósofos que habrá: ";
    std::cin >> numFilos;
    std::cout << "\n-------------------------------------------" << std::endl;
    std::cout << "\nEjecutando procedimiento......." << std::endl;
    std::cout << "\n-------------------------------------------" << std::endl;

    std::mutex mutexFide;

    std::vector<Filo> filos;
    filos.reserve(numFilos);
    std::vector<std::thread> hilos;

    // Creación de los filósofos y los hilos de ejecución
    for (int i = 0; i < numFilos; ++i) {
        filos.emplace_back(i + 1, fide, mutexFide);
        hilos.emplace_back(std::ref(filos[i]));
    }

    // Espera a que todos los hilos terminen su ejecución
    for (int i = 0; i < numFilos; ++i) {
        hilos[i].join();
    }

    std::cout << "\n-------------------------------------------" << std::endl;
    std::cout << "\n....La ejecución del programa ha terminado...." << std::endl;
    std::cout << "\nMostrando datos finales........" << std::endl;
    std::cout << "\n---------------Estadísticas:----------------\n" << std::endl;
    int totalEstmag = 0;
    int totalTimPensa = 0;
    for (int i = 0; i < numFilos; ++i) {
        std::cout << "+ Filosofos N°(" << filos[i].id << ") :" << std::endl;
        std::cout << "\t* Tiempo pensando: " << filos[i].getTimPensa() << "s" << std::endl;
        std::cout << "\t* Estómago: " << filos[i].getEstmag() << std::endl;
        totalEstmag += filos[i].getEstmag();
        totalTimPensa += filos[i].getTimPensa();
        std::cout << std::endl;
    }
    std::cout << "\n---------------Datos finales---------------\n" << std::endl;
    std::cout << "1.- Total de comida consumida por los filósofos: " << totalEstmag << std::endl;
    std::cout << "2.- Fideos que restan: " << fide << std::endl;
    std::cout << "3.- Total de tiempo pensando: " << totalTimPensa << "s" << std::endl;
    std::cout << "\n-------------------------------------------" << std::endl;
    std::cout << "\t\t\n-----FIN DE LA EJECUCIÓN------" << std::endl;
    return 0;
}