#include <Wire.h>
#include <ESP32Servo.h>

// =============== OFFSET KIRITING ===============
float RateCalibrationRoll = 2.1303;
float RateCalibrationPitch = -0.0101;
float RateCalibrationYaw = -1.6220;

float AccXCalibration = 0.1170;
float AccYCalibration = 0.0755;
float AccZCalibration = -0.0428;
// =================================================

float t = 0.004; // ~250Hz

// IMU variables
volatile float RateRoll, RatePitch, RateYaw;
volatile float AccX, AccY, AccZ;
volatile float AngleRoll, AnglePitch;
float compRoll = 0, compPitch = 0;

// Receiver variables
volatile unsigned long ch1Start = 0, ch2Start = 0, ch3Start = 0, ch4Start = 0;
volatile int chVal[4] = {1500, 1500, 1000, 1500}; // default values

// Channel pins
const int CH1_PIN = 34; // Roll
const int CH2_PIN = 35; // Pitch
const int CH3_PIN = 32; // Throttle
const int CH4_PIN = 33; // Yaw

// PWM outputs
Servo motor1, servoRoll, servoPitch, servoYaw;
const int M_PIN = 13;
const int SR_PIN = 12;
const int SP_PIN = 14;
const int SY_PIN = 27;

// Interrupt handlers for each channel
void IRAM_ATTR ch1ISR() {
  if (digitalRead(CH1_PIN) == HIGH) {
    ch1Start = micros();
  } else {
    if (ch1Start != 0) {
      chVal[0] = micros() - ch1Start;
      chVal[0] = constrain(chVal[0], 1000, 2000);
    }
  }
}

void IRAM_ATTR ch2ISR() {
  if (digitalRead(CH2_PIN) == HIGH) {
    ch2Start = micros();
  } else {
    if (ch2Start != 0) {
      chVal[1] = micros() - ch2Start;
      chVal[1] = constrain(chVal[1], 1000, 2000);
    }
  }
}

void IRAM_ATTR ch3ISR() {
  if (digitalRead(CH3_PIN) == HIGH) {
    ch3Start = micros();
  } else {
    if (ch3Start != 0) {
      chVal[2] = micros() - ch3Start;
      chVal[2] = constrain(chVal[2], 1000, 2000);
    }
  }
}

void IRAM_ATTR ch4ISR() {
  if (digitalRead(CH4_PIN) == HIGH) {
    ch4Start = micros();
  } else {
    if (ch4Start != 0) {
      chVal[3] = micros() - ch4Start;
      chVal[3] = constrain(chVal[3], 1000, 2000);
    }
  }
}

// IMU reading function
void readIMU() {
  // Read accelerometer
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 6, true);

  int16_t AX = (Wire.read() << 8) | Wire.read();
  int16_t AY = (Wire.read() << 8) | Wire.read();
  int16_t AZ = (Wire.read() << 8) | Wire.read();

  // Read gyroscope
  Wire.beginTransmission(0x68);
  Wire.write(0x43);
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 6, true);

  int16_t GX = (Wire.read() << 8) | Wire.read();
  int16_t GY = (Wire.read() << 8) | Wire.read();
  int16_t GZ = (Wire.read() << 8) | Wire.read();

  // Convert to physical units and apply calibration
  RateRoll  = (GX / 65.5f) - RateCalibrationRoll;
  RatePitch = (GY / 65.5f) - RateCalibrationPitch;
  RateYaw   = (GZ / 65.5f) - RateCalibrationYaw;

  AccX = (AX / 4096.0f) - AccXCalibration;
  AccY = (AY / 4096.0f) - AccYCalibration;
  AccZ = (AZ / 4096.0f) - AccZCalibration;

  // Calculate angles from accelerometer
  AngleRoll  = atan2(AccY, sqrt(AccX*AccX + AccZ*AccZ)) * 57.2958f;
  AnglePitch = -atan2(AccX, sqrt(AccY*AccY + AccZ*AccZ)) * 57.2958f;

  // Complementary filter
  compRoll  = 0.98f * (compRoll  + RateRoll * t)  + 0.02f * AngleRoll;
  compPitch = 0.98f * (compPitch + RatePitch * t) + 0.02f * AnglePitch;
}

void setup() {
  Serial.begin(115200);

  // Initialize I2C and MPU6050
  Wire.begin();
  delay(100);

  // Wake up MPU6050
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission(true);
  delay(100);

  // Configure accelerometer (±8g)
  Wire.beginTransmission(0x68);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission(true);

  // Configure gyroscope (±500°/s)
  Wire.beginTransmission(0x68);
  Wire.write(0x1B);
  Wire.write(0x08);
  Wire.endTransmission(true);

  // Setup receiver pins
  pinMode(CH1_PIN, INPUT_PULLUP);
  pinMode(CH2_PIN, INPUT_PULLUP);
  pinMode(CH3_PIN, INPUT_PULLUP);
  pinMode(CH4_PIN, INPUT_PULLUP);

  // Attach separate interrupts for each channel
  attachInterrupt(digitalPinToInterrupt(CH1_PIN), ch1ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(CH2_PIN), ch2ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(CH3_PIN), ch3ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(CH4_PIN), ch4ISR, CHANGE);

  // Initialize servos and motor
  motor1.attach(M_PIN, 1000, 2000);
  servoRoll.attach(SR_PIN, 1000, 2000);
  servoPitch.attach(SP_PIN, 1000, 2000);
  servoYaw.attach(SY_PIN, 1000, 2000);

  // Set initial positions
  motor1.writeMicroseconds(1000);
  servoRoll.writeMicroseconds(1500);
  servoPitch.writeMicroseconds(1500);
  servoYaw.writeMicroseconds(1500);

  delay(2000); // Wait for ESC initialization

  Serial.println("RC Aircraft Stabilizer Ready!");
  Serial.println("Roll\tPitch\tThrottle");
}

void loop() {
  readIMU();

  // Throttle control (direct pass-through)
  int throttle = chVal[2];
  throttle = constrain(throttle, 1000, 1800);
  motor1.writeMicroseconds(throttle);

  // Stabilization gain
  float k = 5.0;

  // Calculate corrections
  float rollCorr  = compRoll * k;
  float pitchCorr = compPitch * k;

  // Apply corrections to RC inputs
  int rollOut  = 1500 + (chVal[0] - 1500) - (int)rollCorr;
  int pitchOut = 1500 + (chVal[1] - 1500) - (int)pitchCorr;
  int yawOut   = chVal[3]; // No yaw stabilization

  // Constrain outputs
  rollOut  = constrain(rollOut, 1000, 2000);
  pitchOut = constrain(pitchOut, 1000, 2000);
  yawOut   = constrain(yawOut, 1000, 2000);

  // Send to servos
  servoRoll.writeMicroseconds(rollOut);
  servoPitch.writeMicroseconds(pitchOut);
  servoYaw.writeMicroseconds(yawOut);

  // Serial output for monitoring
  Serial.print(rollOut);
  Serial.print("\t");
  Serial.print(pitchOut);
  Serial.print("\t");
  Serial.println(throttle);

  // Maintain loop frequency (~250Hz)
  delayMicroseconds(4000);
}