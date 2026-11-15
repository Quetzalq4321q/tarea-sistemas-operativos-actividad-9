#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

/**
 * Versión mejorada (POSIX) de:
 *   so-main/exclusion_mutua/exclusionMutua00.cpp
 *   so-main/exclusion_mutua/exclusionMutua01.cpp
 *   so-main/exclusion_mutua/exclusionMutua02.cpp
 *
 * Mejoras aplicadas:
 * 1. Exclusión mutua garantizada mediante pthread_mutex_t.
 * 2. Validación de la entrada del usuario (tope > 0) y limpieza de búfer stdin.
 * 3. Comprobación explícita de códigos de retorno de todas las llamadas pthread_*.
 * 4. Verificación final del valor esperado vs. el valor real obtenido.
 */

namespace {

constexpr int NUM_FILOSOFOS = 5;

struct ContextoCompartido {
    long long variable = 0;
    long long tope = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
};

struct ArgumentoHilo {
    int id = 0;
    ContextoCompartido* ctx = nullptr;
};

void verificarPthread(int codigo, const char* operacion) {
    if (codigo != 0) {
        std::fprintf(stderr, "Error en %s: %s\n", operacion, std::strerror(codigo));
        std::exit(EXIT_FAILURE);
    }
}

void* tareaFilosofo(void* arg) {
    auto* datos = static_cast<ArgumentoHilo*>(arg);
    ContextoCompartido* ctx = datos->ctx;

    for (long long i = 1; i <= ctx->tope; ++i) {
        verificarPthread(pthread_mutex_lock(&ctx->mutex), "pthread_mutex_lock");
        ctx->variable += 1;
        verificarPthread(pthread_mutex_unlock(&ctx->mutex), "pthread_mutex_unlock");
    }

    return nullptr;
}

long long leerTopeValido() {
    char buffer[128]{};
    while (true) {
        std::printf("Ingrese tope (iteraciones por filósofo, > 0): ");
        std::fflush(stdout);
        if (!std::fgets(buffer, sizeof(buffer), stdin)) {
            std::fprintf(stderr, "\nFin de entrada (EOF) detectado. Abortando.\n");
            std::exit(EXIT_FAILURE);
        }
        char* fin = nullptr;
        errno = 0;
        const long long tope = std::strtoll(buffer, &fin, 10);
        const bool huboDigitos = (fin != nullptr && fin != buffer);
        while (fin && (*fin == ' ' || *fin == '\t' || *fin == '\r' || *fin == '\n')) {
            ++fin;
        }
        if (errno == 0 && huboDigitos && *fin == '\0' && tope > 0) {
            return tope;
        }
        std::printf("Entrada inválida. Intente con un entero positivo.\n");
    }
}

} // namespace

int main() {
    ContextoCompartido ctx;
    ctx.tope = leerTopeValido();

    std::printf("Valor inicial de variable = %lld\n", ctx.variable);

    verificarPthread(pthread_mutex_init(&ctx.mutex, nullptr), "pthread_mutex_init");

    pthread_t filosofos[NUM_FILOSOFOS]{};
    ArgumentoHilo args[NUM_FILOSOFOS]{};

    for (int i = 0; i < NUM_FILOSOFOS; ++i) {
        args[i] = ArgumentoHilo{i, &ctx};
        verificarPthread(
            pthread_create(&filosofos[i], nullptr, &tareaFilosofo, &args[i]),
            "pthread_create"
        );
    }

    for (int i = 0; i < NUM_FILOSOFOS; ++i) {
        verificarPthread(pthread_join(filosofos[i], nullptr), "pthread_join");
    }

    verificarPthread(pthread_mutex_destroy(&ctx.mutex), "pthread_mutex_destroy");

    const long long esperado = static_cast<long long>(NUM_FILOSOFOS) * ctx.tope;
    std::printf("Valor final de variable   = %lld\n", ctx.variable);
    std::printf("Valor esperado            = %lld\n", esperado);
    std::printf("Estado de integridad      = %s\n",
                (ctx.variable == esperado) ? "CORRECTO (sin condicion de carrera)" : "ERROR");

    return 0;
}
