CC=../arm-linux-musleabi-cross/bin/arm-linux-musleabi-gcc
CFLAGS=-DMIMEFILE=\""/etc/mime.types"\" -DWEBFS_VERSION=\"1.22\" -D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64

ODIR=.

DEPS = httpd.h

OBJ = webfsd.o webcam.o mime.o ls.o request.o response.o config-parser.o

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

webfsd: $(OBJ)
	$(CC) -static -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o webfsd

# ../arm-linux-musleabi-cross/arm-linux-musleabi/bin/strip webfsd
