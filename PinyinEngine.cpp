/**********************************************************************
** Copyright (C) 2004 MurphyTalk
**
** This file is part of the MurphyTalk PinYin Chinese input method,
** which is for the Qtopia Environment.
**
** This file is partially based on Smart Chinese Input Method 
** by James Su
** Copyright (c) 2002 James Su <suzhe@turbolinux.com.cn>
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
**
** murphytalk@gmail.com
** $Date: 2004/07/20 11:26:05 $
**
**********************************************************************/
#include "PinyinEngine.h"
#include "public.h"

#include <string.h>

static PinyinInitial get_initial_from_letter(char c)
{
	switch(c){
		case 'b': return SCIM_PINYIN_Bo;
		case 'c': return SCIM_PINYIN_Ci;
		case 'd': return SCIM_PINYIN_De;
		case 'f': return SCIM_PINYIN_Fo;
		case 'g': return SCIM_PINYIN_Ge;
		case 'h': return SCIM_PINYIN_He;
		case 'j': return SCIM_PINYIN_Ji;
		case 'k': return SCIM_PINYIN_Ke;
		case 'l': return SCIM_PINYIN_Le;
		case 'm': return SCIM_PINYIN_Mo;
		case 'n': return SCIM_PINYIN_Ne;
		case 'p': return SCIM_PINYIN_Po;
		case 'q': return SCIM_PINYIN_Qi;
		case 'r': return SCIM_PINYIN_Ri;
		case 's': return SCIM_PINYIN_Si;
		case 't': return SCIM_PINYIN_Te;
		case 'w': return SCIM_PINYIN_Wo;
		case 'x': return SCIM_PINYIN_Xi;
		case 'y': return SCIM_PINYIN_Yi;
		case 'z': return SCIM_PINYIN_Zi;
		default: return SCIM_PINYIN_ZeroInitial;
	}
}

static unsigned int get_first_token(const char *pinyin,unsigned int pinyin_len,const char **token)
{
	const char *p = pinyin;
	const char *end = pinyin + pinyin_len;
	while(p < end && *p == ' ')
		p++;
	if(p >= end)
		return 0;

	const char *start = p;
	while(p < end && *p != ' ')
		p++;

	*token = start;
	return p - start;
}

static unsigned int get_token_consumed_length(const char *pinyin,unsigned int pinyin_len,
					       const char *token,unsigned int token_len)
{
	const char *p = token + token_len;
	const char *end = pinyin + pinyin_len;
	while(p < end && *p == ' ')
		p++;
	return p - pinyin;
}

static bool phrase_offset_exists(const PhraseOffsetFrequencyPairVector &phrases,uint32 offset)
{
	for(PhraseOffsetFrequencyPairVector::const_iterator i=phrases.begin();i!=phrases.end();i++){
		if(i->first == offset)
			return true;
	}
	return false;
}

static void append_unique_phrases(PhraseOffsetFrequencyPairVector &dst,
				  const PhraseOffsetFrequencyPairVector &src)
{
	for(PhraseOffsetFrequencyPairVector::const_iterator i=src.begin();i!=src.end();i++){
		if(!phrase_offset_exists(dst,i->first))
			dst.push_back(*i);
	}
}

bool PinyinEngine::append_first_syllable_chars(const char *pinyin,unsigned int pinyin_len)
{
	if(pinyin_len <= 1)
		return false;

	PinyinKey first_key;
	int first_len = first_key.set_key(scim_default_pinyin_validator,pinyin);
	if(first_len <= 0 || (unsigned int)first_len >= pinyin_len)
		return false;

	unsigned int char_count = m_table.find_chars(m_chars,first_key);
	if(char_count == 0)
		return false;

	m_mixed_candidates = true;
	m_mixed_char_commit_length = first_len;
	return true;
}

