/*
* Capture the temp using a DHT11, (D4) then display it on the 128x64 display (I2C D1/D2)
* Also respond to client requests with a Web Server response
* Paul & Ben Adamson 2017
*/

#include <Adafruit_BMP085_U.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <DHT_U.h>

const char* DEVICE_NAME = "  -- Mini Sensor --";

// sensor details
const int DHT_PIN = D4;
const int DHTTYPE = DHT11;
DHT_Unified dht(DHT_PIN, DHTTYPE);
Adafruit_BMP085_Unified bmp180;
boolean dhtPresent = false;
boolean bmp180Present = false;

//display details
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_RESET);  //-1 indicates reset pin not used
#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif
unsigned long nextRefreshTime = 0;

// WiFi Details
const char* ssid = "<network name>";
const char* password = "<network password>";
const int HTTP_PORT = 80;
const int HTTP_EXT_PORT = 58000; // port forwarded at the firewall to the wifi server local ip

const int WIFI_WAIT = 10; // 2*tenths of seconds to wait for WiFi and show splashscreen
const int WIFI_STATUS_TIME = 500; // ms to show the wifi status page on startup
const int SENSOR_DETAILS_TIME = 500; // ms to show sensor details on startup (twice)
const int PAGE_REFRESH_SEC = 10; // secs between auto refresh of web page
const int TEMP_READING_DELAY = 1000; // ms to wait between readings
const int GET_IP_TIMEOUT = 5000; // ms to wait for our external ip address from server

IPAddress ip(192, 168, 1, 178);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
// Initialize the WiFi server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
WiFiServer server(HTTP_PORT);

int temp;
int humid;
int pressure;
boolean tempPresent;
boolean humidPresent;
boolean pressurePresent;

String extIPAddr;

void setup()
{
	// initialise the temp sensor
	dht.begin();
	{
		dhtPresent = true;
	}
	// initialise the pressure sensor
	if (bmp180.begin()) {
		bmp180Present = true;
	}

	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)
	display.cp437(true); // correct the extended character set.

	display.display();  // show the [custom] bitmap in the buffer
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0, 57);

	// connecting to a WiFi network
	WiFi.config(ip, gateway, subnet); // static IP
	WiFi.begin(ssid, password);
	unsigned long loopCount = millis() + WIFI_WAIT;
	while ((WiFi.status() != WL_CONNECTED) && (millis() < loopCount)) {
		display.print(".");
		display.display();
		delay(200);
	}

	// start the Ethernet connection and the server:
	server.begin();
	printWiFiStatus();
	delay(WIFI_STATUS_TIME); // showing the wifi status

	printSensorDetails();
	delay(SENSOR_DETAILS_TIME);

	// try to connect to http://api.ipify.org to get our external IP address (beyond the router)
	WiFiClient ipClient;
	if (ipClient.connect("api.ipify.org", 80)) {
		ipClient.println("GET / HTTP/1.0");
		ipClient.println("Host: api.ipify.org");
		ipClient.println();
		unsigned long timeout = millis() + GET_IP_TIMEOUT;
		while (ipClient.available() == 0) {
			if (millis() > timeout) {
				// timeout!
				ipClient.stop();
			}
		}
		if (millis() < timeout) {
			// Read all the lines of the reply from server
			while (ipClient.available()) {
				extIPAddr = ipClient.readStringUntil('\r');
			}
		}
	}
}

