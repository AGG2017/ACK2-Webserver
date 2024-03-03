#!/bin/bash
cd src || exit 1
make
if [ $? -eq 0 ]; then
	../arm-linux-musleabi-cross/arm-linux-musleabi/bin/strip webfsd
	if [ ! -d "../webserver/opt/bin" ]; then
		mkdir ../webserver/opt/bin
	fi
	cp -f webfsd ../webserver/opt/bin/webfsd
	cd ../webserver
	rm -f webserver.zip
	zip -r webserver.zip etc opt
	echo "SUCCESS! The package is ready in: webserver/webserver.zip"
	cd ..
	exit 0
fi
cd ..
echo "ERRORS FOUND!"
exit 1
