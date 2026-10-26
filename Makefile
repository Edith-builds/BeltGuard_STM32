CC ?= cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror -pedantic
.PHONY: test clean
test: tests/test_core tests/test_rfid
	./tests/test_core
	./tests/test_rfid
	python3 -m unittest discover -s tests -p 'test_*.py'
tests/test_core: Core/bg.c Core/bg.h tests/test_core.c
	$(CC) $(CFLAGS) -ICore Core/bg.c tests/test_core.c -lm -o $@
tests/test_rfid: Drivers/mfrc522.c Drivers/mfrc522.h tests/test_rfid.c
	$(CC) $(CFLAGS) -IDrivers Drivers/mfrc522.c tests/test_rfid.c -o $@
clean:
	rm -f tests/test_core tests/test_rfid
