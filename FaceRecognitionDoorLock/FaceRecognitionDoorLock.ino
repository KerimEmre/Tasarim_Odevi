#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

//GEREKLİ KÜTÜPHANE TANIMLAMALARI

#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

#define RED 13
#define GREEN 14
#define LOCK 12
#define FLASH_LED 4

//Wifi id ve şifresi
const char* ssid = "asd"; 
const char* password = "hakan123"; 


boolean intruder = false;
WiFiClientSecure clientTCP;

//Bot ile bağlantıyı sağlamak için telegram botunun id ve token'ı

String chatId = "899333340";
String BOTtoken = "6458808775:AAEZHfPCD3nzTMxg1YvXAARIgtQA-yt_JGI";


//Mesaj zamanlaması için ayarlamalar
bool sendPhoto = false;
UniversalTelegramBot bot(BOTtoken, clientTCP);
int lockState = 0;
String r_msg = "";
const unsigned long BOT_MTBS = 1000; 
unsigned long bot_lasttime;

void handleNewMessages(int numNewMessages);
String sendPhotoTelegram();


//Bottaki unlock komutu ile kilidi açacak olan fonksiyon
String unlockDoor(){  
 if (lockState == 0) {
  digitalWrite(LOCK, HIGH);
  digitalWrite(GREEN,HIGH);
  digitalWrite(RED,LOW);
  lockState = 1;
  delay(100);
  return "Door Unlocked. /lock";
 }
 else{
  return "Door Already Unlocked. /lock";
 }  
}

//Bottaki lock komutu ile kilidi kitleyecek olan fonksiyon

String lockDoor(){
 if (lockState == 1) {
  digitalWrite(LOCK, LOW);
  digitalWrite(GREEN,LOW);
  digitalWrite(RED,HIGH);
  lockState = 0;
  delay(100);
  
  return "Door Locked. /unlock";
 }
 else{
  return "Door Already Locked. /unlock";
 }
}


//Fotoğraf gönderecek olan fonksiyon ve gerekli bağlantı işlemleri
String sendPhotoTelegram(){
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }  
  
  Serial.println("Connect to " + String(myDomain));

  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Connection successful");
    
   Serial.println("Connected to " + String(myDomain));
    
    String head = "--IotCircuitHub\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + chatId + "\r\n--IotCircuitHub\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--IotCircuitHub--\r\n";

    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
  
    clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=IotCircuitHub");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    
    clientTCP.print(tail);
    
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()){
      Serial.print(".");
      delay(100);      
      while (clientTCP.available()){
          char c = clientTCP.read();
          if (c == '\n'){
            if (getAll.length()==0) state=true; 
            getAll = "";
          } 
          else if (c != '\r'){
            getAll += String(c);
          }
          if (state==true){
            getBody += String(c);
          }
          startTimer = millis();
       }
       if (getBody.length()>0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody="Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }
  return getBody;
}


//Atılacak mesajlar için gerekli fonksiyon tanımlamaları ve bağlantı işlemleri
void handleNewMessages(int numNewMessages){
  Serial.print("Handle New Messages: ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++){
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != chatId){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Ulaşan mesajı yazdırma
    String text = bot.messages[i].text;
    Serial.println(text);

    String fromName = bot.messages[i].from_name;
    if (text == "/photo") {
      sendPhoto = true;
      Serial.println("New photo request");
    }
    if (text == "/lock"){
      String r_msg = lockDoor();
      bot.sendMessage(chatId, r_msg, "");
    }
    if (text == "/unlock"){
      String r_msg = unlockDoor();
      bot.sendMessage(chatId, r_msg, "");
    }
    if (text == "/start"){
      String welcome = "Welcome to the ESP32-CAM Telegram Smart Lock.\n";
      welcome += "/photo : Takes a new photo\n";
      welcome += "/unlock : Unlock the Door\n";
      welcome += "/lock : Lock the Door\n";
      welcome += "To get the photo please tap on /photo.\n";
      bot.sendMessage(chatId, welcome, "Markdown");
    }
  }
}



void startCameraServer();

boolean matchFace = false;
boolean openLock = false;

long prevMillis=0;
int interval = 6000;  //DELAY

void setup() {
  pinMode(LOCK,OUTPUT);
  pinMode(RED,OUTPUT);
  pinMode(GREEN,OUTPUT);
  pinMode(FLASH_LED,OUTPUT);
  digitalWrite(LOCK,LOW);
  digitalWrite(RED,HIGH);
  digitalWrite(GREEN,LOW);
  
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
 
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif


  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
 
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

void loop() {
  
  //Fotoğraf gönderme
  if (sendPhoto){
    Serial.println("Preparing photo");
    digitalWrite(FLASH_LED, HIGH);
    delay(50);
    sendPhotoTelegram(); 
    digitalWrite(FLASH_LED, LOW);
    sendPhoto = false; 
  }
  
  //İntruder alert verdiği zaman kilit de kapalıysa kişi tanımlanamadığı için bottan bize mesaj gönderiyor
  if(intruder == true && openLock==false)
  {
    Serial.println("Preparing photo");
    sendPhotoTelegram(); 
    sendPhoto = false;
    intruder = false;
  }
    
  //Yüz tanımlanırsa ve kilit kapalıysa kilidi açıyor
  if(matchFace==true && openLock==false)
  {
    openLock=true;
    digitalWrite(LOCK,HIGH);
    digitalWrite(GREEN,HIGH);
    digitalWrite(RED,LOW);
    prevMillis=millis();
    Serial.print("UNLOCK DOOR");    
   }

  //Kilit açıkken intruder uyarısı alıp da boş yere mesaj atmasın diye eklendi
  if(openLock==true){
    intruder = 0;
  }

  //Yüzü tanıyıp kilidi açtıktan belirli bir süre sonra kilidi otomatik olarak kapatması için gerekli fonksiyon
  if (openLock == true && millis()-prevMillis > interval)
   {
    openLock=false;
    matchFace=false;
    digitalWrite(LOCK,LOW);
    digitalWrite(GREEN,LOW);
    digitalWrite(RED,HIGH);
    Serial.print("LOCK DOOR");
    
    }


  //Botun mesajları için gerekli zaman kontrollerini yapan fonksiyon
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }
}
