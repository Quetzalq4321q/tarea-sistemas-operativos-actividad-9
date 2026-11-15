#include <atomic>
#include <chrono>
#include <iostream>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

/**
 * Versión moderna C++17 de:
 *   so-main/exclusion_mutua/exclusionMutua00.cpp .. 02.cpp
 *
 * Mejoras aplicadas:
 * 1. Comparación directa entre std::mutex (con std::lock_guard RAII) y std::atomic<long long>.
 * 2. Eliminación de variables globales mutables mediante encapsulación.
 * 3. Medición de rendimiento con std::chrono::steady_clock.
 * 4. Validación robusta de entradas con std::cin.
 */

namespace {

constexpr int NUM_HILOS = 5;

long long leerTope() {
    long long tope = 0;
    while (true) {
        std::cout << "Ingrese tope de incrementos por hilo (> 0): ";
        if (std::cin >> tope && tope > 0) {
            return tope;
        }
        if (std::cin.eof()) {
            std::cerr << "\nFin de entrada (EOF) detectado. Usando valor por defecto: 10000.\n";
            return 10000;
        }
        std::cout << "Entrada inválida. Debe ingresar un entero positivo.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

void ejecutarConMutex(int numHilos, long long tope) {
    long long variable = 0;
    std::mutex mtx;
    std::vector<std::thread> hilos;
    hilos.reserve(static_cast<std::size_t>(numHilos));

    const auto inicio = std::chrono::steady_clock::now();
    for (int i = 0; i < numHilos; ++i) {
        hilos.emplace_back([&variable, &mtx, tope]() {
            for (long long j = 0; j < tope; ++j) {
                std::lock_guard<std::mutex> lock(mtx);
                ++variable;
            }
        });
    }
    for (auto& h : hilos) {
        h.join();
    }
    const auto fin = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(fin - inicio).count();

    const long long esperado = static_cast<long long>(numHilos) * tope;
    std::cout << "\n[std::mutex + std::lock_guard]\n"
              << "  Resultado : " << variable << " (Esperado: " << esperado << ")\n"
              << "  Tiempo    : " << ms << " ms\n";
}

void ejecutarConAtomico(int numHilos, long long tope) {
    std::atomic<long long> variable{0};
    std::vector<std::thread> hilos;
    hilos.reserve(static_cast<std::size_t>(numHilos));

    const auto inicio = std::chrono::steady_clock::now();
    for (int i = 0; i < numHilos; ++i) {
        hilos.emplace_back([&variable, tope]() {
            for (long long j = 0; j < tope; ++j) {
                variable.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& h : hilos) {
        h.join();
    }
    const auto fin = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(fin - inicio).count();

    const long long esperado = static_cast<long long>(numHilos) * tope;
    std::cout << "\n[std::atomic<long long>]\n"
              << "  Resultado : " << variable.load() << " (Esperado: " << esperado << ")\n"
              << "  Tiempo    : " << ms << " ms\n";
}

} // namespace

int main() {
    const long long tope = leerTope();
    ejecutarConMutex(NUM_HILOS, tope);
    ejecutarConAtomico(NUM_HILOS, tope);
    return 0;
}
