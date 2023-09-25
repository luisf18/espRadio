#ifndef ESPNOW_H
#define ESPNOW_H

#include "Arduino.h"

/*/  
    
    espRadio V2.0

    RX: ok (apenas recebe pacotes)

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

// ========================================================================================
// CALLBACK external
// ========================================================================================
#if defined(ESP32)
void ESP_RADIO_onRecive(const uint8_t * mac,const uint8_t *incomingData, int len);
#elif defined(ESP8266)
void ESP_RADIO_onRecive(uint8_t * mac, uint8_t *incomingData, uint8_t len);
#endif

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

//typedef struct espRadio_MAC_t{
//  
//}espRadio_MAC_t;

// Structure V2.0
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


#define Devices_max_len 16

// Struct Devices
typedef struct espRadio_device_t {
  uint8_t  MAC[6];
  int32_t  ID;
  char     name[20];
  uint32_t color;
  uint8_t  valid;
} espRadio_device_t;

typedef struct espRadio_config_t {
   int32_t type;
   int32_t color;
   int32_t ID;
   char    name[20];
} espRadio_config_t;


// ==/  Memory data  /=====================================================================

#define ESPRADIO_USE_LITTLEFS // comente para desabilitar

#if defined( ESPRADIO_USE_LITTLEFS )
#include <FS.h>
#include <LittleFS.h>
#endif

// ========================================================================================


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

class ESP_RADIO{
  public:

    enum ACTIONS{
      
      RISE = 0,
      FALL, 
      RECIVE,
      SEND,
      
      START_MODE_NORMAL,
      START_MODE_PORT,
      
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

    enum TYPE{
      RX = 0,
      TX,
      RX_monitor,
      TX_monitor,
    };

    enum MODES{
      NORMAL = 0, // check MAC destino
      PORT,       // só checa a porta "ID"
      PEER        // realiza pedido de conexão e pareamento com confirmação de TX e RX
    };

    ////// ESPNOW /////////////////////////////////////////////////////////////////
    void deinit(){
      esp_now_deinit();
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
    }
    
    void begin( int _type, int (*f)(int) ){

      // get self mac
      WiFi.macAddress(Devices[0].MAC);
      // Serial.print("SELF MAC: ");
      // print_MAC(Devices[0].MAC);
      // Serial.println(" <<");

      type = _type;
      function_type = _type;
      handle_espRadio_f = f;

      MEMORY_begin();
      //read_config();
      read_devices();

      mode( NORMAL );
      Serial.println("ESPNOW - iniciando...");
      WiFi.disconnect();
      WiFi.mode(WIFI_STA);
      if(esp_now_init() != 0){ Serial.println("Error initializing ESP-NOW"); return; }
      Serial.println("\n\nBEGIN ESPNOW!!");

      if( function_type == RX  ){ // Begin ESPNOW RX
        #if defined(ESP8266)
        esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
        #endif
        esp_now_register_recv_cb(ESP_RADIO_onRecive);
        peer( broadcastAddress, true );
        if(type == RX_monitor) Flag_send = true;
        pack_bind.ID = 102;
      }else if( function_type == TX ){ // Begin ESPNOW TX
        Flag_send = true;
        #if defined(ESP8266)
        esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
        #endif
        peer( broadcastAddress, true );
        if( type == TX_monitor ){
          esp_now_register_recv_cb(ESP_RADIO_onRecive);
        }
      }
      //if( mode == 0 ) esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
    }


    //// BIND /////////////////////////////////////////////////////////////

    void set_reciving(boolean on){
      if( on ) esp_now_register_recv_cb(ESP_RADIO_onRecive);
      else esp_now_unregister_recv_cb();
    }

    boolean binding(){
      return Flag_bind;
    }

    boolean bind(){
      return Flag_bind;
    }
    
    void bind(boolean _bind){
      if(_bind) bind_on(); else bind_off();
    }

    void bind_on(){
      Flag_bind = true;
      call( BIND_ON );
      memcpy(pack_bind.name, Devices[0].name, 20);
      pack_bind.color = Devices[0].color;
      if( function_type == TX ){
        pack_bind.ID = 101;
        set_reciving(true);
      }else{
        pack_bind.ID = 102;
      }
    }

    void bind_off(){
      Flag_bind = false;
      call( BIND_OFF );
      if( type == TX ){ esp_now_unregister_recv_cb(); }
    }

    int mode(){
      return Mode;
    }

    void mode( int _mode ){
      switch(_mode){
        case NORMAL: if( handle_espRadio_f != nullptr ) handle_espRadio_f( START_MODE_NORMAL ); Mode = NORMAL; break;
        case PORT:   if( handle_espRadio_f != nullptr ) handle_espRadio_f( START_MODE_PORT );   Mode = PORT;   break;
      }
    }

    int port(){
      return Port;
    }

    void port( uint8_t _port ){
      Port = _port;
    }

    ///////////////////////////////////////////////////////////////////////////////
    ////// TX MAC lists ///////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////
    // pacote pack_tx;

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

    pacote      pack_tx;
    pacote_bind pack_bind;

    void send_broadcast(){
      esp_now_send( broadcastAddress, (uint8_t *) &pack_tx, sizeof(pacote));
      if( function_type == RX ){
        #if defined(ESP8266)
        esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
        #endif
      }
    }
    
    void send(){
      if( function_type == RX ){
        esp_now_send( Devices[device_connect_number].MAC, (uint8_t *) &pack_tx, sizeof(pacote));
        #if defined(ESP8266)
        esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
        #endif
      }else{
        esp_now_send( broadcastAddress, (uint8_t *) &pack_tx, sizeof(pacote));
      }
    }

    void send_bind(){
      if( type == TX ){
        esp_now_send( broadcastAddress, (uint8_t *) &pack_bind, sizeof(pack_bind));
      }else{
        esp_now_send( Devices[device_connect_number].MAC, (uint8_t *) &pack_bind, sizeof(pack_bind));
      }
    }

    ////// MAC list ///////////////////////////////////////////////////////////////
    pacote pack_rx;
    
    espRadio_device_t * device( uint8_t i ){ return ( i>=Devices_max_len ?  Devices+Devices_max_len-1 : Devices+i ); }

    void self_device( const char *_name, int ID, uint32_t color ){
      memcpy( Devices[0].name, _name, 20);
      Devices[0].ID     = ID;
      Devices[0].color = color;
    }

    void add_device( const char *_name, int ID, uint32_t color, uint8_t _MAC_0, uint8_t _MAC_1, uint8_t _MAC_2, uint8_t _MAC_3, uint8_t _MAC_4, uint8_t _MAC_5 ){
      add_device( _name, ID, color, _MAC_0, _MAC_1, _MAC_2, _MAC_3, _MAC_4, _MAC_5, Devices_len );
    }

    void add_device( const char *_name, int ID, uint32_t color, uint8_t _MAC_0, uint8_t _MAC_1, uint8_t _MAC_2, uint8_t _MAC_3, uint8_t _MAC_4, uint8_t _MAC_5, uint8_t _index ){
      if( _index >= Devices_max_len ) return;
      memcpy( Devices[_index].name, _name, 20);

      Serial.print( "NAME: " );
      Serial.println( Devices[_index].name );

      Devices[_index].ID     = ID;
      Devices[_index].color  = color;
      Devices[_index].MAC[0] = _MAC_0;
      Devices[_index].MAC[1] = _MAC_1;
      Devices[_index].MAC[2] = _MAC_2;
      Devices[_index].MAC[3] = _MAC_3;
      Devices[_index].MAC[4] = _MAC_4;
      Devices[_index].MAC[5] = _MAC_5;

      Devices_len++;
    }

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

    ////// RX /////////////////////////////////////////////////////////////////////
    uint16_t failsafe_delay = 400;
    uint16_t bind_delay     = 100;
    uint16_t send_delay     = 100;
    uint16_t device_connect_number = 0;

    espRadio_device_t device_connect(){
      return Devices[device_connect_number];
    }

    void update(){
      
      if( Flag_bind && function_type == TX ){
        if( millis() >= bind_timeout ){
          bind_timeout = millis() + bind_delay;
          send_bind();
        }
      }

      if( !Flag_bind && Flag_send ){
        if( millis() >= send_timeout ){
          send_timeout = millis() + send_delay;
          call(SEND);
          send();
        }
      }
      
      if( Flag_online ){
        if( millis() >= failsafe_timeout ){
          Flag_online = false;
          del_peer( Devices[device_connect_number].MAC );
          if( handle_espRadio_f != nullptr ) handle_espRadio_f( FALL );
        }
      }

    }

    void onRecive(const uint8_t * _mac, const uint8_t *incomingData, uint8_t len){
      
      uint8_t mac[6];
      memcpy(mac,_mac,6);
      
      // print
      Serial.printf("Bytes received: %d ",len);
      print_MAC( mac );
      
      // Device check
      int device_i = check_mac(mac);
      
      // print
      Serial.printf("[ Device: %d ]",device_i);
      
      // atua
      if( Flag_bind || device_i >= 0 ){

        pacote pack;
        memcpy(&pack, incomingData, sizeof(pack));
        
        if(pack.code == 1804){

          if( Flag_bind ){
            if( pack.ID == 101 || pack.ID == 102 ){ // TX -> RX , RX -> TX
              // send self mac to TX...
              Flag_bind = false;

              // Save new device
              pacote_bind PB;
              memcpy(&PB, incomingData, sizeof(PB));

              // Caso seja novo, verifica se existe espaço
              if( device_i < 0 && Devices_len < Devices_max_len ){
                device_i = Devices_len;
                Devices_len++;
              }

              if( device_i >= 0 ){
                memcpy(Devices[device_i].MAC, mac, 6);
                Devices[device_i].color = PB.color;
                Devices[device_i].ID    = PB.device_ID;
                memcpy(Devices[device_i].name, PB.name, 20);
                save_devices();
                bind_off();
                call( BINDED );

                peer( Devices[device_connect_number].MAC, true );
                
                if( pack.ID == 101 ){
                  send_bind();
                  send_bind();
                }

              }else{
                call( BIND_FAIL );
              }

            }
          }else if( pack.ID <= 100 ){ // reciving...

            boolean valid = false;

            if( Mode == NORMAL ){
              valid = (check_mac( pack.MAC_rx ) == 0);
            }else if( Mode == PORT ){
              valid = ( Port == pack.ID );
            }

            if( valid ){
              if(!Flag_online){
                device_connect_number = device_i;
                Flag_online = true;
                call( RISE );
                peer( Devices[device_connect_number].MAC, true );
              }
              failsafe_timeout = millis() + failsafe_delay;
              pack_rx = pack;
              call( RECIVE );
            }

          }

        }
      }
      // print
      Serial.println();
    }

    boolean online(){return Flag_online;}

    int call(int action){
      return ( handle_espRadio_f == nullptr ? 0 : handle_espRadio_f( action ) );
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

    //// LITTLEFS /////////////////////////////////////////////////////////////
    #if defined( ESPRADIO_USE_LITTLEFS )
    
    boolean LITTLEFS_begin(){
      // Serial.println("Mount LittleFS");
      if (!LittleFS.begin()){
        Serial.println("[ERROR] LittleFS mount failed!");
        return false;
      }
      return true;
    }

    // READ ------------------------------
    boolean LITTLEFS_read_devices(){
      
      // le o arquivo
      File file = LittleFS.open(path_devices,"r");
      if(!file){
        Serial.println("[ERRO] Failed to open file for reading");
        return false;
      }
      String content = file.readString();
      file.close();

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
              int pos = mac.indexOf(':');
              while( pos >= 0 ){ mac.remove(pos,1); pos = mac.indexOf(':'); }
              // read mac
              for(int k=5;k>=0;k--){
                if( mac.length() > 0 ){
                  pos = mac.length()-2;
                  if(pos<0) pos = 0;
                  Devices[i].MAC[k] = strtoul( mac.substring( pos ).c_str(), 0, 16 );
                  mac.remove(pos,2);
                }
              }

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
      File file = LittleFS.open(path_config,"r");
      if(!file){
        Serial.println("[ERRO] Failed to open file for reading");
        return false;
      }
      
      String content = file.readString();
      Serial.println( "[FILE] Devices:\n" + content + "\n" );
      
      file.close();
      return true;
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

    void LITTLEFS_save_config(){
      //...
    }

    #endif
    ///////////////////////////////////////////////////////////////////////////


    void print_device( espRadio_device_t D ){
      char mac[18];
      snprintf(mac, 18, "%02x:%02x:%02x:%02x:%02x:%02x", D.MAC[0], D.MAC[1], D.MAC[2], D.MAC[3], D.MAC[4], D.MAC[5]);
      Serial.printf( "[DEVICE][ MAC: %s ][ Name: %s ][ ID:%d Color:#%06x ][ %s ]\n", mac, D.name, D.ID, D.color, D.valid ? "valido":"invalido" );
    }

    
  private:
    
    const char * path_devices = "/espRadio_devices.csv";
    const char * path_config  = "/espRadio_config.csv";

    // basic config.
    uint8_t function_type = RX; // RX or TX
    uint8_t type = RX;
    uint8_t Mode = NORMAL;
    uint8_t Port = 0;
    
    // Device list
    //uint8_t Device_last_connect = 1;
    uint8_t Devices_len = 1;
    espRadio_device_t Devices[Devices_max_len];

    // timeouts
    uint32_t failsafe_timeout = 0;
    uint32_t bind_timeout = 0;
    uint32_t send_timeout = 0;
    
    // Flags
    boolean Flag_online = false;
    //boolean Flag_change = false;
    boolean Flag_bind   = false;
    boolean Flag_send   = false;

    // Callback function
    int (*handle_espRadio_f)(int) = nullptr;

    // RX ////////////////////////////
    // ...
    
    // TX ////////////////////////////
    // ...
    
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