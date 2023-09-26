#ifndef ESPNOW_H
#define ESPNOW_H

//#define ESP8266 // <-- comentar isso

#include "Arduino.h"

/*/  
    
    espRadio V2.0

    tanto RX como TX recebem e enviam.

    config:

      send_mode:
        broadcast: envia apenas em broadcast
        Device:
          TX: envia para config.mac_target
          RX: envia para device_connect

      recive_mode:
        NORMAL:
          TX: indiferente
          RX: só recebe se o mac de origem estiver listado e se
              o endereço mac destino do pacote for seu proprio mac
        PORT:
          TX: indiferente
          RX: só recebe se o mac de origem estiver listado e se
              o port for o mesmo local

    
    -----------------------------------------------------------------------
    |                                 |  TX             |  RX             |
    -----------------------------------------------------------------------
    | envia pacotes frequentementes?  |  sim            |  Pode           |
    | recebe pacotes frequentementes? |  Pode           |  sim            |
    -----------------------------------------------------------------------
    | Durante o inicio do bind        | envia e recebe  | recebe          |
    -----------------------------------------------------------------------
    | Ao receber pacote de bind       | volta ao estado | volta ao estado |
    |                                 | anterior        | anterior e      |
    |                                 |                 | envia resposta  |
    -----------------------------------------------------------------------

/*/  

#if (defined(ESP8266) || defined(ESP32))

#if defined(ESP8266)
  #include <espnow.h>
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <esp_mac.h>
  #include <esp_now.h>
  #include <WiFi.h>
  esp_now_peer_info_t espnow_local_info;
#endif

 // Memory
#define ESPRADIO_USE_LITTLEFS // comente para desabilitar

#if defined( ESPRADIO_USE_LITTLEFS )
#include <FS.h>
#include <LittleFS.h>
#endif


// ========================================================================================
// CALLBACK external
// ========================================================================================
#if defined(ESP32)
void ESP_RADIO_onRecive(const uint8_t * mac,const uint8_t *incomingData, int len);
#elif defined(ESP8266)
void ESP_RADIO_onRecive(uint8_t * mac, uint8_t *incomingData, uint8_t len);
#endif

// ==/  Structs  /=====================================================================

/*/
    siginificados:
    
      code: Codigo identificador do "protocolo" da biblioteca espRadio.
      len:  Quantidade de canais
      ID:   Modo de conexão
            0 até 100 -> envio de dados porta 0
            101       -> envio de dados para realizar bind do TX para o RX
            102       -> envio de dados para realizar bind do RX para o TX
      MAC_rx: Codigo MAC de destino do pacote
            00:00:00:00:00:00 -> broadcasting

    Modos de Recepção:

      Normal: Verifica apena o MAC de destino, se for igual ao MAC local
              valida o pacote, se não ele é descartado.
      Porta:  Ignora o MAC de destino verifica apena o ID. Caso seja menor
              ou igual a 100 e coincida com a porta local selecionada o 
              pacote recebido é validado, caso contrário é descartado.
      Bind:   Neste modo o receptor aguarda a conexão de um transmissor
              isso é realizado através do envio de um pacode com ID=101.
              Ao receber o receptor salva o codigo MAC do TX e envia de 
              volta um pacote com ID=102.

/*/


// pacotes de comunicação --------------------

typedef struct pacote {
    int32_t code = 1804;
    int32_t len;
    int32_t ID;
    int32_t ch[20];
    uint8_t MAC_rx[6];
} pacote;

typedef struct pacote_bind { // pacote exclusivo para bind
    int32_t  code = 1804;
    int32_t  len;
    int32_t  ID;
    char     name[20];
    int32_t  device_ID;
    uint32_t color;
} pacote_bind;


// Dispositivos ------------------------------

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define Devices_max_len 16

typedef struct espRadio_device_t {
  uint8_t  MAC[6];
  int32_t  ID;
  char     name[20];
  uint32_t color;
  uint8_t  valid;
} espRadio_device_t;

typedef struct espRadio_config_t {
   uint8_t  Radio_role = 0; // RX
   uint8_t  telemetry  = false;
   uint32_t delay_bind = 150;
   uint32_t delay_send = 50;
   uint32_t delay_failsafe = 400;
   uint8_t  send_mode   = 0; // Broadcast
   uint8_t  recive_mode = 0; // NORMAL
   uint8_t  mac_target[6];
   uint8_t  port = 0;
} espRadio_config_t;

