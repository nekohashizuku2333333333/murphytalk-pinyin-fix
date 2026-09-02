CXX ?= g++
REPO ?= ..
OUT ?= fuzz_pinyin
CXXFLAGS ?= -O2 -Wall -Wextra -DMURPHY_TEST -I$(CURDIR)/stubs -I$(REPO)
FIXTURE_DIR ?= $(CURDIR)/fixtures
IPK ?= $(REPO)/dist/murphytalk.pinyin_1.1.45_arm_jianpin_qtopia232_tolerantparse.ipk

FUZZ_SRCS = $(CURDIR)/fuzz_pinyin.cpp $(REPO)/PinyinEngine.cpp $(REPO)/scim/scim_pinyin.cpp $(REPO)/phrase/PinyinPhrase.cpp $(REPO)/public.cpp

.PHONY: all fixture fuzz clean

all: $(OUT)

$(OUT): $(FUZZ_SRCS) $(REPO)/PinyinEngine.h $(CURDIR)/stubs/qstring.h
	$(CXX) $(CXXFLAGS) -o $@ $(FUZZ_SRCS)

fixture:
	rm -rf $(FIXTURE_DIR)
	mkdir -p $(FIXTURE_DIR)
	tar xzf $(IPK) -C $(FIXTURE_DIR)
	tar xzf $(FIXTURE_DIR)/data.tar.gz -C $(FIXTURE_DIR)

fuzz: $(OUT) fixture
	MURPHY_TEST_PHRASE_FILE=$(FIXTURE_DIR)/home/zaurus/.murphytalk/murphytalk_phrase.dat \
	./$(OUT) $(REPO)/scim/pinyin_table.txt \
		$(FIXTURE_DIR)/home/zaurus/.murphytalk/murphytalk_phrase_idx.txt \
		20000 $(CURDIR)/fuzz_regressions.txt 25

clean:
	rm -f $(OUT)
	rm -rf $(FIXTURE_DIR)
