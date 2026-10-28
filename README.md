# ☄️ Meteor Strike

## Presentación 
¡Os damos la bienvenida a **Meteor Strike**! Este proyecto es un juego de pinball que lleva la experiencia clásica de los arcades al espacio exterior. Hemos recreado la frenética y adictiva jugabilidad de una mesa tradicional, pero envuelta en una inmersiva temática interestelar.

## De qué trata el juego 
El objetivo es mantener la bola en juego el mayor tiempo posible dentro del límite de 4 minutos, afinando los reflejos con los flippers y conseguir la puntuación más alta. Para lograrlo, debes lanzar la bola a la zona de juego, usar los flippers para evitar que caiga por el desagüe inferior y apuntar a los distintos elementos del escenario para activar eventos y sumar puntos.

## Interfaz y Elementos en Pantalla
* Panel Izquierdo (Estado del Jugador): Muestra el logotipo del juego, la puntuación actual ("SCORE"), el tiempo transcurrido ("TIME"), el contador de bolas restantes ("NEXT BALL") y el estado del combo "B-O-N-U-S".
* Tablero Central (Zona de Juego): Ambientado en el espacio con un fondo galáctico. La bola, diseñada como un meteorito, interactúa con flippers rojos en la parte inferior y rebotadores amarillos que contienen las letras B, O, N, U, S. Al perder, se muestra una pantalla de "GAME OVER" con la puntuación final.
* Panel Derecho (Clasificación y Ajustes): Incluye una tabla de máximas puntuaciones ("HIGHSCORES") con el récord de puntos y tiempo, junto con un menú de configuración ("SETTINGS") en tiempo real.

## Controles y Modificación de Físicas
**Controles de juego**

* Iniciar / Jugar de nuevo: Pulsa la tecla Espacio para comenzar o reiniciar tras un Game Over.
* Flippers: Flechas laterales o A/D para accionar las paletas rojas inferiores.

**Ajustes de Simulación (Menú SETTINGS)**

* F1: Activa o desactiva el modo de depuración (DEBUG).
* F2: Limita la tasa de fotogramas por segundo (FPS LIMIT: 60).
* F3 / F4: Reduce o aumenta la gravedad de la simulación (GRAVITY).
* F5 / F6: Reduce o aumenta el rebote de los materiales (BOUNCE).

**Mecánicas de Juego**

* Gravedad y Rebote Dinámicos: A diferencia de un pinball estático, el usuario puede experimentar cómo la alteración de la gravedad o del rebote afecta drásticamente a la parábola, la aceleración y la pérdida de energía cinética del meteorito tras cada impacto.
* Objetivos y Combos: Impactar contra los rebotadores circulares amarillos permite encender las letras del combo (B-O-N-U-S) para incrementar la puntuación, alterando el vector de velocidad del meteorito tras colisiones elásticas.

**Características principales**
* **Físicas 2D realistas:** Implementación de rebotes, aceleración y colisiones fluidas garantizando un alto rendimiento.
* **Interfaz HUD completa:** Panel de control con seguimiento en tiempo real de la puntuación (Score), temporizador (Time) y bolas restantes (Next Ball).
* **Mecánicas de la mesa:** Obstáculos con multiplicadores de puntos, bumpers y flippers responsivos.
* **Sistema de clasificación:** Tabla de Highscores integrada que registra la puntuación y el tiempo exacto de los mejores jugadores.

**Grupo RealGamers**
* [Ian León]
* [Juncheng Ye]
* [Diyang Zhou]

## Tecnologías utilizadas
* **Lenguaje:** C++
* **Renderizado gráfico:** Raylib
* **Motor de físicas:** Box2D