// ====================================================================================


class ESP_RADIO{
  
  public:

    //====/ ENUMs /==============================
    enum ACTIONS{
      
      RISE = 0,
      FALL, 
      RECIVE,
      SEND,
      
      CHANGE_RECIVE_MODE,
      CHANGE_SEND_MODE,
      
      BIND_ON,
      BIND_OFF,
      BINDED,
      BIND_FAIL,

      MEMORY_BEGIN,
      SAVED,
      
      READ_DEVICES,
      SAVE_DEVICES,
      RECOVER_DEVICES,

      READ_CONFIG,
      SAVE_CONFIG,
      RECOVER_CONFIG

    };

    enum RADIO_ID{
      RX = 0,
      TX,
      RX_telemetry,
      TX_telemetry,
    };

    enum RECIVE_MODES{
      NORMAL = 0,  // check MAC destino
      PORT         // só checa a porta "ID"
      // RECIVE_PEER       // realiza pedido de conexão e pareamento com confirmação de TX e RX
    };

    enum SEND_MODES{
      SEND_BROADCAST = 0,
      SEND_DEVICE
    };

    //====/ ENUMs /=========================================

    //====/ begin and deinit /==============================

    espRadio_config_t config;
    
    void deinit(){
      esp_now_deinit();
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
    }
    
    void begin( int (*f)(int) ){

      handle_espRadio_f = f;
      WiFi.macAddress(Devices[0].MAC); // get self mac

      // get data from memory
      MEMORY_begin();
      read_config();
      read_devices();

      Serial.printf( "\n\n|X| RADIO CONFIG\n" );
      Serial.printf( " | Name            %s\n", Devices[0].name );
      Serial.printf( " | Radio role      %s\n", (config.Radio_role?"TX":"RX") );
      Serial.printf( " | Telemetry       %s\n", (config.telemetry?"ON":"OFF") );
      Serial.printf( " | Delay Failsafe  %d ms\n", config.delay_failsafe );
      Serial.printf( " | Delay Send      %d ms\n", config.delay_send );
      Serial.printf( " | Delay Bind      %d ms\n", config.delay_bind );
      Serial.printf( " | Recive_mode     %s\n", (config.recive_mode==NORMAL?"NORMAL":"PORT") );      
      Serial.printf( " | Send_mode       %s\n", (config.send_mode==SEND_BROADCAST?"BROADCAST":"DEVICE") );
      Serial.printf( " | Port            %d\n", config.port );
      Serial.printf( " | MAC target      %02x:%02x:%02x:%02x:%02x:%02x\n\n",
        config.mac_target[0],
        config.mac_target[1],
        config.mac_target[2],
        config.mac_target[3],
        config.mac_target[4],
        config.mac_target[5]
      );
      
      // Init ESPNOW --------------------------------
      WiFi.disconnect();
      WiFi.mode(WIFI_STA);
      if(esp_now_init() != 0){
        Serial.println("[Error] initializing ESP-NOW");
        return;
      }
      Serial.println("\n\nBEGIN ESPNOW!!");


      // Config. self device ------------------------
      if( config.Radio_role == RX  ){ // Begin ESPNOW RX
        
        #if defined(ESP8266)
        esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
        #endif

        enable_reciving();

        if( config.telemetry ) enable_sending();

      }else if( config.Radio_role == TX ){ // Begin ESPNOW TX
      
        #if defined(ESP8266)
        esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
        #endif
        
        enable_sending();

        if( config.telemetry ) enable_reciving();

      }

      Serial.printf( "[ begin VERIFICANDO ] PEER BROADCAST: %d\n", esp_now_is_peer_exist(broadcastAddress) );

      //if( mode == 0 ) esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
    }


    //// MODOS ////////////////////////////////////////////////////////////

    void enable_telemetry(){
      config.telemetry = true;
      if( config.Radio_role == RX ){
        enable_sending();
      }else if( config.Radio_role == TX ){
        enable_reciving();
      }
    }

    void disable_telemetry(){
      config.telemetry = false;
      if( config.Radio_role == RX ){
        disable_sending();
      }else if( config.Radio_role == TX ){
        disable_reciving();
      }
    }

