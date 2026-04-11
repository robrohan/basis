.PHONY: build

CC=gcc
APP=basis

PLATFORM:=$(shell uname -s)
CPU:=$(shell uname -m)

C_ERRS += -Wall -Wextra -Wpedantic \
		-Wformat=2 -Wno-unused-parameter -Wshadow \
		-Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
		-Wredundant-decls -Wnested-externs -Wmissing-include-dirs \
		-Wno-unused -Wno-unused-parameter
STD:=c11

# Third-party GGUF reader (antirez/gguf-tools, BSD-2-Clause)
# Compiled without our strict warning flags to avoid noise from vendor code.
GGUF_INC  := -I./vendor/gguf
GGUF_OBJS  = ./build/$(PLATFORM)/$(CPU)/gguflib.o ./build/$(PLATFORM)/$(CPU)/fp16.o

ifeq ($(PLATFORM), Darwin)
#	the CI/CD doesn't have BLAS so we need to disable it
#	but only for mac CI/CD. Fall back to standard will work
	ifndef NO_BLAS
		BLAS_CFLAGS  = -DHAVE_BLAS -DACCELERATE_NEW_LAPACK -framework Accelerate
		BLAS_LDFLAGS = -framework Accelerate
	endif
	EDITLINE_CFLAGS  = -DHAVE_EDITLINE
	EDITLINE_LDFLAGS = -ledit
else
	BLAS_LIBS := $(shell pkg-config --libs openblas 2>/dev/null)
	ifneq ($(BLAS_LIBS),)
		BLAS_CFLAGS  = -DHAVE_BLAS $(shell pkg-config --cflags openblas)
		BLAS_LDFLAGS = $(shell pkg-config --libs openblas)
	endif
endif


./build/$(PLATFORM)/$(CPU)/gguflib.o: ./vendor/gguf/gguflib.c ./vendor/gguf/gguflib.h
	mkdir -p ./build/$(PLATFORM)/$(CPU)/
	$(CC) -O2 -std=$(STD) \
	-D_POSIX_C_SOURCE=200809L \
	$(GGUF_INC) \
	-c ./vendor/gguf/gguflib.c \
	-o $@ 

./build/$(PLATFORM)/$(CPU)/fp16.o: ./vendor/gguf/fp16.c ./vendor/gguf/fp16.h
	mkdir -p ./build/$(PLATFORM)/$(CPU)/
	$(CC) -O2 -std=$(STD) \
	-c ./vendor/gguf/fp16.c \
	-o $@ 

HASH = $(shell git log --pretty=format:'%h' -n 1)

help:
	@echo "make clean"
	@echo "make fetch"
	@echo "make download_gpt2"
	@echo "make build"
	@echo "make test"
	@echo "make release_cli"
	@echo "make run"

clean:
	rm -rf build

download_gpt2:
	mkdir -p ./models
	curl -L https://huggingface.co/QuantFactory/gpt2-GGUF/resolve/main/gpt2.Q4_0.gguf \
		-o ./models/gpt2.Q4_0.gguf

fetch:
	curl https://raw.githubusercontent.com/robrohan/r2/refs/heads/main/r2_maths.h > ./vendor/r2_maths.h
	curl https://raw.githubusercontent.com/robrohan/r2/refs/heads/main/r2_strings.h > ./vendor/r2_strings.h
	curl https://raw.githubusercontent.com/robrohan/r2/refs/heads/main/r2_termui.h > ./vendor/r2_termui.h
	curl https://raw.githubusercontent.com/robrohan/r2/refs/heads/main/r2_unit.h > ./vendor/r2_unit.h
	mkdir -p ./vendor/gguf
	curl https://raw.githubusercontent.com/antirez/gguf-tools/main/gguflib.h > ./vendor/gguf/gguflib.h
	curl https://raw.githubusercontent.com/antirez/gguf-tools/main/gguflib.c > ./vendor/gguf/gguflib.c
	curl https://raw.githubusercontent.com/antirez/gguf-tools/main/fp16.h    > ./vendor/gguf/fp16.h
	curl https://raw.githubusercontent.com/antirez/gguf-tools/main/fp16.c    > ./vendor/gguf/fp16.c
	curl https://raw.githubusercontent.com/antirez/gguf-tools/main/bf16.h    > ./vendor/gguf/bf16.h
	curl https://raw.githubusercontent.com/antirez/gguf-tools/main/LICENSE   > ./vendor/gguf/LICENSE


build: $(GGUF_OBJS)
	mkdir -p ./build/$(PLATFORM)/$(CPU)/

	$(CC) $(CUSTOM_CFLAGS) $(C_ERRS) -ggdb -O2 -std=$(STD) \
		-D_POSIX_C_SOURCE=200809L \
		-DVERSION=\"$(HASH)\" \
		./src/tinylisp.c ./src/tinytensor.c ./src/tinysymbolic.c ./src/runtime.c ./src/gguf_loader.c ./src/tokenizer.c ./src/repl.c ./src/main.c \
		$(BLAS_CFLAGS) $(EDITLINE_CFLAGS) \
		$(BLAS_LDFLAGS) \
		$(GGUF_OBJS) \
		-I./vendor \
		-I./src \
		$(GGUF_INC) \
		-o ./build/$(PLATFORM)/$(CPU)/$(APP).debug -lm $(EDITLINE_LDFLAGS)

test: $(GGUF_OBJS)
	mkdir -p ./build/$(PLATFORM)/$(CPU)/

	$(CC) $(CUSTOM_CFLAGS) $(C_ERRS) -ggdb -O2 -std=$(STD) \
		-D_POSIX_C_SOURCE=200809L \
		-DVERSION=\"$(HASH)\" \
		./src/tinylisp.c ./src/tinytensor.c ./src/tinysymbolic.c \
		./src/runtime.c ./src/gguf_loader.c ./src/tokenizer.c \
		./src/test_main.c \
		$(BLAS_CFLAGS) \
		$(BLAS_LDFLAGS) \
		$(GGUF_OBJS) \
		-I./vendor \
		-I./src \
		$(GGUF_INC) \
		-o ./build/$(PLATFORM)/$(CPU)/$(APP).test -lm 

	./build/$(PLATFORM)/$(CPU)/$(APP).test

release_cli: $(GGUF_OBJS)
	mkdir -p ./build/$(PLATFORM)/$(CPU)/

	$(CC) $(CUSTOM_CFLAGS) $(C_ERRS) -O3 -march=native -std=$(STD) \
		-D_POSIX_C_SOURCE=200809L \
		-DVERSION=\"$(HASH)\" \
		$(BLAS_CFLAGS) \
		./src/tinylisp.c ./src/tinytensor.c ./src/tinysymbolic.c \
		./src/runtime.c ./src/gguf_loader.c ./src/tokenizer.c ./src/repl.c ./src/main.c \
		$(GGUF_OBJS) \
		-I./vendor \
		-I./src \
		$(GGUF_INC) \
		$(BLAS_LDFLAGS) \
		-o ./build/$(PLATFORM)/$(CPU)/$(APP) -lm $(EDITLINE_LDFLAGS)

ifeq ($(PLATFORM), Darwin)
	otool -L ./build/$(PLATFORM)/$(CPU)/$(APP)
else
	ldd ./build/$(PLATFORM)/$(CPU)/$(APP)
endif
