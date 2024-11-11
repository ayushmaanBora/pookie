#include <Wire.h>
#include <BluetoothSerial.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// Initialize instances for Bluetooth, GPS, and GSM
BluetoothSerial SerialBT;
TinyGPSPlus gps;
HardwareSerial sim800(1); // Use UART1 for SIM800 on ESP32

// Define pins for the emergency and call buttons
const int emergencyButtonPin = 26; // Button for sending SMS
const int callButtonPin = 27;      // Button for making a call

// Primary emergency contact
String primaryContact = "+916002872225"; // Include country code for international format

// Contact list for additional contacts
String contacts[5]; // Store up to 5 additional contacts
int contactCount = 0;
bool emergencyActive = false;

// Initialize flags
bool isBluetoothConnected = false;
bool audioRecording = false;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_EmergencyDevice"); // Bluetooth device name
  sim800.begin(9600, SERIAL_8N1, 16, 17); // RX, TX pins for SIM800
  pinMode(emergencyButtonPin, INPUT_PULLUP);
  pinMode(callButtonPin, INPUT_PULLUP);

  if (checkGSMModule()) {
    Serial.println("GSM module initialized successfully.");
  } else {
    Serial.println("GSM module initialization failed. Please check connections.");
  }
}

void loop() {
  // Listen for Bluetooth commands
  if (SerialBT.available()) {
    handleBluetoothCommand(SerialBT.readString());
  }

  // Check emergency button press
  if (digitalRead(emergencyButtonPin) == LOW) {
    Serial.println("Emergency button pressed.");
    delay(500); // Debounce delay
    sendEmergencyAlert();
  }

  // Check call button press
  if (digitalRead(callButtonPin) == LOW) {
    Serial.println("Call button pressed.");
    delay(500); // Debounce delay
    makeEmergencyCall();
  }

  // Check GPS data
  while (sim800.available() > 0) {
    char c = sim800.read();
    Serial.print(c);  // Print raw GPS data for debugging
    gps.encode(c);    // Feed data to TinyGPS++
  }
}

// Check GSM module connectivity
bool checkGSMModule() {
  sim800.println("AT"); // Basic AT command to check connectivity
  delay(1000);
  if (sim800.find("OK")) {
    return true;
  }
  return false;
}

// Handle incoming Bluetooth commands for adding/deleting contacts
void handleBluetoothCommand(String command) {
  if (command.startsWith("ADD:")) {
    String newContact = command.substring(4);
    if (contactCount < 5) {
      contacts[contactCount++] = newContact;
      Serial.println("Contact added: " + newContact);
      sendConfirmation("Contact added: " + newContact);
    } else {
      sendConfirmation("Contact list full!");
    }
  } else if (command.startsWith("DELETE:")) {
    String contactToDelete = command.substring(7);
    deleteContact(contactToDelete);
  } else if (command.startsWith("LIST")) {
    listContacts();
  }
}

// Delete a contact by matching its phone number
void deleteContact(String phone) {
  bool found = false;
  for (int i = 0; i < contactCount; i++) {
    if (contacts[i] == phone) {
      for (int j = i; j < contactCount - 1; j++) {
        contacts[j] = contacts[j + 1];
      }
      contactCount--;
      found = true;
      Serial.println("Contact deleted: " + phone);
      sendConfirmation("Contact deleted: " + phone);
      break;
    }
  }
  if (!found) {
    sendConfirmation("Contact not found.");
  }
}

// List all contacts over Bluetooth
void listContacts() {
  SerialBT.println("Contacts:");
  for (int i = 0; i < contactCount; i++) {
    SerialBT.println(contacts[i]);
  }
}

// Send confirmation message over Bluetooth
void sendConfirmation(String message) {
  SerialBT.println(message);
}

// Send SMS with GPS location if available
void sendEmergencyAlert() {
  bool locationAvailable = false;
  int retryCount = 0;
  const int maxRetries = 20; // Increased retry count
  const int retryDelay = 2000; // 2-second delay

  Serial.println("Attempting to get GPS location...");
  
  while (retryCount < maxRetries) {
    if (gps.location.isValid()) {
      locationAvailable = true;
      break;
    }
    retryCount++;
    Serial.println("GPS location not available, retrying...");
    delay(retryDelay);
  }

  if (locationAvailable) {
    String message = "Emergency! Location: http://maps.google.com/maps?q=" +
                     String(gps.location.lat(), 6) + "," + 
                     String(gps.location.lng(), 6);
    Serial.println("Sending SMS to primary contact with GPS location...");
    sendSMS(primaryContact, message);
    for (int i = 0; i < contactCount; i++) {
      sendSMS(contacts[i], message);
    }
    Serial.println("Emergency alert sent.");
  } else {
    Serial.println("GPS location not available after retries.");
  }
}

// Function to send SMS
void sendSMS(String number, String message) {
  sim800.println("AT+CMGF=1"); // Set SMS mode to text
  delay(1000);
  sim800.println("AT+CMGS=\"" + number + "\"");
  delay(1000);
  sim800.println(message);
  delay(1000);
  sim800.write(26); // ASCII code of CTRL+Z to send message
  delay(5000);
  Serial.println("SMS sent to: " + number);
}

// Function to make a call to the primary emergency contact
void makeEmergencyCall() {
  Serial.println("Dialing primary contact: " + primaryContact);
  sim800.println("ATD" + primaryContact + ";"); // Dial the contact number
  delay(30000); // Wait for 30 seconds (modify based on your needs)
  sim800.println("ATH"); // Hang up the call
  Serial.println("Call ended.");
}
