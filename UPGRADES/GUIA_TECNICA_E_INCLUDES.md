# Guía Técnica Detallada, `#include`s y Análisis Archivo por Archivo

Este documento complementa a [README.md](./README.md) explicando:
1. Todos los encabezados (`#include`) utilizados en el repositorio original y en las mejoras de `UPGRADES/`.
2. Los conceptos clave de concurrencia y sistemas operativos presentes en el proyecto.
3. El funcionamiento detallado y diagnóstico de **cada archivo** (originales y mejorados).

---

## 1. Guía de `#include` utilizados (Originales y Mejorados)

### 1.1 Encabezados de C estándar y POSIX (usados en `so-main/` y versión POSIX)

| `#include` | Equivalente C++ | Qué aporta y por qué es relevante |
|---|---|---|
| `<stdio.h>` | `<cstdio>` | Entrada/salida clásica de C (`printf`, `scanf`, `fgets`, `fprintf`, `stdin`, `stdout`, `stderr`). En concurrencia, `printf` suele ser atómico por llamada en algunas implementaciones, pero mezclar lecturas/escrituras sin formato controlado sigue requiriendo cuidado. En `UPGRADES` se usa `fgets` + `strtoll` en la versión POSIX para evitar bloqueos o residuos en el búfer ante entradas inválidas. |
| `<stdlib.h>` | `<cstdlib>` | Utilidades generales: `rand()`, `exit()`, `strtoll()`, `EXIT_FAILURE`. **Importante:** `rand()` usa estado global interno y no es seguro entre hilos (*thread-safe*) en todas las plataformas; en `UPGRADES` se reemplaza por `std::mt19937` con `thread_local`. |
| `<assert.h>` | `<cassert>` | Macro `assert(condicion)` para detectar invariantes rotos durante desarrollo (por ejemplo, comprobar que `pthread_create` devolvió `0`). **Limitación:** desaparece si se compila con `-DNDEBUG` (modo Release). Por eso en `UPGRADES` se reemplaza por verificación explícita en tiempo de ejecución (`verificarPthread`). |
| `<pthread.h>` | `<thread>` / `<mutex>` | API estándar POSIX Threads (*pthreads*): `pthread_t`, `pthread_create`, `pthread_join`, `pthread_mutex_t`, `pthread_mutex_init`, `pthread_mutex_lock`, `pthread_mutex_unlock`, `pthread_mutex_destroy`. Es la base de bajo nivel sobre la que suelen implementarse los hilos en Linux/Unix y MinGW-winpthreads. |
| `<unistd.h>` | `<thread>` + `<chrono>` | Cabecera POSIX para llamadas del sistema Unix, usada en `pthread-example.cpp` únicamente para `sleep(segundos)`. **Limitación:** no es estándar en MSVC/Windows nativo y solo tiene granularidad de segundos enteros. En `UPGRADES` se sustituye por `std::this_thread::sleep_for(...)`. |
| `<cerrno>` / `<cstring>` | — | Usados en `exclusion_mutua_posix_upgraded.cpp`: `<cerrno>` provee `errno` para validar desbordamientos en `std::strtoll`, y `<cstring>` provee `std::strerror(codigo)` para traducir códigos de error POSIX a texto legible. |

### 1.2 Encabezados de C++ moderno (usados en `Completo_1`, `Completo_2` y `UPGRADES/`)

