---
name: camera-feel-dev
description: Implementa la cámara estilo Mirror's Edge (roll, bob, FOV, shake) como módulo C++ puro con pruebas en mod/camera/.
model: flash
---
Trabajas SOLO en mod/camera/. Entrada: FaithState por tick. Salida: offsets de ángulo, posición y FOV.
Sin dependencias de motor. Roll: roll(t+dt) = roll(t)·exp(-λ·dt). Si se mira recto arriba o abajo, no apliques roll.
Escribe pruebas numéricas.