    int recive_mode(){
      return config.recive_mode;
    }

    void recive_mode( int _mode ){

      if( _mode == NORMAL || _mode == NORMAL ){
        config.recive_mode = _mode;
        call( CHANGE_RECIVE_MODE );
      }
    }

    //// BIND /////////////////////////////////////////////////////////////

    boolean binding(){ return Flag_bind; }
    boolean bind(){ return Flag_bind; }
    void bind(boolean _bind){
      if(_bind) bind_on(); else bind_off();
    }

    void bind_on(){
      
      Flag_bind = true;
      call( BIND_ON );
      
      if( Flag_online ) call( FALL );

      // update pack_bind
      memcpy(pack_bind.name, Devices[0].name, 20);
      pack_bind.color = Devices[0].color;

      if( config.Radio_role == TX ){
        pack_bind.ID = 101;
        enable_reciving();
      }else{
        enable_sending();
        Serial.printf( "[ BIND_ON VERIFICANDO ] PEER BROADCAST: %d\n", esp_now_is_peer_exist(broadcastAddress) );
        pack_bind.ID = 102;
      }
    }

    void bind_off(){
      Flag_bind = false;
      Flag_bind_end = false;
      if( config.Radio_role == TX && !config.telemetry ){ disable_reciving(); }
      if( config.Radio_role == RX && !config.telemetry ){ disable_sending();  }
      call( BIND_OFF );
    }

    /// Dispositivos /////////////////////////////////////////////////////////////////

    void print_device( espRadio_device_t D ){
      char mac[18];
      snprintf(mac, 18, "%02x:%02x:%02x:%02x:%02x:%02x", D.MAC[0], D.MAC[1], D.MAC[2], D.MAC[3], D.MAC[4], D.MAC[5]);
      Serial.printf( "[DEVICE][ MAC: %s ][ Name: %s ][ ID:%d Color:#%06x ][ %s ]\n", mac, D.name, D.ID, D.color, D.valid ? "valido":"invalido" );
    }

    espRadio_device_t * device( uint8_t i ){ return ( i>=Devices_max_len ?  Devices+Devices_max_len-1 : Devices+i ); }

    espRadio_device_t device_connect(){
      return Devices[device_connect_number];
    }

    void self_device( const char *_name, int ID, uint32_t color ){
      memcpy( Devices[0].name, _name, 20);
      Devices[0].ID    = ID;
      Devices[0].color = color;
    }

    void add_device( const char *_name, int ID, uint32_t color, uint8_t _MAC_0, uint8_t _MAC_1, uint8_t _MAC_2, uint8_t _MAC_3, uint8_t _MAC_4, uint8_t _MAC_5 ){
      uint8_t mac[6];
      mac[0] = _MAC_0;
      mac[1] = _MAC_1;
      mac[2] = _MAC_2;
      mac[3] = _MAC_3;
      mac[4] = _MAC_4;
      mac[5] = _MAC_5;
      add_device( _name, ID, color, mac );
    }

    boolean add_device( const char *_name, int ID, uint32_t color, uint8_t *mac ){
      int i = check_mac( mac );
      if( i < 0 ){
        if( Devices_len < Devices_max_len ){
          i = Devices_len;
        }else{
          return false;
        }
      }
      add_device( _name, ID, color, mac, i );
      return true;
    }

    void add_device( const char *_name, int ID, uint32_t color, uint8_t *mac, uint8_t _index ){
      
      if( _index >= Devices_max_len ) return;

      String name = _name;

      int pos = name.indexOf(',');
      while( pos >= 0 ){ name.remove(pos,1); pos = name.indexOf(','); }

      memcpy( Devices[_index].MAC, mac, 6);
      memcpy( Devices[_index].name, name.c_str(), 20);
      Devices[_index].ID    = ID;
      Devices[_index].color = color;
      
      if( _index == Devices_len ) Devices_len++;

    }

    //// MAC functions ////

    int check_mac(uint8_t * mac){
      for(uint8_t i=0;i<Devices_len;i++){
        if( memcmp( mac, Devices[i].MAC, 6 ) == 0 ) return i;
      }
      return -1; 
    }

    void print_MAC(){ Serial.print("RADIO MAC: " + WiFi.macAddress() ); }
    void print_MAC( uint8_t *mac ){
      char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      Serial.printf("[ MAC: %s ]",macStr);
    }