| `#include` | Qué aporta y por qué es relevante |
|---|---|
| `<iostream>` | Flujos estándar `std::cin`, `std::cout`, `std::cerr`. **Ojo en concurrencia:** expresiones encadenadas como `std::cout << "A" << x << "\n";` realizan múltiples llamadas `operator<<` y **no son atómicas** entre hilos, produciendo líneas entrelazadas si no se protegen con un `std::mutex`. |
| `<sstream>` | `std::ostringstream` permite construir el mensaje completo en memoria local del hilo antes de adquirir `coutMutex`, minimizando el tiempo que el mutex de consola permanece bloqueado. |
| `<iomanip>` | Manipuladores de formato como `std::fixed` y `std::setprecision(2)` para mostrar tiempos y porcentajes con dos decimales. |
| `<string>` | `std::string` reemplaza a los arreglos fijos `char[20]` (`filosofos01..03`) eliminando riesgos de desbordamiento de búfer. |
| `<limits>` | `std::numeric_limits<int>::max()` evita constantes mágicas como `2147483647` (`Completo_2`) y `std::numeric_limits<std::streamsize>::max()` limpia correctamente `std::cin` tras entradas no numéricas. |
| `<thread>` | `std::thread`, `std::this_thread::sleep_for`, `std::ref`, `std::cref`. Abstracción portable de hilos del estándar C++11+. |
| `<mutex>` | Primitivas de exclusión mutua: `std::mutex`, `std::lock_guard` (RAII simple), `std::unique_lock` (RAII flexible para `condition_variable` o `defer_lock`), `std::lock` y `std::scoped_lock` (C++17, bloquea múltiples mutex sin *deadlock*). |
| `<condition_variable>` | `std::condition_variable` permite que un hilo duerma eficientemente sin espera activa (*busy waiting*) hasta que otro hilo modifique el estado compartido y llame a `notify_one()` o `notify_all()`. Es indispensable para monitores reales y productor-consumidor. |
| `<atomic>` | `std::atomic<long long>` permite operaciones atómicas libres de cerrojos (*lock-free* cuando el hardware lo soporta) para contadores simples como en `exclusion_mutua_cpp_upgraded.cpp` o métricas en `productor_consumidor_upgraded.cpp`. |
| `<chrono>` | Manejo tipado de tiempo: `std::chrono::milliseconds`, `std::chrono::duration<double>` y `std::chrono::steady_clock` (reloj monotónico ideal para medir intervalos de ejecución). |
| `<random>` | Motor pseudoaleatorio `std::mt19937`, semilla no determinista `std::random_device` y distribuciones uniformes (`uniform_int_distribution`, `uniform_real_distribution`). Declarado como `thread_local`, cada hilo tiene su propio generador sin contención ni estado global compartido. |
| `<vector>` | Contenedor dinámico seguro en reemplazo de arreglos C de tamaño fijo (`tenedores(numFilos)`, `hilos`, `filosofos`). |
| `<queue>` | Cola FIFO `std::queue<T>` utilizada como estructura del búfer acotado en `productor_consumidor_upgraded.cpp`. |
| `<memory>` | Punteros inteligentes RAII (`std::shared_ptr`, `std::unique_ptr`) que garantizan liberación automática de memoria y evitan las fugas de `new` sin `delete` presentes en `monitores/` y `productores02.cpp`. |
| `<algorithm>` | Funciones utilitarias como `std::min` para limitar la porción de fideos tomada al saldo disponible. |

---

## 2. Conceptos más relevantes de concurrencia aplicados

### 2.1 Condición de carrera (*Race Condition*) y sección crítica
Cuando dos o más hilos leen y escriben la misma variable sin coordinación (`variable = variable + 1` en `exclusionMutua00/01` o `cont++` en `monitor01`), la operación no es atómica en CPU: se descompone en *cargar -> sumar -> guardar*. Si dos hilos leen el mismo valor simultáneamente, uno sobrescribe el incremento del otro y se pierden actualizaciones. La solución es envolver esa **sección crítica** con un `mutex` o usar `std::atomic`.

### 2.2 Exclusión mutua con RAII vs. `lock()`/`unlock()` manual
En `Completo_1` y `Completo_2` originales se usaba `mutex.lock()`/`mutex.unlock()` o `unique_lock` mal gestionados. En C++ moderno se prefiere **RAII** (*Resource Acquisition Is Initialization*):
- `std::lock_guard<std::mutex>`: adquiere el mutex al construirse y lo libera automáticamente al salir del ámbito (`}`), incluso si ocurre un `return` anticipado o una excepción.
- `std::scoped_lock(m1, m2)` (C++17): adquiere varios mutex simultáneamente utilizando un algoritmo libre de interbloqueo (*deadlock-free*) y los libera automáticamente al salir del ámbito.

