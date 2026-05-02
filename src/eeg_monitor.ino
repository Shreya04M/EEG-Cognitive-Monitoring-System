#include <Arduino.h>
#include <arduinoFFT.h>

#define SAMPLES 64
#define SAMPLING_FREQUENCY 250
#define SENSOR_PIN 34
#define WINDOW_SIZE 5
#define CALIBRATION_TIME 10

// -------- FFT --------
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY);

// -------- SIGNAL --------
double filteredValue = 0;
double alphaFilter = 0.1;

double avgSignal = 0;
double minSignal = 0;
double maxSignal = 0;

// -------- BAND STRUCT --------
struct Band {
  double low, high, power;
};

Band delta = {0.5, 4, 0};
Band theta = {4, 8, 0};
Band alpha = {8, 13, 0};
Band beta  = {13, 30, 0};
Band gammaBand = {30, 45, 0};

// -------- STATE --------
double focusHistory[WINDOW_SIZE] = {0};
int indexHistory = 0;
String lastState = "None";
int stableCount = 0;
double baselineFocus = 1;

// -------- TIMING --------
unsigned long lastLogTime = 0;
unsigned long lastAnalysisTime = 0;

#define LOG_INTERVAL 30000        // 30 sec
#define ANALYSIS_INTERVAL 120000  // 2 min

// -------- BEHAVIOR --------
unsigned long drowsyStart = 0;
unsigned long inactiveStart = 0;
bool drowsyFlag = false;
bool inactiveFlag = false;
int prevFocus = 0;

// -------- SETUP --------
void setup() {
  Serial.begin(115200);

  Serial.println("Time,AvgSignal,MinSignal,MaxSignal,Delta,Theta,Alpha,Beta,Gamma,State,FocusScore");

  calibrateBaseline();
}

// -------- LOOP --------
void loop() {

  readSignal();
  performFFT();
  calculateBandPower();

  String state = analyzeState();
  int focusScore = calculateFocusScore();

  unsigned long currentTime = millis();

  // -------- CSV LOG (30 sec) --------
  if (currentTime - lastLogTime >= LOG_INTERVAL) {

    Serial.print(currentTime); Serial.print(",");
    Serial.print(avgSignal); Serial.print(",");
    Serial.print(minSignal); Serial.print(",");
    Serial.print(maxSignal); Serial.print(",");
    Serial.print(delta.power); Serial.print(",");
    Serial.print(theta.power); Serial.print(",");
    Serial.print(alpha.power); Serial.print(",");
    Serial.print(beta.power); Serial.print(",");
    Serial.print(gammaBand.power); Serial.print(",");
    Serial.print(state); Serial.print(",");
    Serial.println(focusScore);

    lastLogTime = currentTime;
  }

  // -------- ANALYSIS (2 min) --------
  if (currentTime - lastAnalysisTime >= ANALYSIS_INTERVAL) {

    double focusIndex = beta.power / (alpha.power + theta.power + 1);
    double relaxIndex = alpha.power / (beta.power + 1);
    double drowsyIndex = theta.power / (beta.power + 1);

    printDetailedAnalysis(state, focusScore, focusIndex, relaxIndex, drowsyIndex);
    behaviorAnalysis(state, focusScore);

    lastAnalysisTime = currentTime;
  }
}

// -------- SIGNAL --------
void readSignal() {

  double rawSum = 0;
  minSignal = 4096;
  maxSignal = 0;

  for (int i = 0; i < SAMPLES; i++) {

    double raw = analogRead(SENSOR_PIN);

    rawSum += raw;

    if (raw < minSignal) minSignal = raw;
    if (raw > maxSignal) maxSignal = raw;

    filteredValue = alphaFilter * raw + (1 - alphaFilter) * filteredValue;

    vReal[i] = filteredValue;
    vImag[i] = 0;

    delayMicroseconds(1000000 / SAMPLING_FREQUENCY);
  }

  avgSignal = rawSum / SAMPLES;
}

// -------- FFT --------
void performFFT() {
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();
}

// -------- BAND POWER --------
void calculateBandPower() {
  delta.power = theta.power = alpha.power = beta.power = gammaBand.power = 0;

  for (int i = 1; i < SAMPLES / 2; i++) {
    double freq = (i * SAMPLING_FREQUENCY) / SAMPLES;
    double mag = vReal[i];

    if (freq >= delta.low && freq < delta.high) delta.power += mag;
    else if (freq >= theta.low && freq < theta.high) theta.power += mag;
    else if (freq >= alpha.low && freq < alpha.high) alpha.power += mag;
    else if (freq >= beta.low && freq < beta.high) beta.power += mag;
    else if (freq >= gammaBand.low && freq < gammaBand.high) gammaBand.power += mag;
  }
}

