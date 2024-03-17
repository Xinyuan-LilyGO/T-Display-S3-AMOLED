#include "OneButton.h" /* https://github.com/mathertel/OneButton.git */
#include "lvgl.h"      /* https://github.com/lvgl/lvgl.git */

#include <Arduino.h>
#include "rm67162.h"
#include "setup_img.h"
#include "WiFi.h"
#include "esp_sntp.h"
#include "time.h"
#include "factory_gui.h"
#include "zones.h"
#include "pins_config.h"
#include "cert.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#if ARDUINO_USB_CDC_ON_BOOT != 1
#warning "If you need to monitor printed data, be sure to set USB CDC On boot to ENABLE, otherwise you will not see any data in the serial monitor"
#endif

#ifndef BOARD_HAS_PSRAM
#error "Detected that PSRAM is not turned on. Please set PSRAM to OPI PSRAM in ArduinoIDE"
#endif

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf;

OneButton button1(PIN_BUTTON_1, true);
OneButton button2(PIN_BUTTON_2, true);

void led_task(void *param);
void wifi_test(void);
void timeavailable(struct timeval *t);
void printLocalTime();
void SmartConfig();
void setTimezone();

bool pressed;

void my_disp_flush(lv_disp_drv_t *disp,
                   const lv_area_t *area,
                   lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
    lv_disp_flush_ready(disp);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("T-DISPLAY-S3-AMOLED FACTORY TEST");
    pinMode(PIN_BAT_VOLT, ANALOG);

    rm67162_init(); // amoled lcd initialization
    lcd_setRotation(1);


    xTaskCreatePinnedToCore(led_task, "led_task", 1024, NULL, 1, NULL, 0);

    lv_init();
    buf = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE);
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_LCD_BUF_SIZE);

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    sntp_servermode_dhcp(1); // (optional)
    configTime(GMT_OFFSET_SEC, DAY_LIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);



    // test_screen(); // uncomment if you wish to test for bad pixels before factory demo.
    wifi_test();
    LV_DELAY(2000);
    setTimezone();
    printLocalTime();
    ui_begin();

    button1.attachClick(
    []() {
        uint64_t mask = 1 << PIN_BUTTON_1;
        lcd_sleep();
        esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ALL_LOW);
        esp_deep_sleep_start();
    });

    button2.attachClick([]() {
        ui_switch_page();
    });
}

void loop()
{
    lv_timer_handler();
    delay(2);
    button1.tick();
    button2.tick();
    static uint32_t last_tick;
    if (millis() - last_tick > 100) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            lv_msg_send(MSG_NEW_HOUR, &timeinfo.tm_hour);
            lv_msg_send(MSG_NEW_MIN, &timeinfo.tm_min);
        }
        uint32_t volt = (analogReadMilliVolts(PIN_BAT_VOLT) * 2);
        lv_msg_send(MSG_NEW_VOLT, &volt);

        last_tick = millis();
    }
}

//Test screen bad pixels
void test_screen(void)
{
    lv_obj_t * colors_obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(colors_obj,lv_pct(100),lv_pct(100));
    lv_obj_set_style_bg_color(colors_obj,lv_color_make(255,0,0),LV_PART_MAIN);
    lv_obj_center(colors_obj);

    button1.attachClick([]() {
        pressed = true;
    });

    uint8_t index = 0;
    lv_color_t test_color[] = {lv_color_make(0,255,0),lv_color_make(0,0,255),lv_color_make(255,255,255),lv_color_make(0,0,0)};
    while(1){
        if(pressed){
            pressed = false;
            lv_obj_set_style_bg_color(colors_obj,test_color[index],LV_PART_MAIN);
            index++;
            if(index > sizeof(test_color)/sizeof(test_color[0])){
                lv_obj_del(colors_obj);
                break;
            }
        }
        lv_timer_handler();
        button1.tick();
    }
    //test color end
}

void led_task(void *param)
{
    pinMode(PIN_LED, OUTPUT);
    while (1) {
        digitalWrite(PIN_LED, 1);
        delay(20);

        digitalWrite(PIN_LED, 0);
        delay(980);
    }
}