### 2.3 El bug sutil de `Completo_2 Filosofos_Exclusión.cpp`
En el archivo original `Completo_2`, el método `tomarTenedores()` hacía:
```cpp
unique_lock<mutex> lockIzq(mutexTenedorIzq, defer_lock);
unique_lock<mutex> lockDer(mutexTenedorDer, defer_lock);
lock(lockIzq, lockDer);
```
Al terminar `tomarTenedores()`, `lockIzq` y `lockDer` salen de ámbito y **sus destructores desbloquean ambos tenedores inmediatamente antes de que el filósofo empiece a comer**. Luego `dejarTenedores()` volvía a llamar `mutexTenedorIzq.unlock()` sobre mutex que ya no eran propiedad del hilo, produciendo **Comportamiento Indefinido**. En `Completo_2_Filosofos_Exclusion_upgraded.cpp` esto se corrige manteniendo `std::scoped_lock` vivo durante toda la acción de tomar, comer y dejar.

### 2.4 El patrón Monitor real vs. Monitores incompletos (`monitor00`–`04`)
Un **Monitor** verdadero encapsula en una misma clase:
1. Los **datos compartidos** (privados, nunca globales).
2. Un **mutex interno** que garantiza exclusión mutua en todos sus métodos públicos.
3. **Variables de condición** (`std::condition_variable`) para esperar condiciones lógicas sin consumir CPU.

En `monitor00..04` el contador `cont` y el `mutex flag` eran globales, cada `Hilo` instanciaba su propio `Monitor` (o en `monitor04` recibía un puntero sin inicializar), y en `monitor03` se bloqueaba desde fuera del monitor. En `UPGRADES/monitores/monitor_upgraded.cpp` todo queda encapsulado dentro de `MonitorContador` y compartido de forma segura con `std::shared_ptr`.

### 2.5 Productor-Consumidor con cierre ordenado
Un sistema Productor-Consumidor real requiere un **búfer compartido de capacidad finita** y dos señales de sincronización:
- `cvNoLleno`: despierta a los productores cuando hay espacio libre en la cola.
- `cvNoVacio`: despierta a los consumidores cuando hay elementos disponibles.
- Bandera `cerrado` + `notify_all()`: permite que los consumidores procesen los elementos restantes y finalicen limpiamente cuando todos los productores terminan, evitando bloqueos eternos.

---

## 3. Explicación detallada archivo por archivo

### 3.1 Raíz del proyecto

#### `CMakeLists.txt` (original)
- Configura el proyecto en C++14 e incluye todos los 17 archivos `.cpp` dentro de `so-main/` en un único `add_executable(so_main_20240614T154818Z_001 ...)`.
- **Problema:** casi todos esos archivos definen su propia función `int main()`, por lo que compilar ese target completo produce errores de símbolo duplicado (`multiple definition of 'main'`).
- **Mejora (`UPGRADES/CMakeLists.txt`):** define una función auxiliar `add_upgraded_target` que crea **un ejecutable independiente por programa**, enlaza `Threads::Threads` y activa `-Wall -Wextra -Wpedantic` bajo C++17.

#### `Completo_1 Filosofos_Exclusion.cpp`
- **Qué hace:** Simula `numFilosofos` hilos compitiendo por un plato global de `unsigned int fideos = 256`. Cada filósofo piensa entre `1.0` y `10.0` segundos, toma una porción aleatoria (`1..50`) bajo `mutex.lock()`, actualiza su `estomago` y al finalizar muestra estadísticas (estómago máximo, porcentajes de tiempo pensando/comiendo y promedio).
- **Problemas detectados:**
  1. `while (fideos > 0)` e `if (fideos > 0)` leen `fideos` sin mutex (*data race*).
  2. `tiempoComiendo += cantidad` suma unidades de fideos como si fueran tiempo y no simula pausa de comida.
  3. Reconstruye `std::random_device` y `std::mt19937` en cada llamada.
  4. No protege `std::cout` y no valida `numFilosofos <= 0` (riesgo de división por cero).
