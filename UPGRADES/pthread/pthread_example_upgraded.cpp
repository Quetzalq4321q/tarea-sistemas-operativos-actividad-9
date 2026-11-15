#include <chrono>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

/**
 * Versión mejorada de: so-main/pthread/pthread-example.cpp
 *
 * Mejoras aplicadas:
 * 1. Uso de std::thread y std::vector (RAII, sin arreglos C rígidos ni casts void*).
 * 2. Generador aleatorio thread_local std::mt19937 en lugar de rand() (que no es thread-safe).
 * 3. Sincronización de std::cout mediante std::mutex para evitar líneas mezcladas.
 * 4. Uso de std::chrono para pausas portables (funciona igual en Windows, Linux y macOS).
 */

namespace {

constexpr int NUM_THREADS_DEFAULT = 5;
std::mutex coutMutex;

void imprimirSeguro(const std::string& mensaje) {
    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << mensaje << '\n';
}

int generarEsperaMs(int minMs, int maxMs) {
    thread_local std::mt19937 generador(std::random_device{}());
    std::uniform_int_distribution<int> distribucion(minMs, maxMs);
    return distribucion(generador);
}

void ejecutarTrabajo(int idHilo) {
    const int esperaMs = generarEsperaMs(150, 750);

    {
        std::ostringstream oss;
        oss << "HILO " << idHilo << ": Iniciado. Dormirá " << esperaMs << " ms.";
        imprimirSeguro(oss.str());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(esperaMs));

    {
        std::ostringstream oss;
        oss << "HILO " << idHilo << ": Finalizado.";
        imprimirSeguro(oss.str());
    }
}

} // namespace

int main(int argc, char* argv[]) {
    int numHilos = NUM_THREADS_DEFAULT;
    if (argc > 1) {
        try {
            const int parsed = std::stoi(argv[1]);
            if (parsed > 0 && parsed <= 64) {
                numHilos = parsed;
            }
        } catch (...) {
            std::cerr << "Argumento inválido, usando " << NUM_THREADS_DEFAULT << " hilos.\n";
        }
    }

    std::vector<std::thread> hilos;
    hilos.reserve(static_cast<std::size_t>(numHilos));

    for (int i = 0; i < numHilos; ++i) {
        imprimirSeguro("MAIN: Creando hilo " + std::to_string(i) + ".");
        hilos.emplace_back(ejecutarTrabajo, i);
    }

    imprimirSeguro("MAIN: Todos los hilos han sido creados.");

    for (int i = 0; i < numHilos; ++i) {
        if (hilos[static_cast<std::size_t>(i)].joinable()) {
            hilos[static_cast<std::size_t>(i)].join();
            imprimirSeguro("MAIN: Hilo " + std::to_string(i) + " ha terminado.");
        }
    }

    imprimirSeguro("MAIN: Programa finalizado correctamente.");
    return 0;
}