bool PinyinEngine::fallback_first_syllable_chars(const char *pinyin,unsigned int pinyin_len)
{
	m_chars.clear();
	if(!append_first_syllable_chars(pinyin,pinyin_len))
		return false;

	m_key.clear_key();
	m_initial_lookup = true;
	m_commit_pinyin_length = m_mixed_char_commit_length;
	return true;
}

static bool find_first_initial_chars(PinyinTable &table,CharVector &chars,
				     const char *pinyin,unsigned int pinyin_len)
{
	if(pinyin_len <= 1)
		return false;

	PinyinInitial initial = get_initial_from_letter(pinyin[0]);
	if(initial == SCIM_PINYIN_ZeroInitial)
		return false;

	chars.clear();
	return table.find_chars_by_initial(chars,initial) > 0;
}

struct TolerantSegment
{
	PinyinKey key;
	unsigned int start;
	unsigned int len;
	bool complete;
	bool pending;
};

typedef std::vector<TolerantSegment> TolerantSegmentVector;

struct TolerantSplit
{
	TolerantSegmentVector segments;
	int score;
};

typedef std::vector<TolerantSplit> TolerantSplitVector;

static bool split_better(const TolerantSplit &lhs,const TolerantSplit &rhs)
{
	if(lhs.score != rhs.score)
		return lhs.score > rhs.score;
	return lhs.segments.size() < rhs.segments.size();
}

static bool is_complete_key(const char *pinyin,unsigned int len,PinyinKey &key)
{
	int parsed = key.set_key(scim_default_pinyin_validator,pinyin,len);
	return parsed == (int)len && key.get_final() != SCIM_PINYIN_ZeroFinal;
}

static bool is_incomplete_piece(const char *pinyin,unsigned int len,PinyinKey &key)
{
	if(len == 0 || len > 2)
		return false;

	char buf[3];
	memcpy(buf,pinyin,len);
	buf[len] = 0;

	if(len == 1){
		PinyinInitial initial = get_initial_from_letter(buf[0]);
		if(initial == SCIM_PINYIN_ZeroInitial)
			return false;
		key = PinyinKey(initial, SCIM_PINYIN_ZeroFinal, SCIM_PINYIN_ZeroTone);
		return true;
	}

	if(strcmp(buf,"zh") == 0){
		key = PinyinKey(SCIM_PINYIN_Zhi, SCIM_PINYIN_ZeroFinal, SCIM_PINYIN_ZeroTone);
		return true;
	}
	if(strcmp(buf,"ch") == 0){
		key = PinyinKey(SCIM_PINYIN_Chi, SCIM_PINYIN_ZeroFinal, SCIM_PINYIN_ZeroTone);
		return true;
	}
	if(strcmp(buf,"sh") == 0){
		key = PinyinKey(SCIM_PINYIN_Shi, SCIM_PINYIN_ZeroFinal, SCIM_PINYIN_ZeroTone);
		return true;
	}

	return false;
}

static void collect_tolerant_splits(const char *pinyin,unsigned int pinyin_len,
				    unsigned int pos,TolerantSegmentVector &current,
				    int score,TolerantSplitVector &splits)
{
	if(splits.size() >= 32)
		return;

	while(pos < pinyin_len && pinyin[pos] == '\'')
		pos++;

	if(pos >= pinyin_len){
		if(current.size() > 0){
			TolerantSplit split;
			split.segments = current;
			split.score = score;
			splits.push_back(split);
		}
		return;
	}

	if(!isalpha(pinyin[pos]))
		return;

	unsigned int max_len = SCIM_PINYIN_KEY_MAXLEN;
	if(pos + max_len > pinyin_len)
		max_len = pinyin_len - pos;

	for(unsigned int len=max_len;len>0;len--){
		bool has_separator = false;
		for(unsigned int i=0;i<len;i++){
			if(pinyin[pos+i] == '\''){
				has_separator = true;
				break;
			}
		}
		if(has_separator)
			continue;

		PinyinKey key;
		if(is_complete_key(pinyin+pos,len,key)){
			TolerantSegment seg;
			seg.key = key;
			seg.start = pos;
			seg.len = len;
			seg.complete = true;
			seg.pending = false;
			current.push_back(seg);
			collect_tolerant_splits(pinyin,pinyin_len,pos+len,current,score+(int)len*10,splits);
			current.pop_back();
		}
	}

	unsigned int incomplete_max = 2;
	if(pos + incomplete_max > pinyin_len)
		incomplete_max = pinyin_len - pos;
	for(unsigned int len=incomplete_max;len>0;len--){
		PinyinKey key;
		if(is_incomplete_piece(pinyin+pos,len,key)){
			TolerantSegment seg;
			seg.key = key;
			seg.start = pos;
			seg.len = len;
			seg.complete = false;
			seg.pending = (pos + len >= pinyin_len);
			current.push_back(seg);
			collect_tolerant_splits(pinyin,pinyin_len,pos+len,current,
						score + (seg.pending ? (int)len*9 : -((int)len*4)),splits);
			current.pop_back();
		}
	}
}

