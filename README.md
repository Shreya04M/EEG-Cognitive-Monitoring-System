# EEG-Cognitive-Monitoring-System
Real-Time EEG-Based Cognitive Monitoring System using Brain Wave Dominance Analysis

<img width="1407" height="768" alt="Algo_design" src="https://github.com/user-attachments/assets/b261cf3e-18b3-485f-ab6a-258123e182ea" />



## Overview

This project implements a **real-time EEG signal analysis system on ESP32**, using FFT-based frequency decomposition to estimate cognitive state.

Instead of using machine learning, the system relies on **band power ratios and temporal stabilization** to classify mental states and track attention changes over time.

The output is streamed over serial in both **CSV format (for logging)** and **interpretable diagnostics (for live monitoring)**.

---

## What It Actually Does

* Samples analog EEG signal at **250 Hz**
* Applies **exponential smoothing filter** (noise reduction)
* Performs **FFT (64 samples window)**
* Computes power across frequency bands:

  * Delta (0.5–4 Hz)
  * Theta (4–8 Hz)
  * Alpha (8–13 Hz)
  * Beta (13–30 Hz)
  * Gamma (30–45 Hz)
* Derives:

  * Focus index
  * Relaxation index
  * Drowsiness index
* Classifies state using **threshold + rolling window stabilization**
* Generates:

  * Focus score (0–100, baseline normalized)
  * Behavioral alerts (drowsy, inactive, attention change)

---

## Signal Processing Pipeline

Analog Input → Smoothing Filter → FFT → Magnitude Spectrum →
Band Power Aggregation → Ratio Computation → State Classification

---

## Core Design Choices

### 1. Filtering

A simple exponential filter is used:

* Low computational cost (ESP32 friendly)
* Reduces ADC noise before FFT

### 2. FFT Configuration

* Samples: 64
* Sampling Rate: 250 Hz
* Window: Hamming

This gives a usable frequency resolution for EEG bands without overloading memory.

---

### 3. Band Power Computation

Magnitude values from FFT bins are accumulated into predefined frequency ranges.

No normalization or PSD scaling is used — values are **relative**, not absolute.

---

### 4. Cognitive State Logic

State is derived from ratios:

* Focus → Beta dominant
* Relaxed → Alpha dominant
* Drowsy → Theta dominant

Raw classification is NOT used directly.

Instead:

* A **rolling window (size = 5)** is maintained
* State must remain consistent for multiple cycles before being accepted

This avoids flickering due to noise.

---

### 5. Focus Score

Focus score is computed as:

* Based on focus index (beta vs alpha+theta)
* Normalized using **baseline calibration (startup phase)**
* Clamped between 0–100

This makes the score adaptive per user/session.

---

### 6. Behavioral Detection

The system monitors trends over time:

* Drowsiness → sustained theta dominance (>10 sec)
* Inactivity → very low focus score (>15 sec)
* Attention trend:

  * Increasing
  * Decreasing

These are event-based, not instantaneous triggers.

---

## Output Format

### CSV Log (every 30 sec)

```
Time,AvgSignal,MinSignal,MaxSignal,Delta,Theta,Alpha,Beta,Gamma,State,FocusScore
```

### Detailed Analysis (every 2 min)

Includes:

* Signal stats
* Band powers
* Indices
* Interpreted state

---

## Hardware

* ESP32
* Analog EEG sensor (connected to GPIO 34)

---

## Dependencies

* ArduinoFFT library

---

## How to Run

1. Upload code to ESP32
2. Connect EEG output → GPIO 34
3. Open Serial Monitor (115200 baud)
4. Observe:

   * Continuous stream data
   * Periodic analysis blocks

---

## Calibration

At startup (~10 seconds):

* System computes baseline focus index
* Used to normalize focus score

Do not move or introduce noise during this phase.

---

## Limitations (Intentional)

* Single-channel EEG
* No ML / classification model
* No artifact removal (blink, motion)
* Relative power only (not medically accurate)

This is a **lightweight embedded system**, not a clinical tool.

---

## Why This Approach

This project avoids heavy ML pipelines and instead focuses on:

* Real-time performance on microcontroller
* Interpretable signal processing
* Low-latency feedback
* Minimal dependencies

---

## Possible Extensions

* Multi-channel EEG
* Band power normalization (PSD)
* ML-based classification (CNN)
* Real-time dashboard (Bluetooth / WiFi)
* Integration with wearable systems

---



