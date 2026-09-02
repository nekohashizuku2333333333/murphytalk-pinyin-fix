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

PinyinEngine::PinyinEngine(const char *table_file,const char *phrase_index_file)
	:m_table(NULL,table_file),m_table_filename(table_file),
	 m_initial_lookup(false),m_partial_lookup(false),m_commit_pinyin_length(0),
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
	m_commit_pinyin_length = pinyin_len;

	if(pinyin_len == 1){
		PinyinInitial initial = get_initial_from_letter(pinyin[0]);
		if(initial != SCIM_PINYIN_ZeroInitial){
			m_key.clear_key();
			m_initial_lookup = true;
			return m_table.find_chars_by_initial(m_chars,initial);
		}
	}

	if(pinyin_len > 1 && strchr(pinyin,' ') && m_key.set_mixed_key(pinyin)){
		unsigned int count=m_phrases_table.find_phrases(m_offset_freq_pairs,m_key);
		if(count > 0){
			m_phrases_table.get_phrases_by_offsets(m_offset_freq_pairs,m_phrases);
			return count;
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

	if(pinyin_len > 1 && m_key.set_initials_key(pinyin)){
		unsigned int count=m_phrases_table.find_phrases(m_offset_freq_pairs,m_key);
		if(count > 0){
			m_phrases_table.get_phrases_by_offsets(m_offset_freq_pairs,m_phrases);
			return count;
		}
	}

	m_key.set_key(pinyin);

	if(isPhrase()){
		unsigned int count=m_phrases_table.find_phrases(m_offset_freq_pairs,m_key);
		m_phrases_table.get_phrases_by_offsets(m_offset_freq_pairs,m_phrases);
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
		}
		return count;
	}
	else{
		return m_table.find_chars(m_chars,m_key.get_key_by_index(0));
	}
}

QChar PinyinEngine::get_char(unsigned int index)
{	
	//if(index>=m_chars.size()) return QChar();
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

void PinyinEngine::hit(unsigned int index)
{
	if(m_initial_lookup)
		return;

	if(isPhrase()){
		m_phrases_table.set_frequency(m_offset_freq_pairs[index].first,
					      m_offset_freq_pairs[index].second+1);
	}
	else{
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
