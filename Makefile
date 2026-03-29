.PHONY: build

CC=gcc
APP=basis

PLATFORM:=$(shell uname -s)
CPU:=$(shell uname -m)

C_ERRS += -Wall -Wextra -Wpedantic \
		-Wformat=2 -Wno-unused-parameter -Wshadow \
		-Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
		-Wredundant-decls -Wnested-externs -Wmissing-include-dirs \
		-Wno-unused
STD:=c11

hash = $(shell git log --pretty=format:'%h' -n 1)

help:
	@echo "make clean"
	@echo "make fetch"
	@echo "make build"
	@echo "make test"
	@echo "make release_cli"
	@echo "make run"

clean:
	rm -rf build

fetch:
	curl https://raw.githubusercontent.com/robrohan/r2/refs/heads/main/r2_maths.h > ./vendor/r2_maths.h
	curl https://raw.githubusercontent.com/robrohan/r2/refs/heads/main/r2_strings.h > ./vendor/r2_strings.h
	curl https://raw.githubusercontent.com/robrohan/r2/refs/heads/main/r2_termui.h > ./vendor/r2_termui.h
	curl https://raw.githubusercontent.com/robrohan/r2/refs/heads/main/r2_unit.h > ./vendor/r2_unit.h

run:
	 ./build/$(PLATFORM)/$(CPU)/$(APP).debug \
		/tenlib/common.lisp \
		/tenlib/math.lisp

build:
	mkdir -p ./build/$(PLATFORM)/$(CPU)/

	$(CC) $(CUSTOM_CFLAGS) $(C_ERRS) -ggdb -O2 -std=$(STD) \
		-D_POSIX_C_SOURCE=200809L \
		./src/tinylisp.c ./src/main.c \
		-I./vendor \
		-I./src \
		-o ./build/$(PLATFORM)/$(CPU)/$(APP).debug -lm

test:
	mkdir -p ./build/$(PLATFORM)/$(CPU)/

	$(CC) $(CUSTOM_CFLAGS) $(C_ERRS) -ggdb -O2 -std=$(STD) \
		-D_POSIX_C_SOURCE=200809L \
		./src/tinylisp.c ./src/test_main.c \
		-I./vendor \
		-I./src \
		-o ./build/$(PLATFORM)/$(CPU)/$(APP).test -lm

	./build/$(PLATFORM)/$(CPU)/$(APP).test

release_cli:
	mkdir -p ./build/$(PLATFORM)/$(CPU)/

	$(CC) $(CUSTOM_CFLAGS) $(C_ERRS) -O2 -std=$(STD) \
		-D_POSIX_C_SOURCE=200809L \
		./src/tinylisp.c ./src/main.c \
		-I./vendor \
		-I./src \
		-o ./build/$(PLATFORM)/$(CPU)/$(APP) -lm