- **Mejora (`UPGRADES/Completo_1_Filosofos_Exclusion_upgraded.cpp`):** Encapsula `fideos` y `mtx` en `PlatoCompartido`, usa `intentarComer()` protegido por `std::lock_guard`, mide duraciones reales en segundos, valida entradas y protege divisiones por cero.

#### `Completo_2 Filosofos_Exclusión.cpp`
- **Qué hace:** Amplía el simulador pidiendo al usuario `fide` (fideos) y `numFilos` (filósofos), agregando tenedores por pareja de filósofos vecinos (`tenedorIzq`, `tenedorDer`) además de `mutexFide`.
- **Problemas detectados:**
  1. Bug crítico de ciclo de vida de `unique_lock` en `tomarTenedores()` y `.unlock()` inválido en `dejarTenedores()`.
  2. `tenedores` fijo a 5 elementos (`vector<mutex> Filo::tenedores(5)` e `id % 5`): falla fuera de rango si `numFilos > 5`.
  3. `if (fide <= 0)` sin bloqueo de `mutexFide` y `cout` sin mutex.
  4. `LimitFideMax(int fide)` compara un `int` contra `2147483647` (`INT_MAX`), lo cual nunca se cumple porque el desbordamiento ocurre antes en `cin >> fide`.
- **Mejora (`UPGRADES/Completo_2_Filosofos_Exclusion_upgraded.cpp`):** Tenedores dinámicos de tamaño `numFilos`, exigencia de `numFilos >= 2`, bloqueo conjunto con `std::scoped_lock` durante todo el ciclo de comida, lectura segura con `long long` y estadísticas completas de tiempo pensando, tiempo comiendo y estómago.

---

### 3.2 Módulo `so-main/pthread/`

#### `pthread-example.cpp`
- **Qué hace:** Crea 5 hilos POSIX (`pthread_create`), cada uno duerme entre 1 y 5 segundos usando `rand()` y `sleep()`, y el hilo principal los espera con `pthread_join` verificando códigos mediante `assert(!result_code)`.
- **Mejora (`UPGRADES/pthread/pthread_example_upgraded.cpp`):** Migra a C++17 portable (`std::thread`, `std::chrono`), sustituye `rand()` por `thread_local std::mt19937`, protege `std::cout` con `std::mutex` y permite indicar la cantidad de hilos por línea de comandos.

---

### 3.3 Módulo `so-main/exclusion_mutua/`

#### `exclusionMutua00.cpp`
- **Qué hace:** Lanza 5 hilos que incrementan 5 veces cada uno la variable global `variable` (`variable = variable + 1`) **sin mutex**, imprimiendo en cada paso. Muestra cómo los hilos intercalan lecturas/escrituras sin sincronización.

#### `exclusionMutua01.cpp`
- **Qué hace:** Pide al usuario un `tope` de iteraciones por hilo (por ejemplo `100000`) y ejecuta los 5 hilos sin mutex ni `printf` dentro del bucle. Con valores altos de `tope`, el resultado final suele ser menor que `5 * tope` debido a la condición de carrera.

#### `exclusionMutua02.cpp`
- **Qué hace:** Añade `pthread_mutex_t flag;` inicializado con `pthread_mutex_init(&flag, NULL)`, protege `variable = variable + 1` con `pthread_mutex_lock`/`unlock`, y destruye el mutex al final. Garantiza `variable == 5 * tope`.
- **Mejoras en `UPGRADES/exclusion_mutua/`:**
  - `exclusion_mutua_posix_upgraded.cpp`: versión POSIX endurecida (comprobación de retorno en todas las funciones `pthread_*`, lectura robusta con `fgets` + `strtoll` y verificación final).
  - `exclusion_mutua_cpp_upgraded.cpp`: versión C++17 que compara rendimiento y exactitud entre `std::mutex` (`std::lock_guard`) y `std::atomic<long long>`.

