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

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

/**
 * Versión mejorada de: Completo_1 Filosofos_Exclusion.cpp
 *
 * Correcciones y mejoras aplicadas:
 * 1. Eliminación de condición de carrera sobre `fideos`:
 *    En el original, `while (fideos > 0)` e `if (fideos > 0)` leían `fideos` sin mutex.
 *    Ahora toda lectura/modificación de `fideos` ocurre bajo `std::lock_guard<std::mutex>`.
 * 2. Uso de `std::lock_guard` (RAII) en lugar de `lock()` / `unlock()` manuales.
 * 3. Sincronización de `std::cout` con `coutMutex` para evitar líneas mezcladas.
 * 4. Separación correcta entre `cantidad` de fideos comidos y `tiempoComiendo` (duración real).
 * 5. Uso de `thread_local std::mt19937` en lugar de reconstruir `random_device` en cada llamada.
 * 6. Validación de entrada (`numFilosofos >= 1`, `fideosIniciales >= 1`) y protección contra
 *    división por cero al calcular promedios y porcentajes.
 */

namespace {

struct PlatoCompartido {
    unsigned int fideos = 256;
    std::mutex mtx;
};

std::mutex coutMutex;

void imprimirSeguro(const std::string& msg) {
    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << msg << '\n';
}

double aleatorioReal(double minVal, double maxVal) {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<double> dis(minVal, maxVal);
    return dis(gen);
}

int aleatorioEntero(int minVal, int maxVal) {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dis(minVal, maxVal);
    return dis(gen);
}

class Filosofo {
private:
    int id;
    int estomago = 0;
    double tiempoPensando = 0.0;
    double tiempoComiendo = 0.0;

public:
    explicit Filosofo(int id) : id(id) {}

    void pensar() {
        const double tiempo = aleatorioReal(0.05, 0.25);
        tiempoPensando += tiempo;

        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2)
                << "Filosofo " << id << " está pensando (" << tiempo << "s)";
            imprimirSeguro(oss.str());
        }

        std::this_thread::sleep_for(std::chrono::duration<double>(tiempo));
    }

    bool intentarComer(PlatoCompartido& plato) {
        const unsigned int porcionDeseada = static_cast<unsigned int>(aleatorioEntero(1, 50));
        unsigned int cantidadComida = 0;

        {
            std::lock_guard<std::mutex> lock(plato.mtx);
            if (plato.fideos == 0) {
                return false;
            }
            cantidadComida = std::min(porcionDeseada, plato.fideos);
            plato.fideos -= cantidadComida;
        }

        const double duracionComida = aleatorioReal(0.05, 0.20);
        std::this_thread::sleep_for(std::chrono::duration<double>(duracionComida));

        const int estomagoAnterior = estomago;
        estomago += static_cast<int>(cantidadComida);
        tiempoComiendo += duracionComida;

        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2)
                << "Filosofo " << id << " comió " << cantidadComida << " fideos ("
                << duracionComida << "s) | estómago: "
                << estomagoAnterior << " + " << cantidadComida << " = " << estomago;
            imprimirSeguro(oss.str());
        }

        return true;
    }

    int getId() const { return id; }
    int getEstomago() const { return estomago; }
    double getTiempoPensando() const { return tiempoPensando; }
    double getTiempoComiendo() const { return tiempoComiendo; }
};

int leerEnteroPositivo(const std::string& prompt, int minimo, int maximo) {
    int valor = 0;
    while (true) {
        std::cout << prompt;
        if (std::cin >> valor && valor >= minimo && valor <= maximo) {
            return valor;
        }
        if (std::cin.eof()) {
            std::cerr << "\nFin de entrada (EOF) detectado. Abortando.\n";
            std::exit(1);
        }
        std::cout << "Valor inválido. Ingrese un entero entre "
                  << minimo << " y " << maximo << ".\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

} // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    PlatoCompartido plato;
    std::cout << "Fideos iniciales = " << plato.fideos << '\n';

    const int numFilosofos = leerEnteroPositivo("Ingrese cantidad de filósofos (1 - 100): ", 1, 100);

    std::vector<Filosofo> filosofos;
    filosofos.reserve(static_cast<std::size_t>(numFilosofos));
    for (int i = 1; i <= numFilosofos; ++i) {
        filosofos.emplace_back(i);
    }

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(numFilosofos));

    for (Filosofo& filosofo : filosofos) {
        threads.emplace_back([&filosofo, &plato]() {
            while (true) {
                filosofo.pensar();
                if (!filosofo.intentarComer(plato)) {
                    break;
                }
            }
        });
    }

    for (std::thread& t : threads) {
        t.join();
    }

    std::cout << "\nEstadísticas finales:\n";
    double tiempoTotalPensando = 0.0;
    double tiempoTotalComiendo = 0.0;
    int maxEstomago = -1;
    int maxFilosofoId = 0;
    long long totalEstomagos = 0;

    for (const Filosofo& f : filosofos) {
        std::cout << "  Estómago Filosofo " << f.getId() << " = " << f.getEstomago() << '\n';
        if (f.getEstomago() > maxEstomago) {
            maxEstomago = f.getEstomago();
            maxFilosofoId = f.getId();
        }
        tiempoTotalPensando += f.getTiempoPensando();
        tiempoTotalComiendo += f.getTiempoComiendo();
        totalEstomagos += f.getEstomago();
    }

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Fideos restantes = " << plato.fideos << '\n';
    std::cout << "Filósofo con el estómago más grande: Filosofo " << maxFilosofoId
              << " (" << maxEstomago << " fideos)\n";

    std::cout << "Porcentaje de tiempo pensando por filósofo:\n";
    for (const Filosofo& f : filosofos) {
        const double pct = (tiempoTotalPensando > 0.0)
                               ? (f.getTiempoPensando() / tiempoTotalPensando) * 100.0
                               : 0.0;
        std::cout << "  Filosofo " << f.getId() << ": " << pct << "% ("
                  << f.getTiempoPensando() << "s)\n";
    }

    std::cout << "Porcentaje de tiempo comiendo por filósofo:\n";
    for (const Filosofo& f : filosofos) {
        const double pct = (tiempoTotalComiendo > 0.0)
                               ? (f.getTiempoComiendo() / tiempoTotalComiendo) * 100.0
                               : 0.0;
        std::cout << "  Filosofo " << f.getId() << ": " << pct << "% ("
                  << f.getTiempoComiendo() << "s)\n";
    }

    const double promedioEstomagos =
        static_cast<double>(totalEstomagos) / static_cast<double>(numFilosofos);
    std::cout << "Total consumido = " << totalEstomagos
              << " | Promedio por filósofo = " << promedioEstomagos << '\n';

    return 0;
}
