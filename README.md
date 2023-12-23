# IV18 WiFI Nixie Clock

![Alt text](https://github.com/rust84/iv18-clock/blob/main/images/iv18_clock.jpg "iv18 clock")

Make:

https://www.thingiverse.com/make:970845

### Installing the Arduino IDE

Download the latest Arduino IDE [here](https://www.arduino.cc/en/software)

### Setting up your project

Your project folder should look like this:

```sh
📁 AG_ESP8266_wemos_IV18_Clock
├── 📃 AG_ESP8266_wemos_IV18_Clock.ino
└── 📃 TimeZone.ino
```

### Setting up esp8266 boards

Follow the instructions here to add esp8266 boards using the Boards Manager: https://arduino-esp8266.readthedocs.io/en/3.1.2/installing.html#boards-manager

Select your board from the dropdown in the top left corner of the IDE. 

In my case I am using a Lolin D1 Mini so I selected "Lolin(WEMOS) D1 R2 & Mini".

Note that some clone boards use cheaper components and may not supply sufficient current (from memory the Lolin ones are rated for 500ma). YMMV.

I purchased mine from the [AliExpress Store](https://www.aliexpress.com/item/32529101036.html?pdp_npi=4%40dis%21GBP%21%EF%BF%A13.09%21%EF%BF%A13.09%21%21%213.80%213.80%21%402101fff317033385687152793da7ec%2159008795982%21sh01%21UK%213954109371%21&spm=a2g0o.store_pc_home.productList_2006296079365.pic_0)

### Installing Libraries

Go to Tools > Manage Libraries... and add install

- Time https://playground.arduino.cc/Code/Time/
- WifiManager https://github.com/tzapu/WiFiManager
- Arduinojson https://arduinojson.org/?utm_source=meta&utm_medium=library.properties (select 5.13.5 otherwise you will get a deprecation warning)

### Daylight savings

Edit the following lines to adjust to your local time zone and daylight savings.

```
//US Eastern Time Zone (New York, Detroit)
//TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240};    //Daylight time = UTC - 4 hours
//TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};     //Standard time = UTC - 5 hours

//Central European Time Zone (Paris)
//TimeChangeRule myDST = {"CEST", Last, Sun, Mar, 2, 120};    //Daylight summer time = UTC + 2 hours
//TimeChangeRule mySTD = {"CET", Last, Sun, Oct, 2, 60};     //Standard time = UTC + 1 hours

TimeChangeRule myDST = {"UTC", Last, Sun, Mar, 28, 60};    //Daylight summer time = UTC + 1 hours
TimeChangeRule mySTD = {"UTC", Last, Sun, Oct, 31, 0};     //Standard time = UTC + 0 hours
Timezone myTZ(myDST, mySTD);
TimeChangeRule *tcr;        //pointer to the time change rule, use to get TZ abbrev
```

### Verify

Click the ✅ in the top left of the IDE to check it compiles and check the log output for any errors.

### Flash

Depending on which board you received you may have a CP210x or CH341 based USB controller.

If you have trouble getting the board recognised then install the latest drivers.

- [CP210x controllers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- [CH341 controllers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)

Select the COM port your board is connected to from the dropdown.

Click flash and 🙏

### Credits and thanks

All credits go to [aeropic](https://www.thingiverse.com/aeropic/designs) for his original design and code which you can find on [Thingiverse](https://www.thingiverse.com/thing:3417955).

Thank you!