static void collect_tolerant_splits(const char *pinyin,unsigned int pinyin_len,
				    TolerantSplitVector &splits)
{
	TolerantSegmentVector current;
	splits.clear();
	collect_tolerant_splits(pinyin,pinyin_len,0,current,0,splits);
	std::sort(splits.begin(),splits.end(),split_better);
}

static String format_tolerant_split(const char *pinyin,const TolerantSplit &split)
{
	String result;
	for(unsigned int i=0;i<split.segments.size();i++){
		if(i>0)
			result += " ";
		result.append(pinyin + split.segments[i].start,split.segments[i].len);
	}
	return result;
}

static void collect_pinyin_splits(const char *pinyin,unsigned int pinyin_len,
				  unsigned int pos,PinyinKeyVector &current,
				  std::vector<PinyinKeyVector> &splits)
{
	if(splits.size() >= 16)
		return;

	while(pos < pinyin_len && pinyin[pos] == '\'')
		pos++;

	if(pos >= pinyin_len){
		if(current.size() > 1)
			splits.push_back(current);
		return;
	}

	if(!isalpha(pinyin[pos]))
		return;

	unsigned int max_len = SCIM_PINYIN_KEY_MAXLEN;
	if(pos + max_len > pinyin_len)
		max_len = pinyin_len - pos;

	for(unsigned int len=max_len;len>0;len--){
		bool has_separator = false;
		for(unsigned int i=0;i<len;i++){
			if(pinyin[pos+i] == '\''){
				has_separator = true;
				break;
			}
		}
		if(has_separator)
			continue;

		PinyinKey key;
		int parsed = key.set_key(scim_default_pinyin_validator,pinyin+pos,len);
		if(parsed == (int)len && key.get_final() != SCIM_PINYIN_ZeroFinal){
			current.push_back(key);
			collect_pinyin_splits(pinyin,pinyin_len,pos+len,current,splits);
			current.pop_back();
		}
	}
}

static void collect_pinyin_splits(const char *pinyin,unsigned int pinyin_len,
				  std::vector<PinyinKeyVector> &splits)
{
	PinyinKeyVector current;
	splits.clear();
	collect_pinyin_splits(pinyin,pinyin_len,0,current,splits);
}

static void collect_mixed_splits(const char *pinyin,unsigned int pinyin_len,
				 unsigned int pos,PinyinKeyVector &current,
				 std::vector<PinyinKeyVector> &splits)
{
	if(splits.size() >= 16)
		return;

	if(pos >= pinyin_len){
		if(current.size() > 1)
			splits.push_back(current);
		return;
	}

	if(!isalpha(pinyin[pos]))
		return;

	unsigned int max_len = SCIM_PINYIN_KEY_MAXLEN;
	if(pos + max_len > pinyin_len)
		max_len = pinyin_len - pos;

	for(unsigned int len=max_len;len>1;len--){
		PinyinKey key;
		int parsed = key.set_key(scim_default_pinyin_validator,pinyin+pos,len);
		if(parsed == (int)len && key.get_final() != SCIM_PINYIN_ZeroFinal){
			current.push_back(key);
			collect_mixed_splits(pinyin,pinyin_len,pos+len,current,splits);
			current.pop_back();
		}
	}

	PinyinInitial initial = get_initial_from_letter(pinyin[pos]);
	if(initial != SCIM_PINYIN_ZeroInitial){
		current.push_back(PinyinKey(initial, SCIM_PINYIN_ZeroFinal, SCIM_PINYIN_ZeroTone));
		collect_mixed_splits(pinyin,pinyin_len,pos+1,current,splits);
		current.pop_back();
	}
}

