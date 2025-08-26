#include "WiFi.h"
#include "WiFiUdp.h"
#include "parse_console.h"
#include "nvs.h"
#include "PPP.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <driver/i2s.h>
#include "array_process.h"

#define NRST_PIN 2
#define LED_PIN 32
#define BOOT_PIN 22

/*
Tested with 3.0.3 esp32 arduino board package by Espressif Systems.
*/

#define IPV4_ADDR_ANY   0x00000000UL

enum {PERIOD_CONNECTED = 50, PERIOD_DISCONNECTED = 1000};

WiFiUDP udp;
IPAddress server_address; //note: may want to change to our local IP, to support multiple devices on the network



// you shouldn't need to change these settings

#define SAMPLE_RATE 16000
// most microphones will probably default to left channel but you may need to tie the L/R pin low
#define I2S_MIC_CHANNEL I2S_CHANNEL_FMT_ONLY_LEFT
// either wire your microphone to the same pins or change these to match your wiring
#define I2S_MIC_SERIAL_CLOCK GPIO_NUM_14		//SCK
#define I2S_MIC_LEFT_RIGHT_CLOCK GPIO_NUM_13	//WS
#define I2S_MIC_SERIAL_DATA GPIO_NUM_12			//SD

// don't mess around with this
i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
};

// and don't mess around with this
i2s_pin_config_t i2s_mic_pins = {
    .bck_io_num = I2S_MIC_SERIAL_CLOCK,
    .ws_io_num = I2S_MIC_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SERIAL_DATA
};



void setup() {
  // put your setup code here, to run once:
  // pinMode(NRST_PIN, INPUT); //input for HI-Z
  pinMode(NRST_PIN, OUTPUT);
  digitalWrite(NRST_PIN, 1);

  pinMode(BOOT_PIN, OUTPUT); //input for HI-Z
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, 1);
  digitalWrite(BOOT_PIN, 0);
  init_prefs(&preferences, &gl_prefs);

  Serial.begin(2000000);  //this can stay 2mbps. WHICH IS CRAZY omg
  Serial2.begin(2000000, SERIAL_8N1, 16, 17);  //once you have a working system, try pushing this way higher (2MBPS is supported by ESP32!!)

  int connected = 0;
  Serial.printf("\r\n\r\n Trying \'%s\' \'%s\'\r\n",gl_prefs.ssid, gl_prefs.password);
  /*Begin wifi connection*/
  WiFi.mode(WIFI_STA);  
  WiFi.begin((const char *)gl_prefs.ssid, (const char *)gl_prefs.password);
  uint32_t timeout = millis() + 3000;
  while(millis() < timeout && WiFi.status() != WL_CONNECTED);
  //connected = WiFi.waitForConnectResult();
  if (connected != WL_CONNECTED) {
    Serial.printf("Connection to network %s failed for an unknown reason\r\n", (const char *)gl_prefs.ssid);
  }
  else {
    Serial.printf("Connected to network %s\r\n", (const char *)gl_prefs.ssid);
  }
  udp.begin(server_address, gl_prefs.port);
  server_address = IPAddress((uint32_t)IPV4_ADDR_ANY);


	i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
	i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);
}


int cmd_match(const char * in, const char * cmd)
{
  int i = 0;
  for(i = 0; cmd[i] != '\0'; i++)
  {
    if(in[i] == '\0')
      return -1;
    if(in[i] != cmd[i])
      return -1;   
  }
  return i;
}

#define UNSTUFFING_BUFFER_SIZE 256
#define PAYLOAD_BUFFER_SIZE ((UNSTUFFING_BUFFER_SIZE - 2)/2)  //max cap based on unstuffing buffer size
uint8_t gl_unstuffing_buffer[UNSTUFFING_BUFFER_SIZE] = {0};
uint8_t gl_pld_buffer[PAYLOAD_BUFFER_SIZE] = {0};


// put your main code here, to run repeatedly:
uint32_t led_ts = 0;
uint8_t led_state = 1;
uint8_t stm32_enabled = 0;
uint32_t blink_per = PERIOD_DISCONNECTED;
uint8_t udp_pkt_buf[256] = {0};
int ppp_stuffing_bidx = 0;

