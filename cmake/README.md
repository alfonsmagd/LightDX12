# Organización de CMake

El `CMakeLists.txt` raíz declara las opciones públicas y el orden de configuración.
Los módulos se incluyen desde la raíz: sus rutas `CMAKE_CURRENT_SOURCE_DIR` y
`CMAKE_CURRENT_BINARY_DIR` siguen apuntando al proyecto Ldx12, incluso con `add_subdirectory`.

| Módulo | Responsabilidad |
| --- | --- |
| `Ldx12Library.cmake` | Biblioteca principal, fuentes y shader generado |
| `Ldx12BuildHelpers.cmake` | Opciones de compilación y registro de ejemplos |
| `Ldx12App.cmake` | DearImGui, integración Ldx12ImGui y App |
| `Ldx12Install.cmake` | Instalación, exportación y paquete find_package |
| `Ldx12IDE.cmake` | Carpetas y agrupación de fuentes en Visual Studio |

## Añadir un ejemplo

Cada ejemplo declara su target en su propio `samples/<Ejemplo>/CMakeLists.txt`.
`main.cpp` se incluye automáticamente; las demás fuentes son relativas al directorio
del ejemplo. No se buscan archivos con glob.

```cmake
ldx12_add_example(20_MiEjemplo MiEjemplo
    SOURCES README.md shaders/MiEjemplo.hlsl
    LIBRARIES Ldx12::Ldx12 Ldx12::Utils)
```

Esto crea el target `20_MiEjemplo`, el ejecutable `MiEjemplo` y la etiqueta
`20. MiEjemplo` en la carpeta `examples`. `FOLDER` y `LABEL` permiten personalizar
la presentación. `ROOT_ASSETS` admite rutas relativas a la raíz del repositorio.
Los shaders se muestran en el IDE pero se compilan en runtime, igual que antes.

## Opciones

Se mantienen las opciones y valores por defecto de la raíz. Los ejemplos necesitan
Utils y la integración ImGui; App sigue siendo opcional. La opción
`LDX12_BUILD_UTILS` determina además si Utils se incluye en el paquete instalado.
DesktopRetroOverlay conserva su opción independiente. Los tests se declaran en
`tests/CMakeLists.txt`; la lista del núcleo está en `tests/Ldx12/CMakeLists.txt`.
Los módulos nuevos están cubiertos por el filtro `cmake/*` de CI.
