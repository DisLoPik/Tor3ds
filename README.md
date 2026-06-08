# Tor3DS Interactive Bridge 🧅

Demo: https://youtu.be/ei6YDzFaDiQ

A thin-client Tor browser architecture for the Nintendo 3DS. 

Because the Nintendo 3DS lacks the CPU power and RAM to natively handle modern HTML5/CSS3 rendering and Tor's heavy cryptographic routing, this project bypasses those hardware limitations entirely. 

It splits the workload: a Python server runs in the background on your PC to handle the Tor network and render pages using a headless browser. It then beams highly compressed graphical snapshots and invisible link-mapping coordinates directly to the 3DS over your local network. The result is modern web rendering and Tor anonymity running at 60FPS on handheld hardware.

## 🏗️ Architecture

* **The Heavy Lifter (PC Server):** A Python Flask application that connects to the Tor network via a SOCKS5 proxy. It spawns an invisible Chromium window matching the 3DS's exact screen dimensions, takes a compressed JPEG snapshot, maps all clickable `<a href>` screen coordinates, and sends the payload to the console.
* **The Dumb Terminal (3DS Client):** A lightweight C application built with `devkitPro` and `citro2d`. It fetches the payload, decodes the JPEG using `stb_image`, Morton-swizzles the pixels for the 3DS GPU, and overlays an invisible touch grid to make the static image fully interactive.

---

## 💻 PC Server Setup (Backend)

The server must be running on a PC connected to the same local Wi-Fi network as your 3DS.

### Prerequisites
* Python 3.8+
* [Tor Browser](https://www.torproject.org/download/) (or the standalone Tor Expert Bundle)

### Installation
1. Clone this repository to your PC.
2. Navigate to the `Pc Side` directory.
3. Install the required Python libraries:
   ```
   pip install -r requirements.txt
   playwright install
   ```

### Running the Server
1. Open the Tor Browser and leave it running in the background (this opens the required SOCKS5 proxy on port 9150).
2. Run the bridge server:
```
python server.py
```

### 🎮 3DS Client Setup (Frontend)
If you simply want to use the app, download the latest Tor3DS_vX.zip from the Releases tab and skip to step 3.
If you want to compile it from source, follow the instructions below:

Compiling from Source
1. Install devkitPro with the 3DS development environment (devkitARM, libctru, citro2d, citro3d).
2. Open a terminal in the root directory of this repository and build the .3dsx file: 
```
make
```
### Installation to SD Card
3. Create a folder named Tor3DS inside the 3ds folder on your console's SD card (sdmc:/3ds/Tor3DS/).
4. Copy 3DS_side.3dsx and 3DS_side.smdh into that folder.
5. Create a new text file in that exact same folder named config.txt.
6. Open config.txt and type the Target IP of your PC server (e.g., 192.168.x.x). Save the file.
7. Launch the app via the Homebrew Launcher!

### 🕹️ Controls
Touchscreen (Address Bar): Tap the white bar at the top of the bottom screen to open the software keyboard and type a URL (e.g., duckduckgo.com).
Touchscreen (Content): Tap any link on the webpage to navigate to it.
D-Pad Up / Down: Scroll the webpage vertically.
Start: Exit the application.


## License
This project uses a dual-license architecture:
* **The 3DS Client** (`source/` directory) is licensed under the **GPLv3**.
* **The PC Bridge Server** (`Pc Side/` directory) is licensed under the **AGPLv3** to ensure that any network-facing modifications to the backend remain open-source. 
See the respective `LICENSE` files in each directory for full details.
