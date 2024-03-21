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

**arm-linux-musleabi-cross** - The installed MUSL cross tools folder. It is expected to be in the project root folder but can be in another place. Can be also another MUSL tool package according to your platform. The build script and the make file are hardcoded to use `arm-linux-musleabi-cross` in the project root folder, so in case you use another tool or another path, replace this folder name and tool path accordingly.

**install** - Contains information how to install the MUSL cross tool.

**src** - Contains the source code for the custom webserver.

**webserver** - Contains the web pages and other resources to be packed in the result archive file `webserver.zip`

### Additional Information

To include this custom webserver in the generated custom update you have to enable the option `webserver` with the parameter equal to the package name and the port you need, like `webserver="webfs-v5:8000"` (inside the configuration file `options.cfg`).

You can modify the existing or add more static html pages (with css and javascript only) inside the document root folder webfs. For backend support you have to do the processing inside the `request.c` source file. There is no `php` or other script engines available at the backend, so all needed server processing should be done as optimized `C` code.

The web pages document root comes from the update in the printer folder `/opt/webfs`. Then, at the first boot is transfered to the final destination `/mnt/UDISK/webfs` from where the webserver will use the static pages when they are requested. For the template pages that need some processing, they are read from `/opt/webfs` as templates, updated with information and saved in `/mnt/UDISK/webfs` as ready to use static web pages.

You can add more static pages if needed after you have updated the printer. The additional pages should be added in `/mnt/UDISK/webfs`. The template pages in `/opt/webfs` can be also modified after you have updated the printer, but you should not change the parameters in the templates because this may require also change in the executable file `/opt/bin/webfsd`.

The webcam web page requires a web camera connected to any USB slot with support for mpeg or raw YUYV video format of 640x480.

The web page for the Kobra Unleashed interface requires the Kobra Unleashed http server URL to be set in advance inside the file `webserver.json`. This usually can be done in the folder RESOURCES/KEYS (for the custom update project) where a template `webserver.json` already exists and will be included in the generated custom update.

How to build Kobra Unleashed MQTT server with web interface on a Raspberry Pi 4 or 5 can be found [here](https://github.com/AGG2017/kobra-unleashed).

Information how to create custom updates can be found in [this repository](https://github.com/ultimateshadsform/Anycubic-Kobra-2-Series-Tools)
