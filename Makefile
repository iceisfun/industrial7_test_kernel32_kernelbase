# Each directory under tests/ becomes build/<dir>.exe (all .c in it + common/t.c).
# user32 is deliberately NOT linked; everything resolves via GetProcAddress.
CC      = x86_64-w64-mingw32-gcc
SYSINC  ?= /usr/x86_64-w64-mingw32/include
SYSLIB  ?= /usr/x86_64-w64-mingw32/lib
CFLAGS  ?= -O1 -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-format
AREAS   := $(notdir $(wildcard tests/*))
EXES    := $(addprefix build/,$(addsuffix .exe,$(AREAS)))
all: $(EXES)
.SECONDEXPANSION:
build/%.exe: $$(wildcard tests/$$*/*.c) common/t.c common/t.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Icommon -o $@ $(wildcard tests/$*/*.c) common/t.c -static -lkernel32 -ladvapi32 -lntdll -lshell32
clean: ; rm -rf build
# Sanity-run under wine (for harness/compile validation only; the authority is real Win10).
WINE ?= /home/iceisfun/work/wine/proton-ge-custom/build/dist/files/bin/wine
WINEPREFIX ?= /tmp/claude-1000/-home-iceisfun-test-user32/0dbca224-e0c6-4aac-8035-d3516ec0a47c/scratchpad/wpge
wine-run: all
	@for e in $(EXES); do echo "== $$e"; WINEPREFIX=$(WINEPREFIX) WINEARCH=win64 WINEDEBUG=-all timeout 300 $(WINE) $$e 2>/dev/null | grep -E '^(FAIL|SKIP|SUMMARY)|^  (FAIL|NOTE)'; done
.PHONY: all clean wine-run
