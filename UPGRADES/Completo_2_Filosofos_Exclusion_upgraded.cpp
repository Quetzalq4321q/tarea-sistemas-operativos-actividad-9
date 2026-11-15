#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

/**
 * Versión mejorada de: Completo_2 Filosofos_Exclusión.cpp
 *
 * Correcciones críticas respecto al original:
 * 1. BUG CRÍTICO DE TIEMPO DE VIDA EN TENEDORES:
 *    En el original, `tomarTenedores()` creaba `unique_lock` locales que se destruían
 *    al salir del método (desbloqueando los mutex de inmediato), y luego `dejarTenedores()`
 *    volvía a llamar `.unlock()` sobre mutex ya desbloqueados (Comportamiento Indefinido).
 *    Ahora se utiliza `std::scoped_lock` cubriendo todo el bloque `tomar -> comer -> dejar`.
 * 2. TAMAÑO DINÁMICO DE TENEDORES:
 *    El original fijaba `tenedores(5)` e `id % 5`: si el usuario ingresaba > 5 filósofos,
 *    producía acceso fuera de límites (crash). Ahora `tenedores` tiene tamaño `numFilos`
 *    y exige `numFilos >= 2` para formar una mesa circular válida.
 * 3. VALIDACIÓN REAL DE LÍMITE DE FIDEOS:
 *    Se lee con `long long` y se valida contra `INT_MAX` sin desbordamiento silencioso.
 * 4. SIN CONDICIÓN DE CARRERA EN `fide` NI EN `std::cout`:
 *    La comprobación y descuento de fideos se realizan atómicamente bajo `mutexFide`,
 *    y la consola se protege con `mutexCout`.
 */

namespace {

std::mutex mutexCout;

void imprimirSeguro(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutexCout);
    std::cout << msg << '\n';
}

int generarEnteroAleatorio(int minVal, int maxVal) {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(minVal, maxVal);
    return dist(gen);
}

class Filo {
private:
    int id; // 1 .. numFilos
    int numFilos;
    int& fide;
    std::mutex& mutexFide;
    std::vector<std::mutex>& tenedores;
    int estmag = 0;
    double timPensa = 0.0;
    double timCome = 0.0;

public:
    Filo(int id,
         int numFilos,
         int& fide,
         std::mutex& mutexFide,
         std::vector<std::mutex>& tenedores)
        : id(id),
          numFilos(numFilos),
          fide(fide),
          mutexFide(mutexFide),
          tenedores(tenedores) {}

    void operator()() {
        while (true) {
            pensa();
            if (!tomarComerYDejarTenedores()) {
                break;
            }
        }
    }

    int getId() const { return id; }
    int getEstmag() const { return estmag; }
    double getTimPensa() const { return timPensa; }
    double getTimCome() const { return timCome; }

private:
    void pensa() {
        const int pausaMs = generarEnteroAleatorio(80, 250);
        const double segundos = static_cast<double>(pausaMs) / 1000.0;
        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2)
                << "Filósofo N°" << id << " está pensando (" << segundos << "s)";
            imprimirSeguro(oss.str());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(pausaMs));
        timPensa += segundos;
    }

    bool tomarComerYDejarTenedores() {
        const int tenedorIzq = id - 1;
        const int tenedorDer = id % numFilos;

        // std::scoped_lock adquiere ambos tenedores sin deadlock y los libera al salir del ámbito
        std::scoped_lock lockTenedores(tenedores[static_cast<std::size_t>(tenedorIzq)],
                                       tenedores[static_cast<std::size_t>(tenedorDer)]);

        {
            std::ostringstream oss;
            oss << "Filósofo N°" << id << " ha tomado los tenedores "
                << (tenedorIzq + 1) << " y " << (tenedorDer + 1);
            imprimirSeguro(oss.str());
        }

        int porci = 0;
        {
            std::lock_guard<std::mutex> lock(mutexFide);
            if (fide <= 0) {
                std::ostringstream oss;
                oss << "Filósofo N°" << id << " deja los tenedores "
                    << (tenedorIzq + 1) << " y " << (tenedorDer + 1)
                    << " (ya no quedan fideos)";
                imprimirSeguro(oss.str());
                return false;
            }
            const int deseado = generarEnteroAleatorio(1, 25);
            porci = std::min(deseado, fide);
            fide -= porci;
        }

        const int pausaComidaMs = generarEnteroAleatorio(60, 200);
        std::this_thread::sleep_for(std::chrono::milliseconds(pausaComidaMs));

        const int estomagoAnterior = estmag;
        estmag += porci;
        timCome += static_cast<double>(pausaComidaMs) / 1000.0;

        {
            std::ostringstream oss;
            oss << "Filósofo N°" << id << " está comiendo (" << porci << " fideos) | estómago: "
                << estomagoAnterior << " + " << porci << " = " << estmag;
            imprimirSeguro(oss.str());
        }

        {
            std::ostringstream oss;
            oss << "Filósofo N°" << id << " ha dejado los tenedores "
                << (tenedorIzq + 1) << " y " << (tenedorDer + 1);
            imprimirSeguro(oss.str());
        }

        return true;
    }
};

