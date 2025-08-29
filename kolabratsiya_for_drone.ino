#include <Wire.h>

float gyroXsum = 0, gyroYsum = 0, gyroZsum = 0;
float accXsum = 0, accYsum = 0, accZsum = 0;

int samples = 2000;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.beginTransmission(0x68);
  Wire.write(0x6B); Wire.write(0); Wire.endTransmission();

  Serial.println("Please keep the IMU completely still!");
  delay(2000);
  Serial.println("Calibrating...");

  for (int i = 0; i < samples; i++) {
    Wire.beginTransmission(0x68);
    Wire.write(0x3B);
    Wire.endTransmission();
    Wire.requestFrom(0x68, 6);
    int16_t accX = Wire.read()<<8 | Wire.read();
    int16_t accY = Wire.read()<<8 | Wire.read();
    int16_t accZ = Wire.read()<<8 | Wire.read();

    Wire.beginTransmission(0x68);
    Wire.write(0x43);
    Wire.endTransmission();
    Wire.requestFrom(0x68, 6);
    int16_t gyroX = Wire.read()<<8 | Wire.read();
    int16_t gyroY = Wire.read()<<8 | Wire.read();
    int16_t gyroZ = Wire.read()<<8 | Wire.read();

    gyroXsum += gyroX / 65.5;
    gyroYsum += gyroY / 65.5;
    gyroZsum += gyroZ / 65.5;

    accXsum += accX / 4096.0;
    accYsum += accY / 4096.0;
    accZsum += (accZ / 4096.0 - 1.0); // Z normalda 1g bo'ladi
    delay(2);
  }

  float gyroXoffset = gyroXsum / samples;
  float gyroYoffset = gyroYsum / samples;
  float gyroZoffset = gyroZsum / samples;

  float accXoffset = accXsum / samples;
  float accYoffset = accYsum / samples;
  float accZoffset = accZsum / samples;

  Serial.println("------ Calibration Results ------");
  Serial.print("RateCalibrationRoll = ");  Serial.println(gyroXoffset, 4);
  Serial.print("RateCalibrationPitch = "); Serial.println(gyroYoffset, 4);
  Serial.print("RateCalibrationYaw = ");   Serial.println(gyroZoffset, 4);
  Serial.println();
  Serial.print("AccXCalibration = "); Serial.println(accXoffset, 4);
  Serial.print("AccYCalibration = "); Serial.println(accYoffset, 4);
  Serial.print("AccZCalibration = "); Serial.println(accZoffset, 4);
}

void loop() {
  // Nothing here – just read results in Serial monitor
}
