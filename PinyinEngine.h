
/**********************************************************************
** Copyright (C) 2004 MurphyTalk
**
** This file is part of the MurphyTalk PinYin Chinese input method,
** which is for the Qtopia Environment.
**
** This is a based on Smart Chinese Input Method 
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
#ifndef PINYINENGIN_H
#define PINYINENGIN_H

#include <qstring.h>
#include "scim/scim_pinyin.h"
#include "phrase/PinyinPhrase.h"

class PinyinEngine
{
public:
	PinyinEngine(const char *table_file,const char *phrase_index_file);
	~PinyinEngine();	

	unsigned int search(const char* pinyin);
	inline unsigned int get_commit_pinyin_length(){
		return m_commit_pinyin_length;
	}
	unsigned int get_commit_pinyin_length(unsigned int index);
	inline unsigned int get_phrase_candidate_count(){
		return m_phrase_candidate_count;
	}
	inline unsigned int get_char_count(){
		return m_chars.size();
	}
	inline String get_display_pinyin(){
		return m_display_pinyin;
	}
	inline String get_raw_pinyin(){
		return m_raw_pinyin;
	}
	inline bool has_pending_pinyin(){
		return m_has_pending_pinyin;
	}
	QChar get_char(unsigned int index);
	QString get_phrase(unsigned int index);
	bool is_phrase_candidate(unsigned int index);
	void hit(unsigned int index);
	void save_table();
	void append_phrase(QString&,const char* pinyin);
	inline bool isPhrase(){
		return m_key.get_key_count()>1;
	}
	inline String get_formatted_pinyin(){
		return m_key.get_key_string();
	}
	inline void clear_key(){
		m_key.clear_key();
		m_chars.clear();
		m_phrases.clear();
		m_offset_freq_pairs.clear();
		m_initial_lookup=false;
		m_partial_lookup=false;
		m_mixed_candidates=false;
		m_commit_pinyin_length=0;
		m_mixed_char_commit_length=0;
		m_phrase_candidate_count=0;
		m_display_pinyin="";
		m_raw_pinyin="";
		m_has_pending_pinyin=false;
	}
private:
	//PinyinKeyVector   m_keys;
	PinyinPhraseKey   m_key;
	bool append_first_syllable_chars(const char *pinyin,unsigned int pinyin_len);
	bool fallback_first_syllable_chars(const char *pinyin,unsigned int pinyin_len);

	//single hanzi
	PinyinTable m_table;
	CharVector  m_chars;
	String      m_table_filename;
	bool        m_initial_lookup;
	bool        m_partial_lookup;
	bool        m_mixed_candidates;
	unsigned int m_commit_pinyin_length;
	unsigned int m_mixed_char_commit_length;
	unsigned int m_phrase_candidate_count;
	String      m_display_pinyin;
	String      m_raw_pinyin;
	bool        m_has_pending_pinyin;
	
	//phrase
       	PinyinPhraseTable               m_phrases_table;
	PhraseOffsetFrequencyPairVector m_offset_freq_pairs;
	PhraseStringVector              m_phrases;
	String                          m_phrase_idx_filename;
};

#endif
/*
 * Revision history
 * 
 * $Log: PinyinEngine.h,v $
 * Revision 1.3  2004/07/20 11:26:05  Lu Mu
 * (1)phrase frequency
 * (2)self define phrase
 *
 * Revision 1.2  2004/07/17 07:10:45  Lu Mu
 * phrase support
 *
 * Revision 1.1  2004/07/10 15:02:23  Lu Mu
 * v0.0.1 released
 * TODO: phase support
 *
 *
 */