    /// Funções basicas ESPNOW /////////////////////////////////////////////////////

    boolean peer( uint8_t *mac, boolean combo ){
      boolean ok = true;
      #if defined(ESP32)
        memcpy(espnow_local_info.peer_addr, mac, 6);
        espnow_local_info.channel = 0;
        espnow_local_info.encrypt = false;
        esp_err_t Status = esp_now_add_peer(&espnow_local_info);
        //log_err(Status);
        ok = (Status == ESP_OK || Status == ESP_ERR_ESPNOW_EXIST );
      #elif defined(ESP8266)
        esp_now_add_peer( mac, ( combo ? ESP_NOW_ROLE_COMBO : ESP_NOW_ROLE_SLAVE ), 1, NULL, 0);
      #endif
      return ok;
    }

    boolean del_peer( uint8_t *mac ){
      boolean ok = true;
      #if defined(ESP32)
        esp_err_t Status = esp_now_del_peer(mac);
        //log_err(Status);
        ok = (Status == ESP_OK || Status == ESP_ERR_ESPNOW_EXIST );
      #elif defined(ESP8266)
        esp_now_del_peer( mac );
      #endif
      return ok;
    }

    /// Envio de pacotes /////////////////////////////////////////////////////////////////
    
    pacote      pack_tx;
    pacote_bind pack_bind;

    void set_mac_target( uint8_t i ){
      if(i>=Devices_len) return;
      memcpy(config.mac_target,Devices[i].MAC,6);
      peer( config.mac_target, true );
    }

    void set_mac_target( uint8_t *mac ){
      memcpy(config.mac_target,mac,6);
      peer( config.mac_target, true );
    }
    
    void send(){
      memcpy( pack_tx.MAC_rx, config.mac_target, 6 );
      uint8_t *mac = broadcastAddress;
      if( config.send_mode != SEND_BROADCAST ){
        if( config.Radio_role == RX ){
          if( config.Radio_role == RX ){
            if( device_connect_number == 0 ) return;
            mac = Devices[device_connect_number].MAC;
          }else{
            mac = config.mac_target;
          }
        }
      }
      esp_now_send( mac, (uint8_t *) &pack_tx, sizeof(pacote));
      #if defined(ESP8266)
      if( config.Radio_role == RX ) esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
      #endif
    }

    void send_bind(){
      if( config.Radio_role == TX ){
        pack_bind.ID = 101;
        esp_now_send( broadcastAddress, (uint8_t *) &pack_bind, sizeof(pack_bind));
      }else{
        pack_bind.ID = 102;
        esp_now_send( Devices[device_connect_number].MAC, (uint8_t *) &pack_bind, sizeof(pack_bind) );
        //esp_now_send( broadcastAddress, (uint8_t *) &pack_bind, sizeof(pack_bind) );
      }
    }

    /// Atualização /////////////////////////////////////////////////////////////////
    pacote   pack_rx;
    uint16_t device_connect_number = 0;

    boolean online(){return Flag_online;}

    int call(int action){
      return ( handle_espRadio_f == nullptr ? 0 : handle_espRadio_f( action ) );
    }

    void update(){
      
      if( Flag_bind ){
        if( Flag_bind_end ){
          
          peer( Devices[device_connect_number].MAC, true );
          delay(100);

          // Se for RX envia um pacote de bind de volta
          if( config.Radio_role == RX ){
            send_bind(); delay(60);
            send_bind(); delay(60);
            send_bind(); delay(60);
          }

          // encerra o bind
          bind_off();
          call( BINDED );

        }else{
          if( config.Radio_role == TX ){
            if( millis() >= bind_timeout ){
              bind_timeout = millis() + config.delay_bind;
              send_bind();
            }
          }
        }
      }

      if( !Flag_bind && Flag_send ){
        if( millis() >= send_timeout ){
          send_timeout = millis() + config.delay_send;
          call(SEND);
          send();
        }
      }
      
      if( !Flag_bind && Flag_online ){
        if( millis() >= failsafe_timeout ){
          Flag_online = false;
          del_peer( Devices[device_connect_number].MAC );
          device_connect_number = 0;
          call( FALL );
        }
      }

    }

    /// Recebimento de pacotes /////////////////////////////////////////////////////////////////

