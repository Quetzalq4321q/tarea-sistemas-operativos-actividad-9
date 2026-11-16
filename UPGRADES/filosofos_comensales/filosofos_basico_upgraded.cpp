#include <chrono>
#include <iostream>
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
 * Versión mejorada de:
 *   so-main/filosofos_comensales/filosofos00.cpp .. filosofos04.cpp
 *
 * Mejoras aplicadas:
 * 1. Evoluciona de ejecución secuencial / bucles infinitos (for(;;)) a ejecución
 *    concurrente acotada y controlada.
 * 2. Introduce exclusión mutua real sobre los tenedores usando std::scoped_lock
 *    (bloqueo sin interbloqueos / deadlocks).
 * 3. Sincroniza la salida estándar con std::mutex para evitar mensajes entrelazados.
 * 4. Utiliza std::string y std::vector en lugar de char[20] vulnerables.
 */

namespace {

constexpr int CICLOS_POR_FILOSOFO = 3;
std::mutex coutMutex;

void imprimirSeguro(const std::string& mensaje) {
    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << mensaje << '\n';
}

int generarPausaMs(int minMs, int maxMs) {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(minMs, maxMs);
    return dist(gen);
}

void ejecutarFilosofo(int id,
                      const std::string& nombre,
                      std::vector<std::mutex>& tenedores) {
    const std::size_t n = tenedores.size();
    const std::size_t izq = static_cast<std::size_t>(id);
    const std::size_t der = (izq + 1) % n;

    for (int ciclo = 1; ciclo <= CICLOS_POR_FILOSOFO; ++ciclo) {
        imprimirSeguro(nombre + " (Filósofo-" + std::to_string(id) + ") está pensando...");
        std::this_thread::sleep_for(std::chrono::milliseconds(generarPausaMs(80, 200)));

        // std::scoped_lock adquiere ambos tenedores evitando interbloqueo (deadlock)
        std::scoped_lock lockTenedores(tenedores[izq], tenedores[der]);
        {
            std::ostringstream oss;
            oss << nombre << " (Filósofo-" << id << ") está comiendo [ciclo "
                << ciclo << "/" << CICLOS_POR_FILOSOFO
                << ", tenedores " << izq << " y " << der << "]";
            imprimirSeguro(oss.str());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(generarPausaMs(80, 200)));
    }

    imprimirSeguro(nombre + " (Filósofo-" + std::to_string(id) + ") terminó su cena.");
}

} // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    const std::vector<std::string> nombres = {
        "Sócrates", "Pitágoras", "Platón", "Aristóteles", "Hipatia"
    };

    std::vector<std::mutex> tenedores(nombres.size());
    std::vector<std::thread> hilos;
    hilos.reserve(nombres.size());

    for (std::size_t i = 0; i < nombres.size(); ++i) {
        hilos.emplace_back(ejecutarFilosofo, static_cast<int>(i), std::cref(nombres[i]), std::ref(tenedores));
    }

    for (auto& hilo : hilos) {
        hilo.join();
    }

    imprimirSeguro("Todos los filósofos terminaron sin interbloqueos ni condiciones de carrera.");
    return 0;
}
