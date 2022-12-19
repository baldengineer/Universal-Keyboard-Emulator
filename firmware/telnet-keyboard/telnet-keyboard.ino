#include <WiFi.h>
#include "Keyboard.h"
#include "wifi_config.h"

WiFiServer server(port);

void setup() {
  Serial.begin(115200);
  while(!Serial);
  delay(500);
  Serial.println("Universal Keyboard Adapter");

  Serial.println("Turning into a keyboard");
  Keyboard.begin();

  Serial.println("Starting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("PicoW2");
  Serial.print("Connecting to '");
  Serial.print(ssid);
  Serial.println("'");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    static uint32_t attempt_count=0;
    if (attempt_count++ > 100)
      rp2040.restart();
    Serial.print(".");
    delay(100);
  }

  Serial.print("\nConnected to WiFi: ");
  Serial.println(WiFi.localIP().toString());
  Serial.print("Starting telnet server on Port ");
  Serial.println(port);
  server.begin();
}

void print_ip() {
  static uint32_t previous_millis = 0;
  uint32_t current_millis = millis();
  if (current_millis - previous_millis >= 5000) {
    Serial.print("[");
    Serial.print(port);
    Serial.print("]IP: ");
    Serial.println(WiFi.localIP().toString());
    Serial.flush();
    previous_millis = current_millis;
  }
}

#define ESCAPE_TIMEOUT_MS 100
#define ESCAPE_IDLE 0
#define ESCAPE_DETECTED 1
#define ESCAPE_SEQUENCE_ACTIVE 2
#define ESCAPE_LENGTH_MAX 4
void loop() {
  print_ip();
  // listen for incoming clients
  WiFiClient client = server.available();
  if (client) {
    Serial.println("Client CONNECTED");
    while (client.connected()) {
      while (client.available()) {
        static bool drop_null = false;
        static int escape_sequence = ESCAPE_IDLE;
        static uint32_t esc_seq_millis = 0;
        static char escape_string[8];
        static int escape_index = 0;
        static int high_skip = 0; // ignore the two characters after 255
        static bool high_skip_inform = false;
        escape_string[7] = '\0';

        int c = client.read();
        
        if (high_skip-- > 0)
          continue;
        
        if (high_skip_inform) {
          Serial.println("Skipped High Seq");
          high_skip_inform = false;
        }

        if (drop_null && (c==0 || c==10)) {
          if (c==10) Serial.println("Switch MODE CHARACTER!");
          drop_null = false;
          continue;
        }
        drop_null = false; // we only do the drop-check once
        // was escape_detected
        // did 100 ms go by without a new character?
        // if so, the user pressed it.
        // if not, treat the next 4-5 characters as the escape sequence ...

        // if (escape_sequence == ESCAPE_DETECTED) {
        //   if ((millis() - esc_seq_millis >= ESCAPE_TIMEOUT_MS)) {
        //     Serial.println("ESC");
        //     escape_sequence = ESCAPE_IDLE;
        //     escape_index = 0;             
        //   } else {
        //     escape_sequence = ESCAPE_SEQUENCE_ACTIVE;
        //   }
        // }

        if (escape_sequence == ESCAPE_SEQUENCE_ACTIVE) {
          escape_string[escape_index++] = c;
          if (escape_index > ESCAPE_LENGTH_MAX) {
            escape_string[escape_index] = '\0';
            escape_sequence = ESCAPE_IDLE;
            escape_index = 0;
            Serial.println(escape_string);
            continue;
          }
        }

        if ((c >= 32) && (c <=126) && (escape_sequence==ESCAPE_IDLE)) {
          // it is a printable character
          Serial.print("[");
          Serial.print(c);
          Serial.print("] '");
          Serial.write(c);
          Serial.println("'");
        }
        if ((c < 32)) {
          // it is a control character

          // don't print anything for ESC
          if (c != 27) {
            Serial.print("CTRL,");
            Serial.write(c+64);
            Serial.print(" ("); Serial.print(c); Serial.println(")");
          }

          // telnet sends either a \r or \0 after a \n, so we ignore it
          if (c == 13)
            drop_null = true;

          // start timer to see if an esc seq is coming or if a slow human pressed the esc key
          if (c == 27) {
            delay(50);
            if (client.available() > 0)            
              escape_sequence = ESCAPE_SEQUENCE_ACTIVE;
            else
              Serial.println("ESC");
          }
        }
        if ((c > 126) && (c < 251)) {
          Serial.print("HIGH,");
          Serial.println(c);
        }
        if (c == 255) {
          high_skip = 2;
          high_skip_inform = true;
        }
      }
    }
    Serial.println("Client disconnected");
    client.stop();
  }
}

/*
HIGH,255
HIGH,253
CTRL,C (3)
HIGH,255
HIGH,251
CTRL,X (24)
HIGH,255
HIGH,251
CTRL,_ (31)
HIGH,255
HIGH,251
[32] ' '
HIGH,255
HIGH,251
[33] '!'
HIGH,255
HIGH,251
[34] '"'
HIGH,255
HIGH,251
[39] '''
HIGH,255
HIGH,253
CTRL,E (5)
*/
