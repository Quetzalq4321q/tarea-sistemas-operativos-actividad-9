# UPGRADES — Análisis y Versiones Mejoradas (`so-main-20240614T154818Z-001`)

Este directorio contiene la documentación completa y las versiones corregidas/modernizadas (C++17 y POSIX seguro) de todos los programas del repositorio de **Sistemas Operativos: Concurrencia y Sincronización**.

> Para el desglose línea por línea de cada archivo original, el propósito de todos los `#include` y los conceptos teóricos clave, consulta **[GUIA_TECNICA_E_INCLUDES.md](./GUIA_TECNICA_E_INCLUDES.md)**.

---

## 1. Funcionamiento general del proyecto original

El repositorio original está organizado como una secuencia didáctica progresiva para enseñar programación concurrente en C y C++:

1. **Hilos básicos con POSIX (`so-main/pthread/pthread-example.cpp`)**:
   Introducción a la creación (`pthread_create`) y sincronización de finalización (`pthread_join`) de múltiples hilos en C/POSIX.
2. **Filósofos comensales básicos (`so-main/filosofos_comensales/filosofos00.cpp` – `04.cpp`)**:
   Evolución incremental desde un `"Hola mundo!"` (`00`), llamadas secuenciales finitas (`01`), bucles infinitos secuenciales (`02`), dos hilos nombrados con `pthread` (`03`), hasta 5 hilos indexados (`04`) sin sincronización ni tenedores todavía.
3. **Exclusión mutua (`so-main/exclusion_mutua/exclusionMutua00.cpp` – `02.cpp`)**:
   Demuestra primero una **condición de carrera** (*race condition*) sobre un contador global `variable` (`00` y `01`), y luego la soluciona protegiendo la sección crítica con `pthread_mutex_t` (`02`).
4. **Monitores (`so-main/monitores/monitor00.cpp` – `04.cpp`)**:
   Intenta encapsular el acceso concurrente a `cont` en una clase `Monitor` usando `std::thread` y `std::mutex`. Va mostrando errores típicos de diseño: ejecución secuencial (`00`), hilos sin mutex (`01`), mutex dentro de `Monitor` con instancia duplicada por hilo (`02`), bloqueo puesto fuera del monitor en `Hilo` (`03`), y un intento de compartir un único `Monitor*` (`04`) que deja el puntero sin inicializar.
5. **Productor-Consumidor (`so-main/productor_consumidor/productores00.cpp` – `02.cpp`)**:
   Muestra la migración sintáctica de `pthread` (`00`) a `std::thread` funcional (`01`) y a `std::thread` encapsulado en clase (`02`), aunque solo implementa productores en bucle infinito sin búfer ni consumidores.
6. **Simuladores integradores en la raíz**:
   - `Completo_1 Filosofos_Exclusion.cpp`: Múltiples filósofos (`std::thread`) comparten un plato de `256` fideos protegido por `std::mutex` y calculan estadísticas de estómago y tiempos.
   - `Completo_2 Filosofos_Exclusión.cpp`: Añade la simulación de **tenedores compartidos** en la mesa circular además del mutex del plato de fideos.

---

## 2. Mapa entre archivos originales y mejoras en `UPGRADES/`

| Módulo / Archivo original | Archivo mejorado en `UPGRADES/` | Mejoras principales |
|---|---|---|
| `so-main/pthread/pthread-example.cpp` | `pthread/pthread_example_upgraded.cpp` | `std::thread`, `thread_local std::mt19937`, `std::chrono` portable, salida protegida por mutex. |
| `so-main/exclusion_mutua/exclusionMutua00.cpp`..`02.cpp` | `exclusion_mutua/exclusion_mutua_posix_upgraded.cpp` | POSIX `pthread_mutex_t` con verificación de errores, lectura segura (`fgets`/`strtoll`) y validación contra valor esperado. |
| `so-main/exclusion_mutua/exclusionMutua00.cpp`..`02.cpp` | `exclusion_mutua/exclusion_mutua_cpp_upgraded.cpp` | Comparativa moderna entre `std::mutex` (`std::lock_guard`) y `std::atomic<long long>` con medición de tiempo. |
| `so-main/filosofos_comensales/filosofos00.cpp`..`04.cpp` | `filosofos_comensales/filosofos_basico_upgraded.cpp` | Hilos reales con ciclos acotados, tenedores reales sin *deadlock* (`std::scoped_lock`) y consola sincronizada. |
| `so-main/monitores/monitor00.cpp`..`04.cpp` | `monitores/monitor_upgraded.cpp` | Monitor verdadero (estado y mutex privados + `std::condition_variable`), `std::shared_ptr`/`std::unique_ptr` (corrige el puntero nulo de `monitor04.cpp` y las fugas de memoria). |
| `so-main/productor_consumidor/productores00.cpp`..`02.cpp` | `productor_consumidor/productor_consumidor_upgraded.cpp` | Patrón completo Productor-Consumidor con búfer acotado, `condition_variable` (`cvNoLleno`, `cvNoVacio`) y cierre limpio sin bucles infinitos. |
| `Completo_1 Filosofos_Exclusion.cpp` | `Completo_1_Filosofos_Exclusion_upgraded.cpp` | Elimina condición de carrera en `while (fideos > 0)`, usa RAII (`std::lock_guard`), separa cantidad de tiempo real, valida entradas y protege divisiones por cero. |
| `Completo_2 Filosofos_Exclusión.cpp` | `Completo_2_Filosofos_Exclusion_upgraded.cpp` | Corrige el **bug de liberación prematura de `unique_lock` + doble `unlock()`**, dimensiona dinámicamente los tenedores según `numFilos`, elimina *data races* y valida `INT_MAX` con `long long`. |
| [CMakeLists.txt](../CMakeLists.txt) (raíz) y [UPGRADES/CMakeLists.txt](./CMakeLists.txt) | Proyectos independientes | El `CMakeLists.txt` raíz ahora asigna cada `.cpp` (los 17 de `so-main/`, `Completo_1`, `Completo_2` y los 8 de `UPGRADES/` vía `add_subdirectory(UPGRADES)`) a su propio `add_executable(...)` independiente. |

