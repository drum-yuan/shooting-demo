#include "Buffer.h"
#include <string.h>

#define LWS_PRE 16  //reserve 16 bytes for libwebsockets write protocol header data

Buffer::Buffer(unsigned int size):m_buf(NULL),m_pos(LWS_PRE),m_size(0)
{
	m_size = LWS_PRE + size;
	m_buf = new unsigned char[m_size];
}

Buffer::~Buffer()
{
	if (m_buf)
	{
		delete (unsigned char*)m_buf;
		m_buf = NULL;
	}
}

void Buffer::reset()
{
	memset(m_buf, 0, m_pos);
	m_pos = LWS_PRE;
}

void* Buffer::getbuf()
{
	return (unsigned char*)m_buf + LWS_PRE;
}

unsigned int Buffer::getlength()
{
	return m_size;
}

unsigned int Buffer::getdatalength()
{
	return m_pos - LWS_PRE;
}

unsigned int Buffer::append(void* buf, unsigned int size)
{
	if (m_size < m_pos + size)
		return 0;

	memcpy((unsigned char*)m_buf + m_pos, buf, size);
	m_pos += size;

	return size;
}

void Buffer::resize(unsigned int size)
{
	if (LWS_PRE + size != m_size) {
		delete (unsigned char*)m_buf;
		m_size = LWS_PRE + size;
		m_buf = new unsigned char[m_size];
	}
	reset();
}
