#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
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
 *   so-main/monitores/monitor00.cpp .. monitor04.cpp
 *
 * Correcciones críticas respecto a los originales:
 * - monitor01.cpp: tenía condición de carrera en `cont` (sin mutex) y fuga de memoria (new sin delete).
 * - monitor02.cpp: creaba un Monitor distinto por hilo, pero `cont` y `flag` seguían siendo globales
 *                  y `cout << cont` estaba fuera de la sección crítica.
 * - monitor03.cpp: bloqueaba el mutex fuera del monitor (rompía el patrón Monitor).
 * - monitor04.cpp: pasaba un puntero `Monitor* mo;` SIN INICIALIZAR -> Comportamiento Indefinido / Crash.
 *
 * Diseño aplicado:
 * 1. Encapsulación completa: `cont` y `mtx` son privados dentro de `MonitorContador`.
 * 2. Exclusión mutua automática mediante std::lock_guard / std::unique_lock (RAII).
 * 3. Variable de condición (`std::condition_variable`) para coordinar espera por umbral.
 * 4. Gestión segura de memoria con `std::shared_ptr` y objetos por valor (cero fugas de memoria).
 */

namespace {

constexpr int NUM_HILOS = 10;
constexpr int INCREMENTOS_POR_HILO = 10;

class MonitorContador {
private:
    int cont = 0;
    mutable std::mutex mtx;
    std::condition_variable cv;

public:
    void incrementarLote(int idHilo, int cantidad) {
        std::lock_guard<std::mutex> lock(mtx);
        for (int i = 0; i < cantidad; ++i) {
            ++cont;
        }
        std::cout << "[Hilo " << idHilo << "] incrementó en " << cantidad
                  << " -> cont = " << cont << '\n';
        cv.notify_all();
    }

    void esperarMeta(int objetivo) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this, objetivo]() { return cont >= objetivo; });
        std::cout << "[Observador] Meta alcanzada: cont = " << cont << '\n';
    }

    int obtenerValor() const {
        std::lock_guard<std::mutex> lock(mtx);
        return cont;
    }
};

class HiloTrabajador {
private:
    int id;
    std::shared_ptr<MonitorContador> monitor;
    std::thread t;

    void ejecutar() {
        monitor->incrementarLote(id, INCREMENTOS_POR_HILO);
    }

public:
    HiloTrabajador(int idHilo, std::shared_ptr<MonitorContador> mon)
        : id(idHilo), monitor(std::move(mon)), t(&HiloTrabajador::ejecutar, this) {}

    // Prohibimos copia pero permitimos movimiento seguro si se desea
    HiloTrabajador(const HiloTrabajador&) = delete;
    HiloTrabajador& operator=(const HiloTrabajador&) = delete;

    void unir() {
        if (t.joinable()) {
            t.join();
        }
    }

    ~HiloTrabajador() {
        unir();
    }
};

} // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    auto monitorCompartido = std::make_shared<MonitorContador>();
    const int metaEsperada = NUM_HILOS * INCREMENTOS_POR_HILO;

    // Hilo observador que demuestra el uso de std::condition_variable dentro del Monitor
    std::thread observador([&monitorCompartido, metaEsperada]() {
        monitorCompartido->esperarMeta(metaEsperada);
    });

    // std::unique_ptr evita fugas de memoria y evita que HiloTrabajador sea movido tras arrancar `this`
    std::vector<std::unique_ptr<HiloTrabajador>> hilos;
    hilos.reserve(NUM_HILOS);

    for (int i = 0; i < NUM_HILOS; ++i) {
        hilos.push_back(std::make_unique<HiloTrabajador>(i, monitorCompartido));
    }

    for (auto& h : hilos) {
        h->unir();
    }

    observador.join();

    std::cout << "Valor final garantizado dentro del Monitor: "
              << monitorCompartido->obtenerValor()
              << " (Esperado: " << metaEsperada << ")\n";
    return 0;
}
