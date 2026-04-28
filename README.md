# ⚡ PMSM Hardware-in-the-Loop (HIL) with STM32, MATLAB, and Simulink

# Hi, I'm building a PMSM HIL project

### Real-time control of a Permanent Magnet Synchronous Motor using an STM32 controller and a MATLAB/Simulink plant model

### Focused on Embedded Control, Motor Control, FOC, HIL, and Simulation-Based Development

## 📌 Project Overview

This project implements a **Hardware-in-the-Loop (HIL)** framework for **PMSM control** using:

- **STM32** as the embedded controller
- **MATLAB / Simulink** as the motor plant simulation environment
- **UART communication** between the microcontroller and the PC
- **Field-Oriented Control (FOC)** concepts for motor control development and testing

The goal is to validate embedded motor-control algorithms on real hardware while keeping the motor and inverter model in simulation

---

## 🚀 Features

- PMSM plant simulation in **MATLAB / Simulink**
- Embedded controller implementation on **STM32**
- UART-based data exchange between STM32 and MATLAB
- HIL-ready architecture for testing control logic without physical motor hardware
- Support for:
  - speed control
  - current control
  - duty-cycle generation
  - electrical angle feedback
  - torque and speed observation
- Modular structure for future migration to faster binary protocols or DMA-based communication

---

## 🧠 Control Concept

The project is based on a **PMSM control loop** where:

- MATLAB / Simulink computes the plant dynamics
- STM32 receives measured signals from the simulated plant
- STM32 computes the control output
- MATLAB applies the control action back to the PMSM model

Typical exchanged signals include:

- reference speed
- mechanical speed
- phase currents
- electrical speed
- electrical angle
- DC bus voltage
- duty cycles

---

## 🏗️ System Architecture

```text
Reference Speed
      |
      v
+-------------------+
|   MATLAB/Simulink |
|   PMSM Plant      |
+-------------------+
      | measured signals
      v
+-------------------+
|      STM32        |
| Embedded Control  |
+-------------------+
      | duty cycles / control outputs
      v
+-------------------+
|   MATLAB/Simulink |
| applies control   |
+-------------------+