void loop()
{

	if (millis() >= nextRefreshTime) {
		nextRefreshTime += TEMP_READING_DELAY; // wait a second before redrawing or regetting temps
		drawTitleBlock(true, true);

		// display the data
		display.setCursor(0, 19);
		if (tempPresent = getTemp(temp)) {
			display.print("  Temp.:    ");
			display.print(temp);
			display.print(char(248));
			display.println("C");
		}
		else {
			display.println("  Temp.:    N/C");
		}
		if (humidPresent = getHumidity(humid)) {
			display.print("  Humidity: ");
			display.print(humid);
			display.println("%");
		}
		else {
			display.println("  Humidity: N/C");
		}
		if (pressurePresent = getPressure(pressure)) {
			display.print("  Pressure: ");
			display.print(pressure);
			display.println("hPa");
		}
		else {
			display.println("  Pressure: N/C");
		}

		display.setCursor(0, 56);
		display.print("IP:");
		if (WiFi.status() == WL_CONNECTED) {
			// print your WiFi shield's IP address:
			IPAddress ip = WiFi.localIP();
			display.print(ip);
			display.print(" ");
			long rssi = WiFi.RSSI();
			display.println(rssi);
		}
		else {
			display.print(" N/C ");
			long rssi = WiFi.RSSI();
			display.print(rssi);
			display.println("dBm");
		}
		display.display();
	} // draw and get time 

	  // handle any client requests
	  // Check if a client has connected
	WiFiClient client = server.available();
	if (client) {
		// Wait until the client sends some data
		while (!client.available()) {
			delay(1);
		}

		// Read the first line of the request
		String req = client.readStringUntil('\r');
		//Serial.println(req);
		client.flush();

		// send a standard http response header
		client.println("HTTP/1.1 200 OK");
		client.println("Content-Type: text/html");
		client.println("Connection: close");  // the connection will be closed after completion of the response
		client.print("Refresh: ");
		client.println(PAGE_REFRESH_SEC);  // refresh the page automatically every n sec
		client.println();
		client.println("<!DOCTYPE HTML>");
		client.println("<html>");
		client.print("<title>");
		client.print(DEVICE_NAME);
		client.println("</title>");
		client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
		client.println("<link rel=\"stylesheet\" href=\"http://www.w3schools.com/lib/w3.css\">");
		client.println("<body>");

		// header
		client.println("<header class=\"w3-container w3-teal w3-center\">");
		client.println("<h2>The <u>");
		client.println(DEVICE_NAME);
		client.println("</u> Web Server</h2>");
		client.println("</header>");

		// output the temperature
		client.println("<div class=\"w3-container w3-center\">");
		client.println("<p/>");
		client.println("<table class=\"w3-table-all w3-card-4\">");
		client.println("<thead><tr class=\"w3-green\"><th><div class=\"w3-center\">Sensor</div></th><th>Reading</th></tr></thead>");
		if (tempPresent) {
			client.print("<tr><td><div class=\"w3-center\">Temperature</div></td><td>");
			client.print(temp);
			client.println("&#176;C</td></tr>");
		}
		if (humidPresent) {
			client.print("<tr><td><div class=\"w3-center\">Humidity</div></td><td>");
			client.print(humid);
			client.println("%</td></tr>");
		}
		if (pressurePresent) {
			client.print("<tr><td><div class=\"w3-center\">Pressure</div></td><td>");
			client.print(pressure);
			client.println("hPa</td></tr>");
		}
		client.println("</table>");
		client.println("<p/>");
		client.println("</div>");

		// footer
		client.println("<footer class=\"w3-container w3-teal w3-center\">");
		client.println("<p>Brought to you by Arduino, W3, and coded by Ben and Paul Adamson<br/>");
		long rssi = WiFi.RSSI();
		client.print("Web Server WiFi Signal (RSSI): ");
		client.print(rssi);
		client.println("dBm or ");
		client.print(map(constrain(rssi, -100, -50), -100, -50, 0, 100));
		client.println("%<br/>");
		client.print("Web Server Internal IP: ");
		client.println(WiFi.localIP());
		client.print("  Port: ");
		client.print(HTTP_PORT);
		client.println("<br/> ");
		client.print("Web Server External IP:");
		client.println(extIPAddr);
		client.print("  Port: ");
		client.print(HTTP_EXT_PORT);
		client.println("<br/> ");
		client.println("</p></footer>");

		client.println("</body>");
		client.println("</html>");
	}
}

// get the temperature in degrees celcius
// prefer the bmp180 sensor if it is available
boolean getTemp(int& temp) {
	// note float for the sensor, but the function returns an int
	sensors_event_t event;
	if (bmp180Present) {
		float t;
		bmp180.getTemperature(&t);
		temp = t;
		return true;
	}
	else if (dhtPresent) {
		dht.temperature().getEvent(&event);
		if (isnan(event.temperature)) {
			return false;
		}
		temp = event.temperature;
		return true;
	}
	return false;
}

