#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

/**
 * Versión mejorada de:
 *   so-main/productor_consumidor/productores00.cpp .. productores02.cpp
 *
 * Mejoras aplicadas respecto a los originales:
 * 1. Los originales solo tenían productores en bucle infinito usando rand() (inseguro en hilos)
 *    y sin búfer ni consumidores. Aquí se implementa el patrón completo Productor-Consumidor.
 * 2. Búfer acotado (`BufferAcotado`) implementado como Monitor con capacidad fija,
 *    std::mutex y dos std::condition_variable (`cvNoLleno` y `cvNoVacio`).
 * 3. Cierre ordenado (`cerrar()`) para que los consumidores vacíen el búfer y terminen sin colgarse.
 * 4. Uso de `thread_local std::mt19937` en lugar de `rand()`, y cero fugas de memoria.
 */

namespace {

constexpr int NUM_PRODUCTORES = 2;
constexpr int NUM_CONSUMIDORES = 2;
constexpr std::size_t CAPACIDAD_BUFFER = 5;
constexpr int ITEMS_POR_PRODUCTOR = 8;

std::mutex coutMutex;

void imprimirSeguro(const std::string& msg) {
    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << msg << '\n';
}

char generarLetraAleatoria() {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 25);
    return static_cast<char>('a' + dist(gen));
}

template <typename T>
class BufferAcotado {
private:
    std::queue<T> cola;
    std::size_t capacidad;
    bool cerrado = false;
    std::mutex mtx;
    std::condition_variable cvNoLleno;
    std::condition_variable cvNoVacio;

public:
    explicit BufferAcotado(std::size_t cap) : capacidad(cap) {}

    bool producir(T item) {
        std::unique_lock<std::mutex> lock(mtx);
        cvNoLleno.wait(lock, [this]() { return cola.size() < capacidad || cerrado; });
        if (cerrado) {
            return false;
        }
        cola.push(item);
        cvNoVacio.notify_one();
        return true;
    }

    bool consumir(T& itemSalida) {
        std::unique_lock<std::mutex> lock(mtx);
        cvNoVacio.wait(lock, [this]() { return !cola.empty() || cerrado; });
        if (cola.empty() && cerrado) {
            return false;
        }
        itemSalida = cola.front();
        cola.pop();
        cvNoLleno.notify_one();
        return true;
    }

    void cerrar() {
        std::lock_guard<std::mutex> lock(mtx);
        cerrado = true;
        cvNoLleno.notify_all();
        cvNoVacio.notify_all();
    }
};

void tareaProductor(int id,
                    BufferAcotado<char>& buffer,
                    std::atomic<int>& totalProducido) {
    for (int i = 1; i <= ITEMS_POR_PRODUCTOR; ++i) {
        const char letra = generarLetraAleatoria();
        if (!buffer.producir(letra)) {
            break;
        }
        ++totalProducido;

        std::ostringstream oss;
        oss << "[Productor-" << id << "] (" << i << "/" << ITEMS_POR_PRODUCTOR
            << ") produjo: " << letra;
        imprimirSeguro(oss.str());

        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
}

void tareaConsumidor(int id,
                     BufferAcotado<char>& buffer,
                     std::atomic<int>& totalConsumido) {
    char letra{};
    int conteoLocal = 0;
    while (buffer.consumir(letra)) {
        ++conteoLocal;
        ++totalConsumido;

        std::ostringstream oss;
        oss << "  [Consumidor-" << id << "] (#" << conteoLocal
            << ") consumió: " << letra;
        imprimirSeguro(oss.str());

        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }
}

} // namespace

int main() {
    BufferAcotado<char> buffer(CAPACIDAD_BUFFER);
    std::atomic<int> totalProducido{0};
    std::atomic<int> totalConsumido{0};

    std::vector<std::thread> productores;
    std::vector<std::thread> consumidores;

    for (int i = 0; i < NUM_PRODUCTORES; ++i) {
        productores.emplace_back(tareaProductor, i, std::ref(buffer), std::ref(totalProducido));
    }
    for (int i = 0; i < NUM_CONSUMIDORES; ++i) {
        consumidores.emplace_back(tareaConsumidor, i, std::ref(buffer), std::ref(totalConsumido));
    }

    for (auto& p : productores) {
        p.join();
    }

    // Se cierra el búfer una vez que todos los productores finalizaron
    buffer.cerrar();

    for (auto& c : consumidores) {
        c.join();
    }

    std::cout << "\nResumen Productor-Consumidor:\n"
              << "  Total producido : " << totalProducido.load() << '\n'
              << "  Total consumido : " << totalConsumido.load() << '\n';

    return 0;
}