// -------- STATE --------
String analyzeState() {

  double focusIndex = beta.power / (alpha.power + theta.power + 1);
  double relaxIndex = alpha.power / (beta.power + 1);
  double drowsyIndex = theta.power / (beta.power + 1);

  focusHistory[indexHistory] = focusIndex;
  indexHistory = (indexHistory + 1) % WINDOW_SIZE;

  double avgFocus = 0;
  for (int i = 0; i < WINDOW_SIZE; i++) avgFocus += focusHistory[i];
  avgFocus /= WINDOW_SIZE;

  String currentState;

  if (avgFocus > 1.5) currentState = "Focused";
  else if (relaxIndex > 1.2) currentState = "Relaxed";
  else if (drowsyIndex > 1.3) currentState = "Drowsy";
  else currentState = "Neutral";

  if (currentState == lastState) stableCount++;
  else stableCount = 0;

  lastState = currentState;

  return (stableCount >= 3) ? currentState : "Analyzing";
}

// -------- FOCUS SCORE --------
int calculateFocusScore() {
  double focusIndex = beta.power / (alpha.power + theta.power + 1);
  int score = (focusIndex / baselineFocus) * 50;

  if (score > 100) score = 100;
  if (score < 0) score = 0;

  return score;
}

// -------- BEHAVIOR --------
void behaviorAnalysis(String state, int focusScore) {

  if (state == "Drowsy") {
    if (!drowsyFlag) {
      drowsyStart = millis();
      drowsyFlag = true;
    }
    if (millis() - drowsyStart > 10000) {
      Serial.println("⚠️ DROWSY DETECTED");
    }
  } else drowsyFlag = false;

  if (focusScore < 10) {
    if (!inactiveFlag) {
      inactiveStart = millis();
      inactiveFlag = true;
    }
    if (millis() - inactiveStart > 15000) {
      Serial.println("🚨 INACTIVE STATE");
    }
  } else inactiveFlag = false;

  if (focusScore > prevFocus + 10)
    Serial.println("📈 Attention Increasing");
  else if (focusScore < prevFocus - 10)
    Serial.println("📉 Attention Decreasing");

  prevFocus = focusScore;
}

// -------- PRINT ANALYSIS --------
void printDetailedAnalysis(String state, int focusScore,
                          double focusIndex, double relaxIndex, double drowsyIndex) {

  Serial.println("\n===== EEG ANALYSIS =====");
  Serial.print("Time: "); Serial.println(millis());

  Serial.println("\nRaw Signal:");
  Serial.print("Avg: "); Serial.println(avgSignal);
  Serial.print("Min: "); Serial.println(minSignal);
  Serial.print("Max: "); Serial.println(maxSignal);

  Serial.println("\nBand Power:");
  Serial.print("Delta: "); Serial.println(delta.power);
  Serial.print("Theta: "); Serial.println(theta.power);
  Serial.print("Alpha: "); Serial.println(alpha.power);
  Serial.print("Beta : "); Serial.println(beta.power);
  Serial.print("Gamma: "); Serial.println(gammaBand.power);

  Serial.println("\nMetrics:");
  Serial.print("Focus Index: "); Serial.println(focusIndex);
  Serial.print("Relax Index: "); Serial.println(relaxIndex);
  Serial.print("Drowsy Index: "); Serial.println(drowsyIndex);
  Serial.print("Focus Score: "); Serial.println(focusScore);

  Serial.println("\nState:");
  Serial.println(state);

  Serial.println("\nInterpretation:");

  if (state == "Focused")
    Serial.println("High beta activity → strong concentration.");
  else if (state == "Relaxed")
    Serial.println("Alpha dominance → calm state.");
  else if (state == "Drowsy")
    Serial.println("Theta dominance → fatigue or eye closure.");
  else
    Serial.println("No dominant cognitive state.");

  Serial.println("========================\n");
}

// -------- CALIBRATION --------
void calibrateBaseline() {

  double total = 0, count = 0;
  unsigned long start = millis();

  while (millis() - start < CALIBRATION_TIME * 1000) {

    readSignal();
    performFFT();
    calculateBandPower();

    double f = beta.power / (alpha.power + theta.power + 1);

    total += f;
    count++;

    delay(200);
  }

  baselineFocus = total / count;
}