static void collect_mixed_splits(const char *pinyin,unsigned int pinyin_len,
				 std::vector<PinyinKeyVector> &splits)
{
	PinyinKeyVector current;
	splits.clear();
	collect_mixed_splits(pinyin,pinyin_len,0,current,splits);
}

PinyinEngine::PinyinEngine(const char *table_file,const char *phrase_index_file)
	:m_table(NULL,table_file),m_table_filename(table_file),
	 m_initial_lookup(false),m_partial_lookup(false),m_mixed_candidates(false),
	 m_commit_pinyin_length(0),m_mixed_char_commit_length(0),m_phrase_candidate_count(0),
	 m_display_pinyin(""),m_raw_pinyin(""),m_has_pending_pinyin(false),
	 m_phrases_table(phrase_index_file),m_phrase_idx_filename(phrase_index_file)
{
}

PinyinEngine::~PinyinEngine()
{
	save_table();
}

unsigned int PinyinEngine::search(const char* pinyin)
{
	unsigned int pinyin_len = pinyin ? strlen(pinyin) : 0;
	m_initial_lookup = false;
	m_partial_lookup = false;
	m_mixed_candidates = false;
	m_commit_pinyin_length = pinyin_len;
	m_mixed_char_commit_length = pinyin_len;
	m_phrase_candidate_count = 0;
	m_display_pinyin = pinyin ? pinyin : "";
	m_raw_pinyin = pinyin ? pinyin : "";
	m_has_pending_pinyin = false;
	m_chars.clear();
	m_phrases.clear();

	if(pinyin_len == 0){
		m_key.clear_key();
		return 0;
	}

	TolerantSplitVector tolerant_splits;
	collect_tolerant_splits(pinyin,pinyin_len,tolerant_splits);
	if(tolerant_splits.size() > 0){
		m_display_pinyin = format_tolerant_split(pinyin,tolerant_splits[0]);
		m_has_pending_pinyin = tolerant_splits[0].segments.back().pending;

		PhraseOffsetFrequencyPairVector all_pairs;
		PhraseOffsetFrequencyPairVector pairs;
		unsigned int split_limit = std::min(static_cast<unsigned int>(tolerant_splits.size()),
						    static_cast<unsigned int>(4));
		for(unsigned int i=0;i<split_limit;i++){
			if(tolerant_splits[i].segments.size() <= 1)
				continue;
			PinyinKeyVector keys;
			for(unsigned int j=0;j<tolerant_splits[i].segments.size();j++)
				keys.push_back(tolerant_splits[i].segments[j].key);
			m_key.set_key_vector(keys);
			if(m_phrases_table.find_phrases(pairs,m_key) > 0)
				append_unique_phrases(all_pairs,pairs);
		}

		if(all_pairs.size() > 0){
			m_offset_freq_pairs = all_pairs;
			m_phrases_table.get_phrases_by_offsets(m_offset_freq_pairs,m_phrases);
			m_phrase_candidate_count = m_phrases.size();
		}

		const TolerantSegment &first = tolerant_splits[0].segments[0];
		if(first.complete){
			m_table.find_chars(m_chars,first.key);
			char first_pinyin[SCIM_PINYIN_KEY_MAXLEN+1];
			memcpy(first_pinyin,pinyin+first.start,first.len);
			first_pinyin[first.len]=0;
			m_key.set_key(first_pinyin);
		}
		else{
			m_table.find_chars_by_initial(m_chars,first.key.get_initial());
			m_key.clear_key();
		}

		m_initial_lookup = (m_phrase_candidate_count == 0 && !first.complete);
		m_mixed_candidates = m_phrase_candidate_count > 0;
		m_mixed_char_commit_length = first.start + first.len;
		while(m_mixed_char_commit_length < pinyin_len &&
		      pinyin[m_mixed_char_commit_length] == '\'')
			m_mixed_char_commit_length++;
		m_commit_pinyin_length = (m_phrase_candidate_count > 0) ? pinyin_len : m_mixed_char_commit_length;

		if(m_phrase_candidate_count + m_chars.size() > 0)
			return m_phrase_candidate_count + m_chars.size();
	}
	else{
		m_has_pending_pinyin = pinyin_len > 0;
	}

	if(pinyin_len == 1){
		PinyinInitial initial = get_initial_from_letter(pinyin[0]);
		if(initial != SCIM_PINYIN_ZeroInitial){
			m_key.clear_key();
			m_initial_lookup = true;
			return m_table.find_chars_by_initial(m_chars,initial);
		}
	}

	if(pinyin_len > 1){
		PinyinKey whole_key;
		int whole_len = whole_key.set_key(scim_default_pinyin_validator,pinyin,pinyin_len);
		if(whole_len == (int)pinyin_len && whole_key.get_final() != SCIM_PINYIN_ZeroFinal){
			unsigned int char_count = m_table.find_chars(m_chars,whole_key);
			if(char_count > 0){
				char whole_pinyin[SCIM_PINYIN_KEY_MAXLEN+1];
				memcpy(whole_pinyin,pinyin,pinyin_len);
				whole_pinyin[pinyin_len]=0;
				m_key.set_key(whole_pinyin);
				m_commit_pinyin_length = pinyin_len;
				return char_count;
			}
		}
	}

	if(pinyin_len > 1){
		std::vector<PinyinKeyVector> splits;
		collect_pinyin_splits(pinyin,pinyin_len,splits);
		if(splits.size() > 0){
			PhraseOffsetFrequencyPairVector all_pairs;
			PhraseOffsetFrequencyPairVector pairs;
			for(std::vector<PinyinKeyVector>::iterator i=splits.begin();i!=splits.end();i++){
				m_key.set_key_vector(*i);
				unsigned int count=m_phrases_table.find_phrases(pairs,m_key);
				if(count > 0)
					append_unique_phrases(all_pairs,pairs);
			}
			if(all_pairs.size() > 0){
				m_offset_freq_pairs = all_pairs;
				m_phrases_table.get_phrases_by_offsets(m_offset_freq_pairs,m_phrases);
				m_phrase_candidate_count = m_phrases.size();
				append_first_syllable_chars(pinyin,pinyin_len);
				return m_phrase_candidate_count + m_chars.size();
			}
		}
	}

	if(pinyin_len > 1 && strchr(pinyin,' ') && m_key.set_mixed_key(pinyin)){
		unsigned int count=m_phrases_table.find_phrases(m_offset_freq_pairs,m_key);
			if(count > 0){
				m_phrases_table.get_phrases_by_offsets(m_offset_freq_pairs,m_phrases);
				m_phrase_candidate_count = m_phrases.size();
				const char *token = NULL;
				unsigned int token_len = get_first_token(pinyin,pinyin_len,&token);
			if(token_len == 1){
				PinyinInitial initial = get_initial_from_letter(token[0]);
				if(initial != SCIM_PINYIN_ZeroInitial &&
				   m_table.find_chars_by_initial(m_chars,initial) > 0)
					m_mixed_candidates = true;
				m_mixed_char_commit_length = get_token_consumed_length(pinyin,pinyin_len,token,token_len);
			}
			return m_phrase_candidate_count + m_chars.size();
		}

		const char *token = NULL;
		unsigned int token_len = get_first_token(pinyin,pinyin_len,&token);
		if(token_len == 1){
			PinyinInitial initial = get_initial_from_letter(token[0]);
			if(initial != SCIM_PINYIN_ZeroInitial){
				m_key.clear_key();
				m_initial_lookup = true;
				m_commit_pinyin_length = get_token_consumed_length(pinyin,pinyin_len,token,token_len);
				return m_table.find_chars_by_initial(m_chars,initial);
			}
		}
		else if(token_len > 1){
			PinyinKey first_key;
			int parsed = first_key.set_key(scim_default_pinyin_validator,token,token_len);
			if(parsed == (int)token_len && first_key.get_final() != SCIM_PINYIN_ZeroFinal){
				unsigned int char_count=m_table.find_chars(m_chars,first_key);
				if(char_count > 0){
					char first_pinyin[SCIM_PINYIN_KEY_MAXLEN+1];
					memcpy(first_pinyin,token,token_len);
					first_pinyin[token_len]=0;
					m_key.set_key(first_pinyin);
					m_partial_lookup = true;
					m_commit_pinyin_length = get_token_consumed_length(pinyin,pinyin_len,token,token_len);
					return char_count;
				}
			}
		}
	}

	if(pinyin_len > 1 && !strchr(pinyin,'\'')){
		std::vector<PinyinKeyVector> splits;
		collect_mixed_splits(pinyin,pinyin_len,splits);
		if(splits.size() > 0){
			PhraseOffsetFrequencyPairVector all_pairs;
			PhraseOffsetFrequencyPairVector pairs;
			for(std::vector<PinyinKeyVector>::iterator i=splits.begin();i!=splits.end();i++){
				m_key.set_key_vector(*i);
				unsigned int count=m_phrases_table.find_phrases(pairs,m_key);
				if(count > 0)
					append_unique_phrases(all_pairs,pairs);
			}
			if(all_pairs.size() > 0){
				m_offset_freq_pairs = all_pairs;
				m_phrases_table.get_phrases_by_offsets(m_offset_freq_pairs,m_phrases);
				m_phrase_candidate_count = m_phrases.size();
				if(m_table.find_chars_by_initial(m_chars,get_initial_from_letter(pinyin[0])) > 0)
					m_mixed_candidates = true;
				m_mixed_char_commit_length = 1;
				return m_phrase_candidate_count + m_chars.size();
			}
		}
	}

	if(pinyin_len > 1 && m_key.set_initials_key(pinyin)){
		unsigned int count=m_phrases_table.find_phrases(m_offset_freq_pairs,m_key);
		if(count > 0){
			m_phrases_table.get_phrases_by_offsets(m_offset_freq_pairs,m_phrases);
			m_phrase_candidate_count = m_phrases.size();
			if(m_table.find_chars_by_initial(m_chars,get_initial_from_letter(pinyin[0])) > 0)
				m_mixed_candidates = true;
			m_mixed_char_commit_length = 1;
			return m_phrase_candidate_count + m_chars.size();
		}
	}

	m_key.set_key(pinyin);

	if(!m_key.isValid()){
		if(find_first_initial_chars(m_table,m_chars,pinyin,pinyin_len)){
			m_key.clear_key();
			m_initial_lookup = true;
			m_commit_pinyin_length = 1;
			return m_chars.size();
		}
		return 0;
	}

	if(isPhrase()){
		unsigned int count=m_phrases_table.find_phrases(m_offset_freq_pairs,m_key);
		m_phrases_table.get_phrases_by_offsets(m_offset_freq_pairs,m_phrases);
		m_phrase_candidate_count = m_phrases.size();
		if(count > 0){
			append_first_syllable_chars(pinyin,pinyin_len);
			return m_phrase_candidate_count + m_chars.size();
		}
		if(count == 0 && pinyin_len > 1){
			PinyinKey first_key;
			int first_len = first_key.set_key(scim_default_pinyin_validator,pinyin);
			if(first_len > 0 && (unsigned int)first_len < pinyin_len){
				unsigned int char_count=m_table.find_chars(m_chars,first_key);
				if(char_count > 0){
					char first_pinyin[SCIM_PINYIN_KEY_MAXLEN+1];
					memcpy(first_pinyin,pinyin,first_len);
					first_pinyin[first_len]=0;
					m_key.set_key(first_pinyin);
					m_partial_lookup = true;
					m_commit_pinyin_length = first_len;
					return char_count;
				}
			}
			if(fallback_first_syllable_chars(pinyin,pinyin_len)){
				return m_chars.size();
			}
			if(find_first_initial_chars(m_table,m_chars,pinyin,pinyin_len)){
				m_key.clear_key();
				m_initial_lookup = true;
				m_commit_pinyin_length = 1;
				return m_chars.size();
			}
		}
		return count;
	}
	else{
		unsigned int char_count=m_table.find_chars(m_chars,m_key.get_key_by_index(0));
		if(char_count == 0 && fallback_first_syllable_chars(pinyin,pinyin_len)){
			return m_chars.size();
		}
		if(char_count == 0 && find_first_initial_chars(m_table,m_chars,pinyin,pinyin_len)){
			m_key.clear_key();
			m_initial_lookup = true;
			m_commit_pinyin_length = 1;
			return m_chars.size();
		}
		return char_count;
	}

	return 0;
}

