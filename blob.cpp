// The "blob" class exists to give a place to store uninterpreted
// large data, and make copying "it" around efficient; like copying a
// pointer, but including lifetime management.

// "blob_data" acts as the container of the data and is
// reference-counted.  "blob" is a reference.  Methods attached to
// blob maintain a correct referene count in blob_data.  When the last
// instance of a blob is destroyed, the reference count in blob_data
// drops to zero and it's deleted.  By the way, that means all
// instances of blob_data must be created by a blob.

// Some useage of a blob is thread-safe.
// o You can assign blob to blob as long as the blob is "anchored",
//   that is, you're sure the refcount is > 0
// o you can inspect the blob contents
// You can only change a blob's contents if you know no other thread
// may be inspecting it.

// 'blob' doesn't provide for comparisons.  You'll have to gain access
// to the content and compare that yourself.  You can tell if two
// blobs refer to the same blob_data if their pvoid() pointers are the
// same (i.e. a.pvoid() == b.pvoid())

#include <iostream>
#include <iomanip>

#include <cstring>
#include <cstdlib>
#include <cassert>

#include "katomic.h"
#include "blob.h"

blob_data::blob_data() : m_refcount(1)
{
}

blob_data::~blob_data()
{
}

void blob_data::ref()
{
	katomic_inc(&m_refcount);
}

void blob_data::unref()
{
	if(katomic_dec_test(&m_refcount))
		delete this;
}

blob::blob()
{
	m_container = NULL;
}

blob::blob(const blob &from)
{
	if(from.m_container)
		from.m_container->ref(); // Note new reference first
	m_container = from.m_container;
}

blob::blob(const std::string &from)
{
	m_container = new blob_data;
	m_container->m_content.resize(from.size()+1);
	memcpy(&m_container->m_content[0], from.c_str(), from.size()+1);
}

blob::blob(const char *from)
{
	if(from == NULL)
		m_container = NULL;
	else
	{
		int len = strlen(from);
		m_container = new blob_data;
		m_container->m_content.resize(len + 1);
		memcpy(&m_container->m_content[0], from, len+1);
	}
}

blob::blob(const void *from, size_t len)
{
	m_container = new blob_data;
	m_container->m_content.resize(len);
	memcpy(&m_container->m_content[0], from, len);
}

blob::blob(const std::vector<char> &from)
{
	m_container = new blob_data;
	m_container->m_content.resize(from.size());
	memcpy(&m_container->m_content[0], &from[0], from.size());
}

blob::blob(const std::vector<unsigned char> &from)
{
	m_container = new blob_data;
	m_container->m_content.resize(from.size());
	memcpy(&m_container->m_content[0], &from[0], from.size());
}

blob::~blob()
{
	if(m_container)
		m_container->unref();
}

blob &blob::operator=(const blob &from)
{
	// Note: If this blob gets assigned from itself (I don't
	// expect this to happen but, whatever), unref-ing first would
	// cause the data to be deleted by this assignment.  So, ref()
	// the new first, then unref() the old.

	if(from.m_container)
		from.m_container->ref(); // Ref new
	if(m_container)
		m_container->unref();	// Unref current

	m_container = from.m_container;
	return *this;
}

static blob::blobvec_t empty_blob;

blob::const_blobvec_t &blob::get() const
{
	if(m_container == NULL)
		return empty_blob;

	return m_container->m_content;
}

blob::blobvec_t &blob::update()
{
	if(m_container == NULL)
		m_container = new blob_data;

	return m_container->m_content;
}

blob::blobvec_t::size_type blob::size() const
{
	if(m_container == NULL)
		return 0;				// No content.

	size_t s = m_container->m_content.size();
	return s;
}

std::string blob::str() const
{
	if(m_container == NULL)
		return std::string("");
	return std::string((const char *)&(m_container->m_content[0]), size());
}

const void *blob::pvoid() const
{
	if(m_container)
	{
		return (const void *)(&(m_container->m_content[0]));
	}
	return NULL;
}

const char *blob::c_str() const
{
	if(m_container)
	{
		return (const char *)(&(m_container->m_content[0]));
	}
	return NULL;
}

blob blob::clone()
{
	// Make another distinct blob with the same content as this one
	blob b;
	if(m_container)
	{
		blobvec_t &d = b.update();
		d.resize(size());
		memcpy(&d[0], &m_container->m_content[0], size());
	}
	return b;
}

bool blob::operator==(const blob &from) const
{
	const blobvec_t &left = this->get();
	const blobvec_t &right = from.get();
	if(left.size() != right.size())
		return false;
	if(left.size() == 0)
		return true;
	if(memcmp(&left[0], &right[0], left.size()) != 0)
		return false;
	return true;
}

bool blob::operator!=(const blob &from) const
{
	return ! (*this == from);
}

std::ostream &operator<<(std::ostream &out, const blob &b)
{
	blob::const_blobvec_t &data=b.get();
	int oldfill = out.fill('0');
	std::streamsize oldw = out.width();
	std::streamsize w = 0;
	const char *delim = "";
	std::ios_base::fmtflags oldflags = out.setf(std::ios_base::uppercase);
	for(int i = 0; i < data.size(); i++) {
		w += strlen(delim) + 2;
		out << delim <<
			std::setw(2) << std::setbase(16) << (int)(data[i] & 0xFF);
		delim = b.delim();
	}
	while(w < oldw) {
		out << "0";
		w++;
	}
	out.fill(oldfill);
	out.flags(oldflags);
	out.width(0);
	return out;
}

const char *blob::delim() const { return ""; }

void blob::clear()
{
	// Don't touch other users' copies
	if(m_container)
		m_container->unref();
	m_container = NULL;
}

const unsigned char &blob::operator[](size_t ix) const
{
	// Note: m_container must be set and blob.size() must be > ix
	return m_container->m_content[ix];
}

std::istream &operator>>(std::istream &in, blob &b)
{
	// Read pairs of chars as hex until a non-hex char
	blob xb;
	blob::blobvec_t &xxb = xb.update();
	int c, d;

	for(;;)
	{
		c = in.get();
		if(!in)
			break;
		if(!isxdigit(c))
		{
			in.putback(c);
			break;
		}
		c = toupper(c);
		c -= '0';
		if(c > 9) c -= 'A' - '9' - 1;
		int i = c << 4;
		d = in.get();
		if(!in) {
			in.putback(c);
			break;
		}

		if(!isxdigit(d)) {
			in.putback(d);
			break;
		}
		d = toupper(d);
		d -= '0';
		if(d > 9) d -= 'A' - '9' - 1;
		i |= d;

		xxb.push_back(i);
	}

	b = xb;
	return in;
}
