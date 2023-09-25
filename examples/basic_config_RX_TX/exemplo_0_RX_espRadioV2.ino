#include "espRadio.h"

/*/   espRadio V2.0 - Exemplo de receptor   /*/

/*/
oq falta:
  1 - salvar lista MAC e configurações na memoria flash
  2 - suporte a envio de pacotes
  3 - suporte ao TX
  4 - colocar nome do dispositivo
/*/

#include "GPIO_CORE.h"
GPIO_CORE btn(0,LOW,GPIO_IN_DIG_UP);

void setup() {

  Serial.begin(115200);
  Serial.println("\n\n\n");

  pinMode(2,OUTPUT);
  btn.begin();
  btn.filter_debounce(50,2);

  // Config and begin espnow Radio reciver
  espRadio.begin( espRadio.TX, handle_radio );

  // Set port mode -> só verifica a porta, ignora o mac destino
  espRadio.mode( espRadio.PORT );
  
  Serial.println("\nRADIO - ESPNOW start");
  
  // update led state
  digitalWrite(2,!espRadio.online());

}

boolean BIND = false;
uint32_t timeout = 0;
boolean  state   = false;

void loop() {
  
  btn.update();
  espRadio.update();
  
  if( btn.is_on_for(700) ){ digitalWrite(2,LOW); }
  
  if( btn.fall() ){
    if( btn.was_on_for(700) ){
      Serial.println("FORMATANDO!");
      LittleFS.format();
      Serial.println("END");
      delay(500);
      digitalWrite(2,HIGH);
    }else{
      //esp_now_send( broadcastAddress, (uint8_t *) &espRadio.pack_rx, sizeof(pacote));
      BIND = !BIND;
      if( BIND ) espRadio.bind_on(); else espRadio.bind_off();
      digitalWrite(2,!espRadio.online());
    }
    
  }

  if( BIND ){
    BIND = espRadio.binding();
    if(timeout <= millis()){ state = !state; digitalWrite(2,state); timeout = millis() + 100; }
  }

}


/// callback -------------------------------------------------------

int VAR = 0;

int handle_radio( int action ){
  if( action == espRadio.RISE ){
    digitalWrite(2,LOW); Serial.println("Radio conectado!");
  }else if( action == espRadio.FALL ){
    digitalWrite(2,HIGH); Serial.println("Radio desconectado!");
  }else if( action == espRadio.RECIVE ){
    Serial.printf(
      "\nNew data!\n[ Device[%d]: %s / %d ][ MODE:%d ][ LEN:%d ][ CODE: %d ][ ID: %d ][ ",
      espRadio.device_connect_number,
      espRadio.device_connect().name,
      espRadio.device_connect().ID,
      espRadio.mode(),
      espRadio.pack_rx.len,
      espRadio.pack_rx.code,
      espRadio.pack_rx.ID
    );
    for(int i=0;i < ( espRadio.pack_rx.len > 20 ? 20 : espRadio.pack_rx.len );i++)
      Serial.printf("%d  ",espRadio.pack_rx.ch[i]);
    Serial.println("]");
  }else if( action == espRadio.SEND ){
    Serial.println("[ SEND ]");
    espRadio.pack_tx.len = 1;
    VAR++;
    espRadio.pack_tx.ch[0] = VAR;
  }else if( action == espRadio.START_MODE_NORMAL ){
    Serial.println("[ START NORMAL MODE ]");
  }else if( action == espRadio.START_MODE_PORT ){
    Serial.println("[ START PORT MODE ]");
  }else if( action == espRadio.BIND_ON ){
    Serial.println("[ BIND ON ]");
  }else if( action == espRadio.BIND_OFF ){
    Serial.println("[ BIND OFF ]");
  }else if( action == espRadio.BINDED ){
    Serial.println("[ BINDED ]");
  }else if( action == espRadio.MEMORY_BEGIN ){
    Serial.println("[ MEMORY BEGIN ]");
  }else if( action == espRadio.SAVED ){
    Serial.println("[ MEMORY SAVED ]");
  }else if( action == espRadio.SAVE_DEVICES ){
    Serial.println("[ MEMORY SAVE DEVICES ]");
  }else if( action == espRadio.READ_DEVICES ){
    Serial.println("[ MEMORY READ DEVICES ]");
  }else if( action == espRadio.RECOVER_DEVICES ){
    Serial.println("[ MEMORY RECOVER DEVICES ]");
    espRadio.self_device( "Prometheus", 3, 0x0000FF ); // self
    espRadio.add_device(  "Coelhinho" , 3, 0x00FFFF, 0x30, 0x83, 0x98, 0xb6, 0x30, 0x21); // coversor PPM -> ESPNOW
    espRadio.add_device(  "Bananinha" , 3, 0xFF0000, 0x60, 0x55, 0xF9, 0x71, 0x66, 0x0C); // Radio Luis - ESP32_C3
    espRadio.add_device(  "R2D2"      , 3, 0xFF00FF, 0xa0, 0xb7, 0x65, 0x4b, 0x02, 0x90); // ESP32 protoboard USB-C
  }

  return true;
}

