all: randbytes handshake sslnew newrawkey rsasign x509storeissuer providerdoall rwlocks pkeyread evp_fetch

clean:
	rm libperf.a *.o randbytes handshake sslnew newrawkey rsasign x509storeissuer providerdoall rwlocks pkeyread evp_fetch

#-Wl,-rpath,$(TARGET_OSSL_LIBRARY_PATH)

CPPFLAGS += -I$(TARGET_OSSL_INCLUDE_PATH) -I.
CFLAGS += -pthread
LDFLAGS += -L$(TARGET_OSSL_LIBRARY_PATH) -L.

libperf.a: perflib/*.c perflib/*.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c perflib/*.c
	ar rcs libperf.a *.o

evp_fetch:	evp_fetch.c libperf.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o evp_fetch evp_fetch.c -lperf -lcrypto

randbytes:	randbytes.c libperf.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o randbytes randbytes.c -lperf -lcrypto

handshake: handshake.c libperf.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o handshake handshake.c -lperf -lcrypto -lssl

sslnew: sslnew.c libperf.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o sslnew sslnew.c -lperf -lcrypto -lssl

newrawkey:	newrawkey.c libperf.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o newrawkey newrawkey.c -lperf -lcrypto

rsasign: rsasign.c libperf.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o rsasign rsasign.c -lperf -lcrypto

x509storeissuer: x509storeissuer.c libperf.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o x509storeissuer x509storeissuer.c -lperf -lcrypto

providerdoall:	providerdoall.c libperf.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o providerdoall providerdoall.c -lperf -lcrypto

rwlocks: rwlocks.c libperf.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o rwlocks rwlocks.c -lperf -lcrypto

regen_key_samples:
	./genkeys.sh > keys.h

pkeyread: pkeyread.c keys.h libperf.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o pkeyread pkeyread.c -lperf -lcrypto