#define NUM_BYTES_HEADER		6	//two bytes sequence, four bytes epoch
#define NUM_BYTES_SAMPLE_BUFFER (512*sizeof(int32_t))
unsigned char pld_buf[NUM_BYTES_HEADER + NUM_BYTES_SAMPLE_BUFFER] = {};
unsigned char * p_raw_sample_buf = (unsigned char *)(&pld_buf[NUM_BYTES_HEADER]);

uint16_t sequence_number = 0;
uint32_t cnt = 0;


void loop() 
{	


	/*TODO: Consider calling this less frequently to reduce latency for real time traffic management*/
	size_t bytes_read = 0;
	i2s_read(I2S_NUM_0, p_raw_sample_buf, NUM_BYTES_SAMPLE_BUFFER, &bytes_read, portMAX_DELAY);
	if(bytes_read != 0)
	{
		//load sequence number
		sequence_number++;	//overflow is normal behvaior
		pld_buf[0] = (sequence_number & 0x00FF);
		pld_buf[1] = (sequence_number & 0xFF00) >> 8;

		//load millisecond epoch
		uint32_t tick = millis();
		pld_buf[0] = tick & 0x000000FF;
		pld_buf[1] = (tick & 0x0000FF00) >> 8;
		pld_buf[2] = (tick & 0x00FF0000) >> 16;
		pld_buf[3] = (tick & 0xFF000000 )>> 24;
		
		// process_32bit_to_16bit(pld_buf, bytes_read, &bytes_read);
		// dump the samples out to the serial channel.
		int32_t * raw_samples = (int32_t*)(&pld_buf[NUM_BYTES_HEADER]);
		for (int i = 0; i < bytes_read/4; i++)
		{
			Serial.printf("%ld\n", raw_samples[i]);
		}

		udp.beginPacket(udp.remoteIP(),udp.remotePort()+gl_prefs.reply_offset);
		udp.write((uint8_t*)pld_buf, NUM_BYTES_HEADER + bytes_read);
		udp.endPacket();
	}


  
  uint32_t ts = millis();

  int len = udp.parsePacket();
  if(len != 0)
  {
    int len = udp.read(udp_pkt_buf,255);
    
    int cmp = -1;
    cmp = cmd_match((const char *)udp_pkt_buf,"WHO_GOES_THERE");
    if(cmp > 0)
    {
      int len = strlen(gl_prefs.name);
      udp.beginPacket(udp.remoteIP(),udp.remotePort()+gl_prefs.reply_offset);
      udp.write((uint8_t*)gl_prefs.name,len);
      udp.endPacket();
    }
//   cmp = cmd_match((const char *)udp_pkt_buf,"SPAM_ME");
//     if(cmp > 0)
//     {
//       int len = strlen(gl_prefs.name);
//       udp.beginPacket(udp.remoteIP(),udp.remotePort()+gl_prefs.reply_offset);
//       udp.write((uint8_t*)"WAZZAAAP",8);
//       udp.endPacket();
//     }
    
    Serial2.write(udp_pkt_buf,len);
    for(int i = 0; i < len; i++)
      udp_pkt_buf[i] = 0;
  }

  uint8_t serial_pkt_sent = 0;
//   while(Serial2.available())
//   {
//       uint8_t new_byte = Serial2.read();
//       int pld_len = parse_PPP_stream(new_byte, gl_pld_buffer, PAYLOAD_BUFFER_SIZE, gl_unstuffing_buffer, UNSTUFFING_BUFFER_SIZE, &ppp_stuffing_bidx);
//       if(pld_len != 0)
//       {
//         if(gl_prefs.en_fixed_target == 0 && udp.remoteIP() != IPAddress(0,0,0,0))
//         {
//           udp.beginPacket(udp.remoteIP(), udp.remotePort()+gl_prefs.reply_offset);
//         }
//         else if (gl_prefs.en_fixed_target == 0 &&udp.remoteIP() == IPAddress(0,0,0,0))
//         {
//           udp.beginPacket(IPAddress(255,255,255,255), gl_prefs.port+gl_prefs.reply_offset);
//         }
//         else
//         {
//           IPAddress remote_ip(gl_prefs.remote_target_ip);
//           udp.beginPacket(remote_ip, gl_prefs.port+gl_prefs.reply_offset);
//         }
//         udp.write((uint8_t*)gl_pld_buffer, pld_len);
//         udp.endPacket();      
//         serial_pkt_sent = 1;
//       }
//   }

  get_console_lines();
  if(gl_console_cmd.parsed == 0)
  {
    uint8_t match = 0;
    uint8_t save = 0;
    int cmp = -1;

    //////////////////////////////////////////////////////////////////////////////////////////////////////
    cmp = strcmp((const char *)gl_console_cmd.buf,"ipconfig\r");
    if(cmp == 0)
    {
      match = 1;
      if(WiFi.status() == WL_CONNECTED)
      {
        Serial.printf("Connected to: %s\r\n", gl_prefs.ssid);
      }
      else
      {
        Serial.printf("Not connected to: %s\r\n", gl_prefs.ssid);
      }
      Serial.printf("UDP server on port: %d\r\n", gl_prefs.port);
      Serial.printf("Server Response Offset: %d\r\n", gl_prefs.reply_offset);
      Serial.printf("IP address is: %s\r\n", WiFi.localIP().toString().c_str());
  
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////
    cmp = cmd_match((const char *)gl_console_cmd.buf,"udpconfig\r");
    if(cmp > 0)
    {
      match = 1;
      Serial.printf("UDP server on port: %d\r\n", gl_prefs.port);
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    cmp = cmd_match((const char *)gl_console_cmd.buf,"setssid ");
    if(cmp > 0)
    {
      match = 1;
      const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
      /*Set the ssid*/
      for(int i = 0; i < WIFI_MAX_SSID_LEN; i++)
      {
        gl_prefs.ssid[i] = '\0';
      }
      for(int i = 0; arg[i] != '\0'; i++)
      {
        if(arg[i] != '\r' && arg[i] != '\n')  //copy non carriage return characters
        {
          gl_prefs.ssid[i] = arg[i];
        }
      }
      Serial.printf("Changing ssid to: %s\r\n", gl_prefs.ssid);
      save = 1;
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    cmp = cmd_match((const char *)gl_console_cmd.buf,"setname ");
    if(cmp > 0)
    {
      match = 1;
      const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
      /*Set the ssid*/
      for(int i = 0; i < DEVICE_NAME_LEN; i++)
      {
        gl_prefs.name[i] = '\0';
      }
      for(int i = 0; arg[i] != '\0'; i++)
      {
        if(arg[i] != '\r' && arg[i] != '\n')  //copy non carriage return characters
        {
          gl_prefs.name[i] = arg[i];
        }
      }
      Serial.printf("Changing device name to: %s\r\n", gl_prefs.name);
      save = 1;
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    cmp = cmd_match((const char *)gl_console_cmd.buf,"settargetip ");
    if(cmp > 0)
    {
      match = 1;
      const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
      char copy[15] = {0};
      Serial.printf("Raw str arg: ");
      for(int i = 0; arg[i] != 0 && arg[i] != '\r' && arg[i] != '\n'; i++)
      {
        copy[i] = arg[i];
        Serial.printf("%0.2X",arg[i]);
      }
      Serial.printf(": %s\r\n", copy);
      IPAddress addr;
      if(addr.fromString((const char *)copy) == true)
        Serial.printf("Parsed IP address successfully\r\n");
      else
        Serial.printf("Invalid IP string entered\r\n");
      gl_prefs.remote_target_ip = (uint32_t)addr;
      Serial.printf("%X\r\n",gl_prefs.remote_target_ip);
      IPAddress parseconfirm(gl_prefs.remote_target_ip);
      Serial.printf("Target IP: %s\r\n", parseconfirm.toString().c_str());
      save = 1;
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    /*
      Usage:
        fixedtarget enable
        fixedtarget disable
      */
    cmp = cmd_match((const char *)gl_console_cmd.buf,"fixedtarget ");
    if(cmp > 0)
    {
      match = 1;
      const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
      int argcmp = cmd_match( arg, "enable");
      if(argcmp > 0)
      {
        gl_prefs.en_fixed_target = 1;
        IPAddress parseconfirm(gl_prefs.remote_target_ip);
        Serial.printf("Enabling Fixed Target: %s\r\n", parseconfirm.toString().c_str());
      }
      argcmp = cmd_match( arg, "disable");
      if(argcmp > 0)
      {
        gl_prefs.en_fixed_target = 0;
        Serial.printf("Disabling Fixed Target\r\n");
      }
      save = 1;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////
    cmp = cmd_match((const char *)gl_console_cmd.buf,"setpwd ");
    if(cmp > 0)
    {
      match = 1;
      const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
      /*Set the password*/
      for(int i = 0; i < WIFI_MAX_PWD_LEN; i++)
      {
        gl_prefs.password[i] = '\0';
      }
      for(int i = 0; arg[i] != '\0'; i++)
      {
        if(arg[i] != '\r' && arg[i] != '\n')
        {
          gl_prefs.password[i] = arg[i];
        }
      }
      Serial.printf("Changing pwd to: %s\r\n",gl_prefs.password);
      save = 1;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////
    cmp = cmd_match((const char *)gl_console_cmd.buf,"setport ");
    if(cmp > 0)
    {
      match = 1;
      const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
      char * tmp;
      int port = strtol(arg, &tmp, 10);
      Serial.printf("Changing port to: %d\r\n",port);
      /*Set the port*/
      gl_prefs.port = port;
      save = 1;
    }
  
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    cmp = cmd_match((const char *)gl_console_cmd.buf,"setTXoff ");
    if(cmp > 0)
    {
      match = 1;
      const char * arg = (const char *)(&gl_console_cmd.buf[cmp]);
      char * tmp;
      int offset = strtol(arg, &tmp, 10);
      Serial.printf("Setting port offset to: %d\r\n",offset);
      gl_prefs.reply_offset = offset;
      save = 1;
    }	  

    //////////////////////////////////////////////////////////////////////////////////////////////////////
    cmp = cmd_match((const char *)gl_console_cmd.buf,"readcred");
    if(cmp > 0)
    {
      match = 1;
      Serial.printf("SSID: \'");
      for(int i = 0; gl_prefs.ssid[i] != 0; i++)
      {
        char c = gl_prefs.ssid[i];
        if(c >= 0x1f && c <= 0x7E)
        {
          Serial.printf("%c",c);
        }
        else
        {
          Serial.printf("%0.2X",c);
        }
      }
      Serial.printf("\'\r\n");

      Serial.printf("Password: \'");
      for(int i = 0; gl_prefs.password[i] != 0; i++)
      {
        char c = gl_prefs.password[i];
        if(c >= 0x1f && c <= 0x7E)
        {
          Serial.printf("%c",c);
        }
        else
        {
          Serial.printf("%0.2X",c);
        }
      }
      Serial.printf("\'\r\n");
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////
    cmp = cmd_match((const char *)gl_console_cmd.buf,"reconnect\r");
    if(cmp > 0)
    {
      match = 1;
      Serial.printf("restarting wifi connection...\r\n");
      /*Try to connect using modified ssid and password. for convenience, as a restart will fulfil the same functionality*/
      WiFi.disconnect();
      WiFi.begin((const char *)gl_prefs.ssid,(const char *)gl_prefs.password);
      udp.begin(server_address, gl_prefs.port);
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////
    cmp = cmd_match((const char *)gl_console_cmd.buf,"restart\r");
    if(cmp > 0)
    {
      Serial.printf("restarting chip...\r\n");
      ESP.restart();
    }




    /********************************Parsing over, cleanup*************************************/
    if(match == 0)
    {
      Serial.printf("Failed to parse: %s\r\n", gl_console_cmd.buf);
    }
    if(save != 0)
    {
      int nb = preferences.putBytes("settings", &gl_prefs, sizeof(nvs_settings_t));
      Serial.printf("Saved %d bytes\r\n", nb);
    }

    for(int i = 0; i < BUFFER_SIZE; i++)
    {
      gl_console_cmd.buf[i] = 0; 
    }
    gl_console_cmd.parsed = 1;
  }


  if(WiFi.status() != WL_CONNECTED)
  {
    blink_per = PERIOD_DISCONNECTED;
  }
  else
  {
    blink_per = PERIOD_CONNECTED;
  }


  if(ts - led_ts > blink_per)
  {
    led_ts = ts;
    digitalWrite(LED_PIN, led_state);
    led_state = ~led_state & 1;
    if(WiFi.status() != WL_CONNECTED)
    {
      //WiFi.reconnect();
      WiFi.disconnect();
      WiFi.begin((const char *)gl_prefs.ssid,(const char *)gl_prefs.password);
      udp.begin(server_address, gl_prefs.port);

    }

  }
}
