CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -DMURPHY_TEST -Itests/stubs -I.
FIXTURE_DIR ?= tests/fixtures
IPK ?= dist/murphytalk.pinyin_1.1.45_arm_jianpin_qtopia232_tolerantparse.ipk

FUZZ_SRCS = tests/fuzz_pinyin.cpp PinyinEngine.cpp scim/scim_pinyin.cpp phrase/PinyinPhrase.cpp public.cpp

.PHONY: all fixture fuzz clean

all: tests/fuzz_pinyin

tests/fuzz_pinyin: $(FUZZ_SRCS) PinyinEngine.h tests/stubs/qstring.h
	$(CXX) $(CXXFLAGS) -o $@ $(FUZZ_SRCS)

fixture:
	rm -rf $(FIXTURE_DIR)
	mkdir -p $(FIXTURE_DIR)
	tar xzf $(IPK) -C $(FIXTURE_DIR)
	tar xzf $(FIXTURE_DIR)/data.tar.gz -C $(FIXTURE_DIR)

fuzz: tests/fuzz_pinyin fixture
	MURPHY_TEST_PHRASE_FILE=$(FIXTURE_DIR)/home/zaurus/.murphytalk/murphytalk_phrase.dat \
	./tests/fuzz_pinyin scim/pinyin_table.txt \
		$(FIXTURE_DIR)/home/zaurus/.murphytalk/murphytalk_phrase_idx.txt \
		20000 tests/fuzz_regressions.txt 25

clean:
	rm -f tests/fuzz_pinyin
	rm -rf $(FIXTURE_DIR)
