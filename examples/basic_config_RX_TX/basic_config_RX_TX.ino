#include "espRadio.h"

/*/   espRadio V2.0 - Exemplo de receptor   /*/

#include "GPIO_CORE.h"
GPIO_CORE btn(0,LOW,GPIO_IN_DIG_UP);

#ifdef ESP32
#define LED_STATE_ON HIGH
#else
#define LED_STATE_ON LOW
#endif

void setup() {

  Serial.begin(115200);
  Serial.println("\n\n\n");

  // Perifericos: led e botão
  pinMode(2,OUTPUT);
  btn.begin();
  btn.filter_debounce(50,2);

  // Config and begin espnow Radio reciver
  //espRadio.begin( espRadio.TX, handle_radio );
  espRadio.begin( handle_radio );
  
  Serial.println("\nRADIO - ESPNOW start");
  
  // update led state
  led( espRadio.online() );

}

boolean BIND = false;
uint32_t timeout = 0;
boolean  state   = false;

void loop() {
  
  btn.update();
  espRadio.update();
  
  if( btn.is_on_for(700) ){ led(1); }
  
  if( btn.fall() ){
    if( btn.was_on_for(700) ){
      Serial.println("FORMATANDO!");
      LittleFS.format();
      Serial.println("END");
      delay(500);
      led(0);
    }else{
      //esp_now_send( broadcastAddress, (uint8_t *) &espRadio.pack_rx, sizeof(pacote));
      BIND = !BIND;
      if( BIND ) espRadio.bind_on(); else espRadio.bind_off();
      led(espRadio.online());
    }
    
  }

  if( BIND ){
    BIND = espRadio.binding();
    if(timeout <= millis()){ state = !state; led(state); timeout = millis() + 100; }
  }

}


void led(boolean st){
  digitalWrite(2,LED_STATE_ON == st);
}


/// callback -------------------------------------------------------

int VAR = 0;

int handle_radio( int action ){
  
  if( action == espRadio.RISE ){
    
    led(1);
    Serial.println("Radio conectado!");

  }else if( action == espRadio.FALL ){
    
    led(0);
    Serial.println("Radio desconectado!");

  }else if( action == espRadio.RECIVE ){
    
    Serial.printf(
      "\nNew data!\n[ Device[%d]: %s / %d ][ Connection mode:%d ][ LEN:%d ][ CODE: %d ][ ID: %d ][ ",
      espRadio.device_connect_number,
      espRadio.device_connect().name,
      espRadio.device_connect().ID,
      espRadio.recive_mode(),
      espRadio.pack_rx.len,
      espRadio.pack_rx.code,
      espRadio.pack_rx.ID
    );
    
    for(int i=0;i < ( espRadio.pack_rx.len > 20 ? 20 : espRadio.pack_rx.len );i++){
      Serial.printf("%d  ",espRadio.pack_rx.ch[i]);
    }
    Serial.println("]");

  }else if( action == espRadio.SEND ){
    
    Serial.println("[ SEND ]");
    espRadio.pack_tx.len = 1;
    VAR++;
    espRadio.pack_tx.ch[0] = VAR;

  }
  else if( action == espRadio.CHANGE_RECIVE_MODE ){ Serial.println("[ CHANGE RECIVE MODE ]");  }
  else if( action == espRadio.CHANGE_SEND_MODE   ){ Serial.println("[ CHANGE SEND MODE ]");    }
  else if( action == espRadio.BIND_ON            ){ Serial.println("[ BIND ON ]");             }
  else if( action == espRadio.BIND_OFF           ){ Serial.println("[ BIND OFF ]");            }
  else if( action == espRadio.BINDED             ){ Serial.println("[ BINDED ]");              }
  else if( action == espRadio.MEMORY_BEGIN       ){ Serial.println("[ MEMORY BEGIN ]");        }
  else if( action == espRadio.SAVED              ){ Serial.println("[ MEMORY SAVED ]");        }
  else if( action == espRadio.SAVE_DEVICES       ){ Serial.println("[ MEMORY SAVE DEVICES ]"); }
  else if( action == espRadio.READ_DEVICES       ){ Serial.println("[ MEMORY READ DEVICES ]"); }
  else if( action == espRadio.RECOVER_DEVICES ){
    Serial.println("[ MEMORY RECOVER DEVICES ]");
    espRadio.self_device( "Prometheus", 3, 0x0000FF ); // self
    espRadio.add_device(  "Coelhinho" , 3, 0x00FFFF, 0x30, 0x83, 0x98, 0xb6, 0x30, 0x21); // coversor PPM -> ESPNOW
    espRadio.add_device(  "Bananinha" , 3, 0xFF0000, 0x60, 0x55, 0xF9, 0x71, 0x66, 0x0C); // Radio Luis - ESP32_C3
    espRadio.add_device(  "R2D2"      , 3, 0xFF00FF, 0xa0, 0xb7, 0x65, 0x4b, 0x02, 0x90); // ESP32 protoboard USB-C
  }else if( action == espRadio.RECOVER_CONFIG ){
    Serial.println("[ MEMORY RECOVER CONFIG ]");
    
    espRadio.config.Radio_role  = espRadio.TX;
    espRadio.config.telemetry   = false;
    //espRadio.config.delay_failsafe = 0;
    //espRadio.config.delay_send     = 0;
    //espRadio.config.delay_bind     = 0;
    espRadio.config.send_mode   = espRadio.SEND_BROADCAST;
    espRadio.config.recive_mode = espRadio.PORT;
    //espRadio.config.mac_target;
    espRadio.config.port        = 0;

  }

  return true;
}

