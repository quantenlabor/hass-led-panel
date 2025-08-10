# hass-led-panel
It's a panel that you can hang on your wall. It has a couple of LEDs that show if something is working or not. Driven by an Arduino sketch on an ESP8266 MCU and WS8211 addressable LEDs.

# HomeAssistant LED Panel

## What is this all about?

It's a panel that you can hand on your wall, like a picture. It has a couple of LEDs that show if something is working or not.

"Working" is defined by

- WiFi works (the ESP can connect to your home WiFi)
- a given system can be pinged successfully
- can connect to a given HTTP or HTTPS URL
- can connect to a given TCP host/port pair
- can send a packet to a given UDP host/port pair
- a Home Assistant entity reports a certain state

If the "something" is working, a corresponding LED lights up green, otherwise red. Simple, eh?

## History

To help phone support for (elderly) relatives with Internet trouble, I needed a solution that narrows down the problem space quickly. Asking "which lights are red?" is way easier than asking "can you ping Google?" or "does the IMAP server respond?".

Later, I decided to add Home Assistant functionality for my own panel next to my entrance door: One LED shows any open windows, another shows (paper) mail. For this, I slightly deviate from the red/green logic by also allowing yellow.

## Hardware

### MCU

The core is an ESP8266 board. I use Wemos D1 Mini clones, they are cheap and powerful enough.

### LED strip

Any strip compatible with the Adafruit Neopixel library will do; WS2811 or 8212B chips work well.

### Power supply

Most old Micro-USB wall warts (leftovers from old mobile phones, for example) will work. The thing does not need much power; the LEDs would be way too bright at full power anyway, so a brightness setting of about 30 works best (range is 0 to 255) with a commensurate reduction in power requirements.

### Case

Do what you like, but an easy and professional-looking case is a picture frame.

IKEA has nice picture frames (like RÖDALM) where you can hide all the electronics inside. 

- File a slot for the Micro USB cable.

- Cut a sheet of cardboard to the size of the picture. 

- Punch one or two rows of holes aligned with the LEDs on your strip; for two rows, cut your strip at the marked position and solder 3 wires (+, GND, and Data) from one strip to the next.
- Connect the end of the strip to the MCU: GND to GND, + to +5V, and Data to D2.
- Print a legend (you can use the Word file in the repo for inspiration). Do not punch holes for the LED; regular printer paper works as a diffusor for a nice even light. Stick the printed paper to the cardboard. 
- Affix the LED strip(s) to the back of the cardboard, e.g. with duct tape. 
- Assemble the frame, using the matte provided by IKEA.

## Firmware

Get the Arduino LED.
Install these libraries, using the Library Manager:

- Adafruit Neopixel
- HTTP Client

In the sketch, adjust #define NUM_PIXELS 10 to the number of LEDs you decided on. One column of 5 LEDs works well with a 6x10" frame, as do two columns of 5 for a total of 10 LEDs. For my relatives, I keep it simple with 5 LEDs (WiFi, home router, Google aka "the Internet", the family email server, and their bank) while my own setup currently has 10.

Connect the D1 Mini to your computer via USB. Use an USB 2.0 port (if available; USB 3.0 ports sometimes cause trouble) and a good USB cable.
Compile and uplaod the sketch. It's completely self-contained and does not need and files uploaded to LittleFS.

## WiFi Settings

Upon first start, the ESP does not know your WiFi credentials, so it opens its own WiFi network. All the LEDs will light up yelllow to indicate this.

- Connect your computer or phone to the **Wifi_Statuslight** network.
- Open http://192.168.4.1 in your web browser.
- Enter your home WiFi credentials (SSID aka network name and password) and click submit.

## Configuration

The ESP will now restart. All the LEDs will light up blue one by one.

- Connect your computer to your home network again.
- Find the IP address of the LED panel:
  - Check your home router for the device named WiFi_Statuslight, or
  - Open the **Serial Monitor** in the Arduino IDE, it will tell you the IP address.
- Let's say the IP address is *192.168.178.54*.
- Open http://192.168.178.54 in a web browser. The config page appears. Make the necessary entries and click *Submit*.

There is one line per LED (the first LED is reserved for the WiFi status as none of the other tests makes sense when the MCU can't connect to the WiFi).

Each line is a "pseudo-URL"; the possible values are

| Type  | Syntax                            |                                | Example                      |
|-------|-----------------------------------|--------------------------------|------------------------------|
| ping  | ping://host                       | Pings the host                 | ping://8.8.8.8               |
| http  | http://host[:port]                | Connects to web server         | http://hass.example.org:8123 |
| https | https://host[:port]               | Connects to web server         | https://google.com           |
| tcp   | tcp://host:port                   | Connects to TCP port           | tcp://imaps.example.org:993  |
| udp   | udp://host:port                   | Sends UDP packet               | udp://ns.example.org:53      |
| hass  | hass://entity_id:red:yellow:green | Queries HASS for entity status | hass://entity_id             |
| null  | null://                           | Do nothing (LED stays black)   | null://reserved for foobar   |

- Below the URLs, there ar for additional, global variables:
  - Timeout: Timeout (in seconds) until a reques is considered failed; the LED turns orange. Not implemented for all URL types!
  - Brightness: The brightness of the LEDs. Adjsut to your LED strip and surroundings. The range is 1 to 255; 30 works well for my strips but yours may be different.
  - HASS URL: The URL of your Home Assistant server. It's only used if you use hass:// pseudo-URLs.
  - HASS LLAT: To authenticate, you need a long-lived authentication token from your Home Assistant server. It's only used if you use hass:// pseudo-URLs. I recommend creating a separate HASS user and setting it to read-only, as an intruder could get at this LRIT easily.

You can revisit the configuration page at any time and make changes as needed.

To be clear, Home Assistant (HASS) is not needed at all; my relatives don't have HASS and it works just as well.

## Contributing

Pull requests are welcome. For major changes, please open an issue first
to discuss what you would like to change.

Please make sure to update tests as appropriate.

## Roadmap

Some things I'm considering, although there is no time plan yet:

- a Test button next to each line of the config screen
- syntax check before saving configuration
- a single "Acknowledge" hardware button that sets a Home Assistant entity to "on". This would work nicely to acknowledge alarms or messages like "you have mail" by triggering some automation in Home Assistant.

## License

MIT license, see LICENSE.txt
