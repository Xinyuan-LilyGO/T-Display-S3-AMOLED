/*
 * @file      octagon_on_amoled.ino
 * @author    Rudi Ackermann (rudi.ackermann@hotmail.de)
 * @license   MIT
 * @copyright Copyright (c) 2023 Rudi Ackermann
 * @date      2023-08-17
 * @note      Sketch Adaptation for AMOLED
*/

// Include necessary libraries and fonts
#include "rm67162.h"
#include <TFT_eSPI.h>
#include "NotoSansMonoSCB20.h"
#include "Latin_Hiragana_24.h"

// Define constants
const float MY_PI = 3.1415926535897932384626433832795;

// Define colors
#define GRAY   0x2A0A
#define LINES  0x8C71

// Initialize display and sprites
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite bgr = TFT_eSprite(&tft);
TFT_eSprite sprite = TFT_eSprite(&tft);

void setup()
{
    /*
    * Compatible with touch version
    * Touch version, IO38 is the screen power enable
    * Non-touch version, IO38 is an onboard LED light
    * * */
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);

    // Initialize AMOLED LCD and set properties
    rm67162_init();
    lcd_setRotation(1);
    sprite.createSprite(536, 240);
    sprite.setTextDatum(MC_DATUM);  // Center the text
    sprite.setTextColor(TFT_RED);
    sprite.setSwapBytes(1);
    sprite.fillScreen(TFT_BLACK);
    sprite.loadFont(NotoSansMonoSCB20);

    // Draw the main graphic
    draw();
}

void loop()
{
    // Intentionally left empty
}

void draw()
{
    sprite.setTextColor(LINES, TFT_BLACK);
    sprite.loadFont(Latin_Hiragana_24);

    const int centerX = 170;
    const int centerY = 240 / 2;
    const float radius = 85.0;
    const float labelOffset = 20.0; // Distance outside the hexagon for labels

    int listX = 415;   // Start position of the list on x-axis
    int listY = 30;    // Initial y-coordinate for the list
    int listStep = 30; // Vertical step between list items

    // Draw surrounding circle and rectangle first (outside the loop)
    sprite.drawCircle(centerX, centerY, radius, TFT_RED);
    sprite.drawRect(0, 0, 536, 240, TFT_YELLOW);

    for (int i = 0; i < 6; i++) {
        int x1 = round(centerX + radius * cos(i * MY_PI / 3));
        int y1 = round(centerY + radius * sin(i * MY_PI / 3));
        int x2 = round(centerX + radius * cos((i + 1) * MY_PI / 3));
        int y2 = round(centerY + radius * sin((i + 1) * MY_PI / 3));

        // Draw hexagon and triangles
        sprite.drawLine(x1, y1, x2, y2, TFT_GREEN);
        sprite.drawLine(x1, y1, centerX, centerY, TFT_GREEN);
        sprite.drawLine(x2, y2, centerX, centerY, TFT_GREEN);

        // Draw points
        sprite.fillCircle(x1, y1, 3, TFT_YELLOW);
        sprite.fillCircle(centerX, centerY, 4, TFT_RED);

        // Label the vertices
        int labelX = round(centerX + (radius + labelOffset) * cos(i * MY_PI / 3));
        int labelY = round(centerY + (radius + labelOffset) * sin(i * MY_PI / 3));
        String label = String(i + 1);
        sprite.drawString(label, labelX, labelY, 2);

        // Display the coordinates as a list beside the graphics
        String x1_str = (x1 < 10) ? "00" + String(x1) : (x1 < 100) ? "0" + String(x1) : String(x1);
        String y1_str = (y1 < 10) ? "00" + String(y1) : (y1 < 100) ? "0" + String(y1) : String(y1);
        String coordLabel = label + ": (" + x1_str + ", " + y1_str + ")";
        sprite.drawString(coordLabel, listX, listY + i * listStep, 2);
    }

    sprite.setTextColor(TFT_RED, TFT_BLACK);
    // Label the center point
    int centerLabelX = centerX;     // This keeps the label vertically centered with the point
    int centerLabelY = centerY - 30; // Adjust by 30 points for better readability
    sprite.drawString("c", centerLabelX, centerLabelY, 2);
    // Add the center's coordinates to the list

    String centerX_str = (centerX < 10) ? "00" + String(centerX) : (centerX < 100) ? "0" + String(centerX) : String(centerX);
    String centerY_str = (centerY < 10) ? "00" + String(centerY) : (centerY < 100) ? "0" + String(centerY) : String(centerY);
    String centerCoordLabel = "c: (" + centerX_str + ", " + centerY_str + ")";
    sprite.drawString(centerCoordLabel, listX, listY + 6 * listStep, 2); // 6 is the number of hexagon vertices

    // Push the sprite to the display
    lcd_PushColors(0, 0, 536, 240, (uint16_t *)sprite.getPointer());
}
