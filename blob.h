#ifndef BLOB_H
#define BLOB_H

#include <iostream>
#include <string>
#include <vector>
#include "katomic.h"

class blob_data
{
	friend class blob;
protected:
	std::vector < unsigned char > m_content;
	katomic_t m_refcount;

	// Protected constructor, so only a blob can make a blob_data.
	blob_data();
	~blob_data();

	void ref();
	void unref();
};

// The way to create a new blob and populate it is to instantiate one,
// call "update()" to obtain a non-const reference to the base
// vector<unsigned char>, then populate that.  Or, clone() an existing blob.
// Oh, there are handy string constructors.

class blob
{
protected:
	blob_data *m_container;		// The blob this points to this data
public:
	typedef std::vector<unsigned char> blobvec_t;
	typedef std::vector<unsigned char> const_blobvec_t;
	blob();						// default constructor
	blob(const blob &from);		// Copy constructor

	// Other constructors:
	blob(const void *from, size_t len); // Arbitrary data
	blob(const std::string &from); // string constructor
	blob(const char *from);		   // C string (0-delim) constructor
	blob(const std::vector<char> &from); // from char vector
	blob(const std::vector<unsigned char> &from); // from unsigned char vector

	blob &operator=(const blob &from); // Assignment operator

	virtual ~blob();					// Destructor

	// A blob initialized from another blob is a reference to the same
	// data.  A blob "assigned" from another blob is a reference to
	// the same data.  If you want another distinct "copy" of the
	// data, you have to use clone().

	virtual blob clone();		// Create a distinct duplicate

	// Gaining access to the blob's contents:
	const_blobvec_t &get() const; // Access blob
								// content (Read-Only)

	blobvec_t &update(); // Access content to change it

	blobvec_t::size_type size() const;

	// a blob can be 'null' which is handy if you are (say) dealing
	// with SQL content.
	bool isnull() const
	{
		return m_container == NULL;
	}

	// Just saying if(blob) tells whenter it's null
	inline operator bool() { return m_container != NULL; }

	std::string str() const;	// Gives it's content as a string.
								// 'null' is represented by an empty
								// string: ""

	const void *pvoid() const;	// Gives it's content as a void*  'null'
								// is represented by NULL

	const char *c_str() const;	// Gives it's content as a C string
								// (only if it contains one!  blob
								// doesn't guarantee a 0 delim at the
								// end) 'null' is represented by NULL

	bool operator==(const blob &from) const; // check for equal content
	bool operator!=(const blob &from) const; // check for unequal content

	virtual const char *delim() const;

	void clear();

	virtual const unsigned char &operator[](size_t offset) const;
};

// output stream insertion
std::ostream &operator<<(std::ostream &out, const blob &b);

// input stream extraction
std::istream &operator>>(std::istream &out, blob &b);

#endif
