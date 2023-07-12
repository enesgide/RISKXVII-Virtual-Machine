TARGET = vm_riskxvii

CC = gcc

CFLAGS     = -c -Wall -Wvla -Werror -Os -std=c11 -flto
LDFLAGS    = -lm -Wl,--gc-sections -s
SRC        = vm_riskxvii.c
OBJ        = $(SRC:.c=.o)

all:$(TARGET)

$(TARGET):$(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

.SUFFIXES: .c .o

.c.o:
	 $(CC) $(CFLAGS) $<

run:
	./$(TARGET)

test:
	echo what are we testing?!
	
tests:
	@echo Building tests...
	@for test_file in test_cases/*.mi ; do \
		echo "   "File: $$test_file ; \
	done

run_tests: $(TARGET)
	@echo Running tests...
	@for test_file in test_cases/*.mi ; do \
		echo "\n"File: $$test_file ; \
		in_file=$$(basename $$test_file .mi).in ; \
		out_file=$$(basename $$test_file .mi).out ; \
		./$(TARGET) $$test_file < test_cases/$$in_file | diff - test_cases/$$out_file || true ; \
	done

clean:
	rm -f *.o *.obj $(TARGET)