---

## 3. Problemas críticos detectados en el código original

1. **Puntero sin inicializar en `monitor04.cpp`**: `Monitor* mo;` se pasa a `new Hilo(mo)` sin haber creado un `new Monitor()`, causando comportamiento indefinido (*Undefined Behavior* / fallo de segmentación).
2. **Error grave de ciclo de vida de `unique_lock` en `Completo_2 Filosofos_Exclusión.cpp`**: `tomarTenedores()` bloqueaba `lockIzq` y `lockDer` como variables locales que se destruían (y desbloqueaban) al retornar el método; luego `dejarTenedores()` llamaba manualmente a `.unlock()` sobre mutex ya desbloqueados.
3. **Desbordamiento de arreglo en `Completo_2 Filosofos_Exclusión.cpp`**: `tenedores` tenía tamaño fijo `5` (`id % 5`), pero el usuario podía ingresar cualquier `numFilos`, provocando accesos fuera de límites si `numFilos > 5`.
4. **Condiciones de carrera en lecturas de `fideos` / `fide` y `std::cout`**: Tanto `Completo_1` como `Completo_2` leían la variable compartida en la condición del bucle sin mutex y escribían en `std::cout` concurrentemente.
5. **Validación inútil de desbordamiento (`LimitFideMax`)**: Comprobaba `fide > 2147483647` sobre un `int` de 32 bits, donde el desbordamiento ocurre antes durante `cin >> fide`.
6. **Fugas de memoria y uso de `rand()`**: Múltiples `new` sin `delete` en `monitores/` y `productores02.cpp`, y llamadas a `rand()` (no garantizado como *thread-safe*) desde múltiples hilos.
7. **`CMakeLists.txt` raíz original no enlazable**: Originalmente incluía los 17 archivos de `so-main/` dentro de un único `add_executable(...)`, lo cual generaba error de enlace por definición múltiple de `main`. Ya fue corregido en [CMakeLists.txt](../CMakeLists.txt) para que cada `.cpp` sea un ejecutable independiente.

---

## 4. Cómo compilar y ejecutar los programas de `UPGRADES/`

### Opción A: Con CMake y Ninja (herramientas incluidas en CLion)

Desde la carpeta raíz `C:\Users\User\Downloads\so-main-20240614T154818Z-001`:

```powershell
$cmake = "C:\Users\User\AppData\Local\Programs\CLion\bin\cmake\win\x64\bin\cmake.exe"
$ninja = "C:\Users\User\AppData\Local\Programs\CLion\bin\ninja\win\x64\ninja.exe"
$cxx   = "C:\Users\User\AppData\Local\Programs\CLion\bin\mingw\bin\g++.exe"

& $cmake -S UPGRADES -B UPGRADES/build -G Ninja -DCMAKE_MAKE_PROGRAM="$ninja" -DCMAKE_CXX_COMPILER="$cxx"
& $cmake --build UPGRADES/build
```

Para ejecutar cualquiera de los binarios compilados desde PowerShell (asegurando las DLLs de MinGW en el `PATH`):

```powershell
$env:PATH = "C:\Users\User\AppData\Local\Programs\CLion\bin\mingw\bin;" + $env:PATH
.\UPGRADES\build\pthread_example_upgraded.exe
.\UPGRADES\build\exclusion_mutua_posix_upgraded.exe
.\UPGRADES\build\exclusion_mutua_cpp_upgraded.exe
.\UPGRADES\build\filosofos_basico_upgraded.exe
.\UPGRADES\build\monitor_upgraded.exe
.\UPGRADES\build\productor_consumidor_upgraded.exe
.\UPGRADES\build\completo_1_upgraded.exe
.\UPGRADES\build\completo_2_upgraded.exe
```

### Opción B: Compilación individual con `g++`

```powershell
$gpp = "C:\Users\User\AppData\Local\Programs\CLion\bin\mingw\bin\g++.exe"
& $gpp -std=c++17 -Wall -Wextra -Wpedantic -pthread UPGRADES/Completo_2_Filosofos_Exclusion_upgraded.cpp -o completo_2.exe
```