// get the Humidity
boolean getHumidity(int& humidity) {
	sensors_event_t event;
	if (dhtPresent) {
		dht.humidity().getEvent(&event);
		if (isnan(event.relative_humidity)) {
			return false;
		}
		humidity = event.relative_humidity;
		return true;
	}
	return false;
}

// get the Pressure
boolean getPressure(int& pressure) {
	// note float for the sensor, but the function returns an int
	sensors_event_t event;
	if (bmp180Present) {
		bmp180.getEvent(&event);
		if (isnan(event.pressure)) {
			return false;
		}
		pressure = event.pressure;
		return true;
	}
	return false;
}

void printWiFiStatus() {
	// Draw title block
	drawTitleBlock(false, false);
	display.println("--WiFi Status:--");
	// print the SSID of the network you're attached to:
	display.print("SSID: ");
	display.println(WiFi.SSID());
	// print your WiFi shield's IP address:
	IPAddress ip = WiFi.localIP();
	display.print("IP: ");
	display.println(ip);
	// print the received signal strength:
	long rssi = WiFi.RSSI();
	display.print("Sig. (RSSI): ");
	display.print(rssi);
	display.println(" dBm");
	// print the status
	display.print("Status: ");
	display.println(WiFi.status());

	display.display();
}

void printSensorDetails() {
	sensor_t sensor;

	if (bmp180Present) {
		bmp180.getSensor(&sensor);
		drawTitleBlock(false, false);
		display.print("Sensor:"); display.println(sensor.name);
		display.print("Driver Ver:"); display.println(sensor.version);
		display.print("Unique ID:"); display.println(sensor.sensor_id);
		display.print("Min/Max:"); display.print(sensor.min_value);	display.print("/"); display.print(sensor.max_value); display.println(" hPa");
		display.print("Resolution:"); display.print(sensor.resolution); display.println(" hPa");
	}
	else {
		display.println("BMP180 Not Connected");
	}
	display.display();

	delay(SENSOR_DETAILS_TIME);

	if (dhtPresent) {
		dht.temperature().getSensor(&sensor);
		drawTitleBlock(false, false);
		display.print("Sensor:"); display.println(sensor.name);
		display.print("Driver Ver:"); display.println(sensor.version);
		display.print("Unique ID:"); display.println(sensor.sensor_id);
		display.print("Min/Max:"); display.print(sensor.min_value); display.print("/"); display.print(sensor.max_value); display.print(char(248)); display.println("C");
		display.print("Resolution:"); display.print(sensor.resolution); display.print(char(248)); display.println("C");
		display.display();

		delay(1000);

		dht.humidity().getSensor(&sensor);
		drawTitleBlock(false, false);
		display.print("Sensor:"); display.println(sensor.name);
		display.print("Driver Ver:"); display.println(sensor.version);
		display.print("Unique ID:"); display.println(sensor.sensor_id);
		display.print("Min/Max:"); display.print(sensor.min_value);	display.print("/"); display.print(sensor.max_value); display.println("%");
		display.print("Resolution:"); display.print(sensor.resolution); display.println("%");
	}
	else {
		display.println("DHT Not Connected");
	}
	display.display();

}

void drawTitleBlock(boolean drawInnerBorder, boolean drawStatusLine) {
	// remove everything
	display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0, 0);

	// Draw title block
	display.println(DEVICE_NAME);
	display.println();
	display.drawFastHLine(0, 9, 128, WHITE);

	if (drawInnerBorder) {
		display.drawRoundRect(2, 12, 124, 39, 5, WHITE);
	}
	else {
		display.drawFastHLine(0, 11, 128, WHITE);  // draw a 2nd underline instead
	}

	// Draw Status Line
	if (drawStatusLine) {
		display.drawFastHLine(0, 53, 128, WHITE);
	}
}