void wifi_test(void)
{
    String text;

    lv_obj_t *log_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_bg_color(log_label, lv_color_black(), 0);
    lv_obj_align(log_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_width(log_label, LV_PCT(100));
    lv_label_set_long_mode(log_label, LV_LABEL_LONG_SCROLL);
    lv_label_set_recolor(log_label, true);

    lv_label_set_text(log_label, "Scan WiFi");
    LV_DELAY(1);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0) {
        text = "no networks found";
    } else {
        text = n;
        text += " networks found\n";
        for (int i = 0; i < n; ++i) {
            text += (i + 1);
            text += ": ";
            text += WiFi.SSID(i);
            text += " (";
            text += WiFi.RSSI(i);
            text += ")";
            text += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " \n" : "*\n";
            delay(10);
        }
    }
    lv_label_set_text(log_label, text.c_str());
    Serial.println(text);
    LV_DELAY(2000);
    text = "Connecting to ";
    Serial.print("Connecting to ");
    text += WIFI_SSID;
    text += "\n";
    Serial.print(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t last_tick = millis();
    uint32_t i = 0;
    bool is_smartconfig_connect = false;
    lv_label_set_long_mode(log_label, LV_LABEL_LONG_WRAP);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        text += ".";
        lv_label_set_text(log_label, text.c_str());
        LV_DELAY(100);
        if (millis() - last_tick > WIFI_CONNECT_WAIT_MAX) {
            /* Automatically start smartconfig when connection times out */
            text += "\nConnection timed out, start smartconfig";
            lv_label_set_text(log_label, text.c_str());
            LV_DELAY(100);
            is_smartconfig_connect = true;
            WiFi.mode(WIFI_AP_STA);
            Serial.println("\r\n wait for smartconfig....");
            text += "\r\n wait for smartconfig....";
            text += "\nPlease use #ff0000 EspTouch# Apps to connect to the "
                    "distribution network";
            lv_label_set_text(log_label, text.c_str());
            WiFi.beginSmartConfig();
            while (1) {
                LV_DELAY(100);
                if (WiFi.smartConfigDone()) {
                    Serial.println("\r\nSmartConfig Success\r\n");
                    Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
                    Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
                    text += "\nSmartConfig Success";
                    text += "\nSSID:";
                    text += WiFi.SSID().c_str();
                    text += "\nPSW:";
                    text += WiFi.psk().c_str();
                    lv_label_set_text(log_label, text.c_str());
                    LV_DELAY(1000);
                    last_tick = millis();
                    break;
                }
            }
        }
    }
    if (!is_smartconfig_connect) {
        text += "\nCONNECTED \nTakes ";
        Serial.print("\n CONNECTED \nTakes ");
        text += millis() - last_tick;
        Serial.print(millis() - last_tick);
        text += " ms\n";
        Serial.println(" millseconds");
        lv_label_set_text(log_label, text.c_str());
    }
}

void printLocalTime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("No time available (yet)");
        return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

// Callback function (get's called when time adjusts via NTP)
void timeavailable(struct timeval *t)
{
    Serial.println("Got time adjustment from NTP!");
    printLocalTime();
    WiFi.disconnect();
}

void setTimezone()
{
    Serial.println ("Setting timezone");
#ifdef CUSTOM_TIMEZONE
    String timezone = CUSTOM_TIMEZONE;
    Serial.println("Custom timezone is set.  Getting timezone info for: "+ timezone);
#else
    String tz_api = GET_TIMEZONE_API;
    Serial.println("No custom timezone is set, connecting to " + tz_api);
    
    WiFiClientSecure *client = new WiFiClientSecure;
    String timezone;
    if (client) {
        client->setCACert(rootCA_ISRG_ROOT_X2);
        HTTPClient https;
        if (https.begin(*client, GET_TIMEZONE_API)) {
            int httpCode = https.GET();
            if (httpCode > 0) {
                // HTTP header has been send and Server response header has been handled
                Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

                // file found at server
                if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                    String payload = https.getString();
                    Serial.println(payload);
                    timezone = payload;
                }
            } else {
                Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
                String timezone = "None";
                Serial.println("Unable to get timezone from API, setting timezone to: " + timezone);
            }
            try {
                  if(https.connected()){
                    https.end();
                  } else {
                    Serial.println("Error https connection dropped");
                    throw 400;
                  }
            }
            catch (...){
              Serial.println("Handled exception");
            }

        }
        delete client;
    }
    
#endif
    for (uint32_t i = 0; i < sizeof(zones); i++) {
            if (timezone == "None") {
                timezone = "Australia/Sydney";
                timezone = "AEST-10AEDT,M10.1.0,M4.1.0/3";
                break;
            }
            if (timezone == zones[i].name) {
                timezone = zones[i].zones;
                break;
            }
        }
    Serial.println("timezone : " + timezone);
    setenv("TZ", timezone.c_str(), 1); // set time zone
    tzset();
}