QChar PinyinEngine::get_char(unsigned int index)
{	
	//if(index>=m_chars.size()) return QChar();
	if(index >= m_phrase_candidate_count)
		index -= m_phrase_candidate_count;
	QChar c(m_chars[index]);
	return c;
}

QString PinyinEngine::get_phrase(unsigned int index)
{
	QString str;
	for(unsigned int i = 0;i<m_phrases[index].size();i++){
		str+=QChar(m_phrases[index][i]);
	}
	return str;
}

bool PinyinEngine::is_phrase_candidate(unsigned int index)
{
	return index < m_phrase_candidate_count;
}

unsigned int PinyinEngine::get_commit_pinyin_length(unsigned int index)
{
	if(index >= m_phrase_candidate_count)
		return m_mixed_char_commit_length;
	return m_commit_pinyin_length;
}

void PinyinEngine::hit(unsigned int index)
{
	if(m_initial_lookup)
		return;

	if(is_phrase_candidate(index)){
		m_phrases_table.set_frequency(m_offset_freq_pairs[index].first,
					      m_offset_freq_pairs[index].second+1);
	}
	else{
		if(index >= m_phrase_candidate_count)
			index -= m_phrase_candidate_count;
		if(m_mixed_candidates)
			return;
		PinyinKey& key=m_key.get_key_by_index(0);
		uint32 freq=m_table.get_char_frequency(m_chars[index],key)+1;
		m_table.set_char_frequency(m_chars[index],freq,key);
	}
}

void PinyinEngine::save_table()
{
	m_table.save_table(m_table_filename.c_str());
	m_phrases_table.save_index(m_phrase_idx_filename.c_str());
	printX86("table saved\n");
}

void PinyinEngine::append_phrase(QString& phrase,const char* pinyin)
{
	if(phrase.length()<2)  return;

	String sPinyin(pinyin);
	trim(sPinyin);

	if(sPinyin.size()==0)  return;

	PhraseString str;
	for(unsigned int i=0;i<phrase.length();i++){
		str.push_back(phrase[i].unicode());
	}
	m_phrases_table.append_phrase(str,sPinyin.c_str());
}
/*
 * Revision history
 * 
 * $Log: PinyinEngine.cpp,v $
 * Revision 1.3  2004/07/20 11:26:05  Lu Mu
 * (1)phrase frequency
 * (2)self define phrase
 *
 * Revision 1.2  2004/07/17 07:10:45  Lu Mu
 * phrase support
 *
 * Revision 1.1  2004/07/10 15:02:22  Lu Mu
 * v0.0.1 released
 * TODO: phase support
 *
 *
 */