    void onRecive(const uint8_t * _mac, const uint8_t *incomingData, uint8_t len){
      
      // Copy MAC
      uint8_t mac[6];
      memcpy(mac,_mac,6);
      
      // Device check
      int device_i = check_mac(mac);
      
      // atualiza
      if( Flag_bind || device_i >= 0 ){

        // recebe pacote
        pacote pack;
        memcpy(&pack, incomingData, sizeof(pack));
        
        if(pack.code == 1804){

          // verifica bind
          if( Flag_bind ){

            if( ( config.Radio_role == RX && pack.ID == 101 ) || ( config.Radio_role == TX && pack.ID == 102 ) ){ // TX -> RX , RX -> TX
              // obtem informações de bind
              pacote_bind PB;
              memcpy(&PB, incomingData, sizeof(PB));

              // Caso seja novo, verifica se existe espaço
              if( device_i < 0 && Devices_len < Devices_max_len ){
                device_i = Devices_len;
                Devices_len++;
              }

              if( device_i >= 0 ){
                
                // adiciona o dispositivo e salva na flash
                add_device( PB.name, PB.device_ID, PB.color, mac, device_i );
                save_devices();

                // pareia para poder enviar pacotes diretamente
                device_connect_number = device_i;
                
                Flag_bind_end = true;

              }else{
                call( BIND_FAIL );
              }

            }

          }else if( pack.ID <= 100 ){ // reciving...

            boolean valid = false;
            
            if( config.recive_mode == NORMAL ){
              valid = (check_mac( pack.MAC_rx ) == 0);
            }else if( config.recive_mode == PORT ){
              valid = ( config.port == pack.ID );
            }

            if( valid ){
              Serial.print( " [VALIDO] " );
              if(!Flag_online){
                device_connect_number = device_i;
                Flag_online = true;
                call( RISE );
                peer( Devices[device_connect_number].MAC, true );
              }
              failsafe_timeout = millis() + config.delay_failsafe;
              pack_rx = pack;
              call( RECIVE );
            }

          }

        }
      }
      
      // print
      Serial.printf("Bytes received: %d ",len);
      print_MAC( mac );
      Serial.printf("[ Device: %d ]\n",device_i);
    }


    //// MEMORY ///////////////////////////////////////////////////////////////

    // BEGIN ------------------------------------
    boolean MEMORY_begin(){  
      if( call( MEMORY_BEGIN ) <= 0 ) return false;
      #if defined( ESPRADIO_USE_LITTLEFS )
      if( !LITTLEFS_begin() ) return false;
      #endif
      return true;
    }

    // READ ------------------------------------
    boolean read_devices(){
      boolean ok = call( READ_DEVICES );
      #if defined( ESPRADIO_USE_LITTLEFS )
      ok = LITTLEFS_read_devices();
      #endif
      if(ok == 0){
        call( RECOVER_DEVICES );
        call( SAVE_DEVICES );
        #if defined( ESPRADIO_USE_LITTLEFS )
        LITTLEFS_save_devices();
        #endif
        call( SAVED );
        return false;
      }
      return true;
    }

    boolean read_config(){
      boolean ok = call( READ_CONFIG );
      #if defined( ESPRADIO_USE_LITTLEFS )
      ok = LITTLEFS_read_config();
      #endif
      if(ok == 0){
        call( RECOVER_CONFIG );
        call( SAVE_CONFIG );
        #if defined( ESPRADIO_USE_LITTLEFS )
        LITTLEFS_save_config();
        #endif
        call( SAVED );
        return false;
      }
      return true;
    }

    // SAVE ------------------------------------

    void save_devices(){
      call( SAVE_DEVICES );
      #if defined( ESPRADIO_USE_LITTLEFS )
      LITTLEFS_save_devices();
      #endif
      call( SAVED );
    }

    void save_config(){
      boolean ok = call( SAVE_CONFIG );
      #if defined( ESPRADIO_USE_LITTLEFS )
      LITTLEFS_save_config();
      #endif
      call( SAVED );
    }

    void str_to_mac( const char *msg, uint8_t *mac ){
      String arg = msg;
      int pos = arg.indexOf(':');
      while( pos >= 0 ){ arg.remove(pos,1); pos = arg.indexOf(':'); }
      // read mac
      for(int k=5;k>=0;k--){
        if( arg.length() > 0 ){
          pos = arg.length()-2;
          if(pos<0) pos = 0;
          mac[k] = strtoul( arg.substring( pos ).c_str(), 0, 16 );
          arg.remove(pos,2);
        }
      }
    }

