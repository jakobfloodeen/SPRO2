#include <SPI.h>
#include <SD.h>
#include <stdio.h>
#include <string.h>

#define CS_PIN 10
#define LOG_INTERVAL 1000

// Keep filename short - SD library only supports 8.3 format
// meas_0.csv = 10 chars total which is fine
#define FILENAME_BASE "meas_"

unsigned long start_delay; //used to start at 0, when the file is ready
int fileamount = 0; //to start from 0, is later appended to the filename
char checkname[15]; //used to check if filename already exists

File dataFile;

void initSD(void);
void logData(void);
unsigned int readSensor(int pin);

void setup(void) {
    Serial.begin(9600);
    while (!Serial);
    initSD();
}

void loop(void) {
    logData();
    delay(LOG_INTERVAL);
}

void initSD(void) {
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CS_PIN, HIGH);
    delay(100);

    if (!SD.begin(CS_PIN)) {
        Serial.println("SD init failed!");
        while (1);
    }

    Serial.println("SD ready.");

    // Find the next available filename
    // Keep incrementing fileamount until we find a name that doesnt exist
    do {
        sprintf(checkname, "%s%d.csv", FILENAME_BASE, fileamount);
        fileamount++;
    } while (SD.exists(checkname)); 

    Serial.print("Creating file: ");
    Serial.println(checkname);

    // Create the file and write header
    dataFile = SD.open(checkname, FILE_WRITE);
    if (dataFile) {
        dataFile.println("timestamp_ms;A0;A1;A5");
        dataFile.close();
        Serial.println("File created!");
        // Record start time for timestamp offset
        start_delay = millis();
    } else {
        Serial.println("Failed to create file!");
        while (1);
    }
}

unsigned int readSensor(int pin) {
    return analogRead(pin);
}

void logData(void) {
    unsigned long timestamp = millis() - start_delay;
    unsigned int a0 = readSensor(A0);
    unsigned int a1 = readSensor(A1);
    unsigned int a5 = readSensor(A5);

    // Build CSV row using sprintf
    char row[50];
    sprintf(row, "%lu;%d;%d;%d", timestamp, a0, a1, a5);

    dataFile = SD.open(checkname, FILE_WRITE);
    if (dataFile) {
        dataFile.println(row);
        dataFile.close();
    } else {
        Serial.println("Write failed!");
    }
}