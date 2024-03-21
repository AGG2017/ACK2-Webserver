## Anycubic Kobra 2 Custom Webserver

This repository contains the source code and the tools needed to build a custom webserver package for Anycubic Kobra 2 Series 3D printers.

The result file `webserver/webserver.zip` can be used as a package for the option `webserver` which is a part of the options available for the `Custom Updates` project for these AC K2 printers. For more information fow to use this package to create a custom update please refer to the information provided in [this repository](https://github.com/ultimateshadsform/Anycubic-Kobra-2-Series-Tools).

This version of the webserver was done based on the great work from these two projects:

[WEBFS](https://linux.bytesex.org/misc/webfs.html) - well optimized web server for static web pages and files.

[MUSL](https://musl.libc.org) - well optimized static library that eliminate the need of using shared libraries (with all other functions in them not used by the caller).

### Installation

Documentation can be found in the `install` folder.

### Build

After installing the tools, building the package can be done by executing the script `build.sh` from the project root folder.

### Folder Information inside the root of the project

**arm-linux-musleabi-cross** - The installed MUSL cross tools folder. It is expected to be in the project root folder but can be in another place. Can be also another MUSL tool package according to your platform. The biuld script and the make file are hardcoded to use `arm-linux-musleabi-cross` in the project root folder, so in case you use another tool or another path, replace this folder name and tool path accordingly.

**install** - Contains information how to install the MUSL cross tool.

**src** - Contains the source code for the custom webserver.

**webserver** - Contains the web pages and other resources to be packed in the result archive file `webserver.zip`

### Additional Information

The webcam web page requires a web camera connected to any USB slot with support for mpeg or raw YUYV video format of 640x480.

The web page for the Kobra Unleashed interface requires the Kobra Unleashed http server URL to be set in advance inside the file webserver.json

How to build Kobra Unleashed MQTT server with web interface on a Raspberry Pi 4 or 5 can be found [here](https://github.com/AGG2017/kobra-unleashed).

Information how to create custom updates can be found in [this repository](https://github.com/ultimateshadsform/Anycubic-Kobra-2-Series-Tools)