    //// LITTLEFS /////////////////////////////////////////////////////////////
    #if defined( ESPRADIO_USE_LITTLEFS )

    boolean LITTLEFS_begin(){
      // Serial.println("Mount LittleFS");
      #ifdef ESP32
      if (!LittleFS.begin(true)){
        Serial.println("[ERROR] LittleFS mount failed!");
        return false;
      }
      #else
      if (!LittleFS.begin()){
        Serial.println("[ERROR] LittleFS mount failed!");
        LittleFS.format();
        return false;
      }
      #endif
      return true;
    }

    boolean LITTLEFS_writeFile(const char * path, const char * message){
      Serial.printf("Writing file: %s\r\n", path);
      File file = LittleFS.open(path, "w");
      if(!file){
          Serial.println("- failed to open file for writing");
          return false;
      }
      if(file.print(message)){
          Serial.println("- file written");
      } else {
          Serial.println("- write failed");
      }
      file.close();
      return true;
    }

    boolean LITTLEFS_readFile(const char * path, String *msg ){
      Serial.printf("[LITTLEFS] Reading file: %s\r\n", path);
      
      File file = LittleFS.open(path,"r");
      if(!file || file.isDirectory()){
          Serial.println(" - failed to open file for reading");
          return false;
      }
      *msg = file.readString();
      file.close();

      Serial.println(" - ok");
      return true;
    }

    // READ ------------------------------
    boolean LITTLEFS_read_devices(){
      
      // le o arquivo
      String content;
      
      if( !LITTLEFS_readFile(path_devices,&content) ) return false;

      // printa o arquivo
      Serial.println( "Reading devices from flash..." );
      Serial.println( "[FILE] Devices:\n" + content + "\n" );

      int i = -1;
      int i_end = content.indexOf( '\n' );

      while( i_end >= 0 ){
        
        String line = content.substring(0,i_end);

        if( i >= 0 ){ // pula o cabeçalho
          int i_begin = 0;

          boolean erro = false;

          for(int j=0;j<4;j++){

            int i_virgula = line.indexOf(',',i_begin);
            
            if( (j<3) && (i_virgula<0) ){ erro = true; break; }

            if( j == 0 && i > 0 ){ // MAC
              String mac = line.substring(i_begin,i_virgula);
              mac.trim();
              str_to_mac( mac.c_str(), Devices[i].MAC );

            } else if( j == 1 ) { // Name
              String name = line.substring(i_begin,i_virgula);
              name.trim();
              name.toCharArray(Devices[i].name, 20);
              //Serial.printf( "\nSAVE NAME: %s\n", Devices[i].name );
            } else if( j == 2 ) { // ID
              String ID = line.substring(i_begin,i_virgula);
              ID.trim();
              Devices[i].ID = ID.toInt();
            } else { // Color
              String color = line.substring(i_begin,i_end);
              color.trim();
              if( color.charAt(0) == '#' ) color.remove(0,1);
              Devices[i].color = strtoul( color.c_str(), 0, 16);
            }
            i_begin = i_virgula+1;
          }
          
          Devices[i].valid = !erro;

          print_device( Devices[i] );

        }

        i++; if( i >= Devices_max_len ) break;
        
        content.remove(0,i_end+1);
        i_end = content.indexOf('\n');

      }

      Devices_len = i;
      
      return true;
    }