---

### 3.4 Módulo `so-main/filosofos_comensales/`

- `filosofos00.cpp`: Programa inicial `"Hola mundo!"`.
- `filosofos01.cpp`: Ejecución **secuencial** de dos filósofos (`Sócrates` y `Pitágoras`), 10 iteraciones cada uno.
- `filosofos02.cpp`: Cambia el bucle a infinito (`for (int i=1; true; i++)`), por lo que `Pitágoras` nunca llega a ejecutarse al ser secuencial.
- `filosofos03.cpp`: Introduce paralelismo real con `pthread_create` para `Sócrates` y `Pitágoras`, permitiendo que ambos impriman concurrentemente (aunque en bucle infinito).
- `filosofos04.cpp`: Generaliza a `NUM_FILOSOFOS = 5` mediante arreglos `pthread_t filosofos[5]` e `int identificadores[5]`, todavía en bucle infinito y sin tenedores.
- **Mejora (`UPGRADES/filosofos_comensales/filosofos_basico_upgraded.cpp`):** Reemplaza el bucle infinito por 3 ciclos finitos, agrega 5 tenedores protegidos con `std::scoped_lock` (sin *deadlock*) y salida sincronizada.

---

### 3.5 Módulo `so-main/monitores/`

- `monitor00.cpp`: Define clases `Monitor` y `Hilo`, pero ejecuta `hilos[i]->run()` secuencialmente en un solo hilo (imprime `10, 20, ..., 100`). Además filtra memoria (`new Monitor`, `new Hilo` sin `delete`).
- `monitor01.cpp`: Añade `std::thread` dentro de `Hilo`, pero sin `mutex`. Introduce condición de carrera sobre `cont` global.
- `monitor02.cpp`: Agrega `mutex flag;` global usado dentro de `Monitor::inc()`. Corrige la carrera sobre los incrementos, pero `cout << cont` queda fuera de `flag` y cada `Hilo` crea un `Monitor` distinto.
- `monitor03.cpp`: Comenta el bloqueo dentro de `Monitor` y lo mueve a `Hilo::run_thread()` (marcado con `----ERROR` en el código original porque rompe la encapsulación del Monitor).
- `monitor04.cpp`: Cambia el constructor a `Hilo(Monitor* mon)` para compartir un solo monitor desde `main()`, pero declara `Monitor* mo;` **sin instanciarlo (`new Monitor`)**, lo que provoca comportamiento indefinido.
- **Mejora (`UPGRADES/monitores/monitor_upgraded.cpp`):** Implementa `MonitorContador` con estado y mutex privados, suma `std::condition_variable` con un hilo observador que espera la meta `100`, y gestiona la memoria con `std::shared_ptr` y `std::unique_ptr`.

---

### 3.6 Módulo `so-main/productor_consumidor/`

- `productores00.cpp`: Lanza 2 productores con `pthread` que generan letras aleatorias (`rand() % 26`) en bucle infinito y las imprimen. No tiene búfer ni consumidor.
- `productores01.cpp`: Reescribe `productores00.cpp` usando `std::thread` con función libre `productor(int k)`. Mantiene el bucle infinito y `rand()`.
- `productores02.cpp`: Encapsula el hilo dentro de una clase `Productor` instanciada con `new Productor(i)` (sin `delete`), manteniendo el bucle infinito y sin consumidor.
- **Mejora (`UPGRADES/productor_consumidor/productor_consumidor_upgraded.cpp`):** Construye un sistema Productor-Consumidor completo con `BufferAcotado<char>` (capacidad `5`), 2 productores, 2 consumidores, `std::condition_variable` (`cvNoLleno`, `cvNoVacio`), cierre ordenado `cerrar()` y verificación `totalProducido == totalConsumido`.