int leerFideosValidos() {
    constexpr long long maxFi = std::numeric_limits<int>::max();
    long long entrada = 0;
    while (true) {
        std::cout << "- ¿Cuántos fideos pondrás? (1 - " << maxFi << "): ";
        if (std::cin >> entrada && entrada >= 1 && entrada <= maxFi) {
            return static_cast<int>(entrada);
        }
        if (std::cin.eof()) {
            std::cerr << "\nFin de entrada (EOF) detectado. Abortando.\n";
            std::exit(1);
        }
        std::cout << "¡Error! Ingrese un entero válido entre 1 y " << maxFi << ".\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

int leerCantidadFilosofos() {
    int n = 0;
    while (true) {
        std::cout << "- Escriba la cantidad de filósofos (2 - 100): ";
        if (std::cin >> n && n >= 2 && n <= 100) {
            return n;
        }
        if (std::cin.eof()) {
            std::cerr << "\nFin de entrada (EOF) detectado. Abortando.\n";
            std::exit(1);
        }
        std::cout << "¡Error! Se requieren al menos 2 filósofos (y máximo 100).\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

} // namespace

int main() {
    std::cout << "\n-------- SIMULADOR DE FILÓSOFOS COMENSALES (UPGRADE) --------\n\n";
    int fide = leerFideosValidos();
    const int numFilos = leerCantidadFilosofos();

    std::cout << "\n-------------------------------------------\n"
              << "Ejecutando procedimiento...\n"
              << "-------------------------------------------\n";

    std::mutex mutexFide;
    std::vector<std::mutex> tenedores(static_cast<std::size_t>(numFilos));

    std::vector<Filo> filos;
    filos.reserve(static_cast<std::size_t>(numFilos));
    for (int i = 0; i < numFilos; ++i) {
        filos.emplace_back(i + 1, numFilos, fide, mutexFide, tenedores);
    }

    std::vector<std::thread> hilos;
    hilos.reserve(static_cast<std::size_t>(numFilos));
    for (int i = 0; i < numFilos; ++i) {
        hilos.emplace_back(std::ref(filos[static_cast<std::size_t>(i)]));
    }

    for (std::thread& h : hilos) {
        h.join();
    }

    std::cout << "\n-------------------------------------------\n"
              << ".... La ejecución del programa ha terminado ....\n"
              << "--------------- Estadísticas ----------------\n\n";

    long long totalEstmag = 0;
    double totalTimPensa = 0.0;
    double totalTimCome = 0.0;

    std::cout << std::fixed << std::setprecision(2);
    for (const Filo& f : filos) {
        std::cout << "+ Filósofo N°(" << f.getId() << ") :\n"
                  << "\t* Tiempo pensando : " << f.getTimPensa() << "s\n"
                  << "\t* Tiempo comiendo : " << f.getTimCome() << "s\n"
                  << "\t* Estómago        : " << f.getEstmag() << " fideos\n\n";
        totalEstmag += f.getEstmag();
        totalTimPensa += f.getTimPensa();
        totalTimCome += f.getTimCome();
    }

    std::cout << "--------------- Datos finales ---------------\n"
              << "1.- Total de comida consumida : " << totalEstmag << " fideos\n"
              << "2.- Fideos que restan         : " << fide << "\n"
              << "3.- Total de tiempo pensando  : " << totalTimPensa << "s\n"
              << "4.- Total de tiempo comiendo  : " << totalTimCome << "s\n"
              << "-------------------------------------------\n";

    return 0;
}
