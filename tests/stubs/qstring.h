#ifndef TEST_STUB_QSTRING_H
#define TEST_STUB_QSTRING_H

#include <stddef.h>
#include <string>
#include <vector>

class QChar
{
public:
	QChar() : m_code(0) {}
	QChar(unsigned short code) : m_code(code) {}
	unsigned short unicode() const { return m_code; }
private:
	unsigned short m_code;
};

class QString
{
public:
	QString() {}
	QString(const char *s)
	{
		if(!s)
			return;
		while(*s)
			m_chars.push_back(QChar((unsigned char)*s++));
	}

	unsigned int length() const { return m_chars.size(); }
	QChar operator[](unsigned int index) const { return m_chars[index]; }

	QString &operator+=(const QChar &ch)
	{
		m_chars.push_back(ch);
		return *this;
	}

	std::string ascii() const
	{
		std::string out;
		for(unsigned int i=0;i<m_chars.size();i++)
			out += static_cast<char>(m_chars[i].unicode() & 0xff);
		return out;
	}

private:
	std::vector<QChar> m_chars;
};

#endif
