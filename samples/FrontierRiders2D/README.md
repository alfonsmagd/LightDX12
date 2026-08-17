# Frontier Riders 2D

Primera vertical slice de un arcade western 2D construida directamente sobre LightD3D12. No modifica el nucleo de la API: el juego vive por completo dentro de este ejemplo y usa un quad, texturas bindless y push constants para dibujar sprites, escenario y HUD.

## Controles

- `WASD`: mover al personaje.
- `Flechas`: apuntar y disparar en cuatro direcciones.
- `Espacio`: disparar en la ultima direccion usada.
- `R`: reiniciar la partida.
- `Escape`: salir.

El primer nivel recorre 5200 unidades de mundo. La camara usa una zona muerta horizontal para que el personaje pueda maniobrar sin que el escenario flote continuamente. La barra superior indica la distancia restante hasta la puerta del fuerte. Los enemigos necesitan dos impactos, mantienen una distancia de combate y el jugador dispone de tres vidas.

## Assets

`assets/cowboy_sheet.png` es la hoja entregada para el prototipo. Contiene cuatro poses para ocho direcciones, pero sus frames no respetan una cuadricula regular. Al cargarla, el ejemplo detecta las bandas reales, elimina el damero RGB y construye un atlas limpio de 4x8 con todos los pies anclados al mismo pixel. Una hoja futura con alfa real se detecta y se usa sin aplicar esa limpieza.

El jugador y los enemigos comparten temporalmente esta hoja; los enemigos reciben un tinte diferente. Los tres planos del escenario son arte provisional original generado para esta vertical slice. Los prompts exactos y el flujo de recorte estan en `assets/ART_PROMPTS.md`. Los proximos sprites pueden anadirse a `assets` y cargarse con `LoadPngRgba`, sin cambios en LightD3D12.

## Arquitectura

- Resolucion logica: 1280x720, escalada por el swapchain.
- Render 2D: seis vertices generados con `SV_VertexID`, sin vertex buffer.
- Sprites: descriptor bindless de textura y rectangulo UV por push constants.
- Pixel art: lectura con `Texture.Load`, por lo que no depende del sampler lineal global.
- Profundidad: fondo lejano, elementos ferroviarios y suelo se desplazan con tres velocidades de parallax.
- Escala 2.5D: la coordenada vertical de los pies controla el tamano del actor y el orden de dibujado.
- Gameplay: movimiento con aceleracion y frenado, disparos, encuentros colocados a mano, colisiones AABB, salud, vidas y puntuacion.
- Impactos: cada muerte genera un spray orientado por la bala; las gotas rotan con su trayectoria, chocan contra el suelo y producen manchas persistentes. El cuerpo cae en la direccion del impacto y permanece unos segundos.
- Fuente: atlas pequeno generado en CPU; el escenario y los personajes son texturas PNG.

## Organizacion del codigo

- `main.cpp`: punto de entrada de seis lineas.
- `App/WindowApplication.hpp/.cpp`: host reutilizable; es el unico sitio que conoce Win32, `DeviceManager`, swapchain, command buffers y presentacion.
- `FrontierApplication.hpp/.cpp`: adaptador pequeno del juego; lee input y delega en los callbacks de `FrontierRun`.
- `FrontierWorld.hpp`: datos puros del jugador y de todas las entidades.
- `FrontierEvents.hpp`: hechos transitorios de gameplay, sin dependencias de render o audio.
- `FrontierPhysics.hpp/.cpp`: simulacion dividida en jugador, enemigos, proyectiles y efectos.
- `AudioManager2D.hpp/.cpp`: manager de audio exclusivo del juego; mantiene un pool de voces y permite lanzar efectos sin gestionar su duracion desde gameplay.
- `FrontierRenderQueue.hpp/.cpp`: vectores de comandos para sprites, particulas y texto, mas el orden de envio del frame.
- `FrontierRenderer.hpp/.cpp`: manager unico accesible con `FrontierRenderer::Get()`; posee la cola, recopila escena/entidades/HUD y finalmente graba los comandos D3D12.
- `FrontierRun.hpp/.cpp`: fachada pequena que conecta mundo, fisica y render.

El juego no adquiere command buffers ni abre render passes. Su bucle se expresa mediante dos callbacks:

```cpp
run.Physics( deltaSeconds, input );
run.Render( commands );
```

`App::RunWindowApplication` se encarga alrededor de ellos de crear la ventana y el dispositivo, calcular el delta, procesar resize/minimizacion, adquirir el command buffer, abrir y cerrar el render pass, enviar el frame y presentar el swapchain. La fisica publica eventos de audio de un solo frame; `FrontierRun` los transforma en llamadas fire-and-forget al manager. Los disparos admiten solapamiento, panorama segun su posicion horizontal y atenuacion con la distancia.

El render usa dos fases. Primero `FrameRenderCollector` accede a la unica cola encapsulada por `FrontierRenderer` y registra sprites, particulas y texto; ninguna de esas funciones toca el command buffer ni recibe la cola como argumento. Despues `DrawRenderQueue` conserva el orden visual y convierte los vectores completos en `CmdPushConstants` y `CmdDraw`.
