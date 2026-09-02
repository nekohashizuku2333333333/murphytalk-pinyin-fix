CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -DX86 -Itests/stubs -I.

FUZZ_SRCS = tests/fuzz_pinyin.cpp PinyinEngine.cpp scim/scim_pinyin.cpp phrase/PinyinPhrase.cpp public.cpp

.PHONY: all fuzz clean

all: tests/fuzz_pinyin

tests/fuzz_pinyin: $(FUZZ_SRCS) PinyinEngine.h tests/stubs/qstring.h
	$(CXX) $(CXXFLAGS) -o $@ $(FUZZ_SRCS)

fuzz: tests/fuzz_pinyin
	./tests/fuzz_pinyin scim/pinyin_table.txt "" 20000 tests/fuzz_regressions.txt

clean:
	rm -f tests/fuzz_pinyin
