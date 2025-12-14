[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/eiNgq3fR)
[![Open in Visual Studio Code](https://classroom.github.com/assets/open-in-vscode-2e0aaae1b6195c2367325f4f02e2d04e9abb55f0b24a779b69b11b9e10269abc.svg)](https://classroom.github.com/online_ide?assignment_repo_id=22047133&assignment_repo_type=AssignmentRepo)

# Celda Braille Actualizable

# Integrantes
- David Esteban Becerra López  
- Anderson Camilo Rosero Yela  
- Laura Daniela Zambrano Guacheta  
- José Luis Pulido Fonseca  

Indice:
1. [Descripción](#descripción)
2. [Informe](#informe)
3. [Implementación](#implementación)
4. [Lista de anexos](#anexos)

## Descripción

Este repositorio contiene el diseño, implementación y validación técnica de una **celda Braille actualizable de 6 puntos** orientada a facilitar el acceso a la lectura para personas con discapacidad visual.

La solución propuesta usa una **FPGA Arty Z7-20** para controlar **seis servomotores (SG90)** organizados en la geometría estándar Braille (6 puntos). El sistema realiza la **conversión en tiempo real de caracteres ASCII a patrones Braille**, y activa/desactiva cada punto mediante **PWM a 50 Hz**. Incluye **entrada por UART USB y teclado matricial 4×4**, y **visualización en LCD 16×2 por I2C**. También utiliza **BRAM** como almacenamiento mínimo de carácter/patrón.

Resultados principales:
- Mapeo de **26 letras minúsculas (a–z)** a Braille de 6 puntos.
- Ciclo de entrada–procesamiento–actuación con **tiempo de respuesta < 2 s**.
- Arquitectura **modular y escalable** para futuras pantallas Braille multicelda.

## Informe

- [Informe (PDF)](Anexos/ProyectoFinalDig2.pdf)

## Implementación



- **Pruebas iniciales con solenoides:** [Video de funcionamiento del prototipo inicial](https://drive.google.com/file/d/1xmhHo_IDViwpNrGJnRIxHDrx-OpW_Uim/view)
- **Funcionamiento con servomotores:** [Video del sistema final con servos](https://drive.google.com/file/d/1_q-ypbL22XOVLf91ws4EtH3OgkULksdn/view?usp=sharing)
- **Demostración completa del proyecto:** [Funcionamiento integral del sistema](https://drive.google.com/file/d/1f3YQ6BSIZDHY-i7Hy-_6NWza2XDqnDq2/view?usp=sharing)



## Anexos

Imágenes incluidas en el repositorio:
- [b1.png](Anexos/b1.png)
- [DiseñoServo.png](Anexos/DiseñoServo.png)
- [masp.png](Anexos/masp.png)
- [Presupuesto.png](Anexos/Presupuesto.png)
- [ProcesadorenVivado.jpeg](Anexos/ProcesadorenVivado.jpeg)
- [Tinkercad.png](Anexos/Tinkercad.png)