    boolean LITTLEFS_read_config(){
      // le o arquivo
      String content;
      if( !LITTLEFS_readFile(path_config,&content) ) return false;
      Serial.println( "[FILE] Config:\n" + content + "\n" );

      int i = 0;
      int i_end = content.indexOf( '\n' );

      while( i_end >= 0 ){
        
        String line = content.substring(0,i_end);

        if( i > 0 ){ // pula o cabeçalho
          int i_begin = 0;

          boolean erro = false;

          for(int j=0;j<9;j++){

            int i_virgula = line.indexOf(',',i_begin);
            
            if( (j<3) && (i_virgula<0) ){ erro = true; break; }

            String arg = ( i_virgula >= 0 ? line.substring(i_begin,i_virgula) : line.substring(i_begin) );
            arg.trim();
            Serial.printf("[arg %d] %s\n",j,arg.c_str());

                 if( j == 0 ) { config.Radio_role     = arg.toInt(); }
            else if( j == 1 ) { config.telemetry      = arg.toInt(); }
            else if( j == 2 ) { config.delay_failsafe = arg.toInt(); }
            else if( j == 3 ) { config.delay_send     = arg.toInt(); }
            else if( j == 4 ) { config.delay_bind     = arg.toInt(); }
            else if( j == 5 ) { config.send_mode      = arg.toInt(); }
            else if( j == 6 ) { config.recive_mode    = arg.toInt(); }
            else if( j == 7 ) { config.port           = arg.toInt(); }
            else if( j == 8 ) { str_to_mac( arg.c_str(), config.mac_target ); }

            i_begin = i_virgula+1;
          }

        }

        i++; if( i >= 2 ) break;
        
        content.remove(0,i_end+1);
        i_end = content.indexOf('\n');

      }

      Devices_len = i;
      
      return true;
    }

    
    void LITTLEFS_save_config(){
      
      File file = LittleFS.open(path_config, "w");
      file.println( "Radio role, Telemetry, Delay faillsafe[ms], Delay send[ms], Delay bind[ms], Send_mode, Recive_mode, Port, MAC target" );

      file.printf(
        "%d, %d, %d, %d, %d, %d, %d, %d, %02x:%02x:%02x:%02x:%02x:%02x\n",
        config.Radio_role,
        config.telemetry,
        config.delay_failsafe,
        config.delay_send,
        config.delay_bind,
        config.send_mode,
        config.recive_mode,
        config.port,
        config.mac_target[0],
        config.mac_target[1],
        config.mac_target[2],
        config.mac_target[3],
        config.mac_target[4],
        config.mac_target[5]
      );

      file.close();

    }

    void LITTLEFS_save_devices(){
      
      File file = LittleFS.open(path_devices, "w");
      file.println( "MAC, Name, ID, Color" );

      for(int i;i<Devices_len;i++){

        file.printf(
          "%02x:%02x:%02x:%02x:%02x:%02x, %s, %d, %06X\n",
          Devices[i].MAC[0], Devices[i].MAC[1], Devices[i].MAC[2], Devices[i].MAC[3], Devices[i].MAC[4], Devices[i].MAC[5],
          Devices[i].name,
          Devices[i].ID,
          Devices[i].color
        );

      }

      file.close();

    }

    #endif
    ///////////////////////////////////////////////////////////////////////////

    
  private:
    
    void enable_reciving(){
      Flag_recive = true;
      esp_now_register_recv_cb(ESP_RADIO_onRecive);
    }

    void disable_reciving(){
      Flag_recive = false;
      esp_now_unregister_recv_cb();
    }

    void enable_sending(){
      Flag_send = true;
      peer( broadcastAddress, true );
    }

    void disable_sending(){
      Flag_send = false;
      del_peer( broadcastAddress );
    }

    const char * path_devices = "/espRadio_devices.csv";
    const char * path_config  = "/espRadio_config.csv";
    
    // Device list
    //uint8_t Device_last_connect = 1;
    uint8_t Devices_len = 1;
    espRadio_device_t Devices[Devices_max_len];

    // timeouts
    uint32_t failsafe_timeout = 0;
    uint32_t bind_timeout = 0;
    uint32_t send_timeout = 0;
    
    // Flags
    //boolean Flag_change = false;
    boolean Flag_online = false;
    boolean Flag_send   = false;
    boolean Flag_recive = false;
    boolean Flag_bind   = false;
    boolean Flag_bind_end = false;

    // Callback function
    int (*handle_espRadio_f)(int) = nullptr;
    
};

ESP_RADIO espRadio;

// onRecive //////////////////////////////////////////////////////////////////
#if defined(ESP32)
void ESP_RADIO_onRecive(const uint8_t * mac,const uint8_t *incomingData, int len){ espRadio.onRecive(mac,incomingData,len); }
#elif defined(ESP8266)
void ESP_RADIO_onRecive(uint8_t * mac, uint8_t *incomingData, uint8_t len){ espRadio.onRecive(mac,incomingData,len); }
#endif


#endif
#endif