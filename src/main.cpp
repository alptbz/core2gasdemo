#include <Arduino.h>
#include "view.h"
#include "networking.h"
#include "sideled.h"
#include "soundwav.h"
#include <Adafruit_SGP30.h>

void mqtt_callback(char* topic, byte* payload, unsigned int length);
void init_gui();
void loop();
void init_networking();
void init_actors();
void init_sensors();

unsigned long next_lv_task = 0;

uint16_t tvoc = 0;
uint16_t eco2 = 0;

Adafruit_SGP30 sgp;

Speaker speaker;

lv_obj_t * label_tvoc;
lv_obj_t * label_eco2;

void init_gui() {
  add_label("TVOC:", 10, 10);
  label_tvoc = add_label("0 ppb", 150, 10);
  add_label("eCO2:", 10, 50);
  label_eco2 = add_label("0 ppm", 150, 50);
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  // Parse Payload into String
  char * buf = (char *)malloc((sizeof(char)*(length+1)));
  memcpy(buf, payload, length);
  buf[length] = '\0';
  String payloadS = String(buf);
  payloadS.trim();

  Serial.println(topic);
  Serial.println(payloadS);
}

unsigned long next_temp_send = 0;
unsigned long next_temp_show = 0;
unsigned long next_sound_play = 0;

#define ECO2_TRESHHOLD 1000
bool reached_treshhold = false;

size_t sound_pos = 0;

bool sound_enabled = false;

void loop() {
  if(next_lv_task < millis()) {
    lv_task_handler();
    next_lv_task = millis() + 5;
  }
  if(next_temp_send < millis()) {
    char buf[32];
    sprintf(buf, "{\"tvoc\":%i, \"eco2\":%i}", tvoc, eco2);
    mqtt_publish("m5core2/status", buf);
    snprintf (buf, 32, "%d", tvoc);
    mqtt_publish("m5core2/tvoc", buf);
    snprintf (buf, 32, "%d", eco2);
    mqtt_publish("m5core2/eco2", buf);
    next_temp_send = millis() + 1000;
  }
  if(next_temp_show < millis()) {
     if(sgp.IAQmeasure()) {
      tvoc = sgp.TVOC;
      eco2 = sgp.eCO2;
      lv_label_set_text(label_tvoc, (String(tvoc)+ " ppb").c_str());
      lv_label_set_text(label_eco2, (String(eco2)+ " ppm").c_str());
     }
     if(eco2 > ECO2_TRESHHOLD && !reached_treshhold) {
      set_sideled_color(0,10, CRGB::Red);
      set_sideled_state(0,10, SIDELED_STATE_BLINK);
      reached_treshhold = true;
      sound_enabled = true;
     }else if(eco2 < (ECO2_TRESHHOLD - 50) && reached_treshhold) {
      set_sideled_color(0,10, CRGB::Green);
      set_sideled_state(0,10, SIDELED_STATE_ON);
      reached_treshhold = false;
      sound_enabled = false;
     }
    next_temp_show = millis() + 1000;
  }
  if(sound_enabled && next_sound_play < millis()) {
    size_t byteswritten = speaker.PlaySound(sounddata + sound_pos, NUM_ELEMENTS);
    
    sound_pos = sound_pos + byteswritten;
    if(sound_pos >= NUM_ELEMENTS) {
      sound_pos = 0;
    }
    next_sound_play = millis() + 100;
  }

  mqtt_loop();
} 

// ----------------------------------------------------------------------------
// MAIN SETUP
// ----------------------------------------------------------------------------

void init_networking() {
  lv_obj_t * wifiConnectingBox = show_message_box_no_buttons("Connecting to WiFi...");
  lv_task_handler();
  delay(5);
  setup_wifi();
  mqtt_init(mqtt_callback);
  close_message_box(wifiConnectingBox);
}

void init_sensors() {
  Serial.println("SGP30 test");

  if (! sgp.begin()){
    Serial.println("Sensor not found :(");
    while (1);
  }
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);
}

void init_actors() {
  speaker.InitI2SSpeakOrMic(1);
}

void setup() {
  init_m5();
  init_display();
  Serial.begin(115200);
  
  init_networking();

  init_gui();
  init_sideled();

  init_sensors();
  init_actors();
}