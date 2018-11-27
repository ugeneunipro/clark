#include "FILEex.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

#include <zlib.h>

#include "file.hh"

#include "../7zC/CpuArch.h"
#include "../7zC/7z.h"
#include "../7zC/7zAlloc.h"
#include "../7zC/7zBuf.h"
#include "../7zC/7zCrc.h"
#include "../7zC/7zFile.h"
#include "../7zC/7zVersion.h"


#include "RingBuffer.h"


using namespace std;

class PlainFile: public FILEex
{
public:
	PlainFile() : FILEex() {
		
	}
	bool open(const char * filename, const char * mode) { 
		file = fopen(filename, mode);
		return file != NULL; 
	}
	~PlainFile() { fclose(file); file = NULL; }
	int seek(long offset, int whence) { return fseek(file, offset, whence); }
	size_t read(void * ptr, size_t size, size_t count) { return fread(ptr, size, count, file); }
	bool read_line(string& line) { return getLineFromFile(file, line); }
private:
	FILE *file;
};


class GzipUtil;

class GZIPFile : public FILEex
{
public:
	GZIPFile();
	~GZIPFile();
	bool open(const char * filename, const char * mode);
	int seek(long offset, int whence);
	size_t read(void * ptr, size_t size, size_t count);
	bool read_line(string& line);

private:

	void reset();

	FILE *io; //IOAdapter* io;
	GzipUtil* z;
	RingBuffer* buf; // seek buffer
	int rewinded; // how much should read from seek buffer

};

const int BUFLEN = 32768;
const char* GZIP_URL = "gz:";
const char* GZIP_SUFFIX = ".gz";

/**
Based on 7z ANSI-C Decoder from http://www.7-zip.org/sdk.html 
*/
class ArchivedLZMAFile : public FILEex
{
public:
	ArchivedLZMAFile(); 
	~ArchivedLZMAFile();
	bool open(const char * filename, const char * mode);
	int seek(long offset, int whence);
	size_t read(void * ptr, size_t size, size_t count);
	bool read_line(string& line);
private:
	bool init(const char* aname, const char *entry);

private:

	ISzAlloc allocImp;
	ISzAlloc allocTempImp;

	CFileInStream archiveStream;
	CLookToRead2 lookStream;
	CSzArEx db;
	SRes res;

	UInt32 entry_idx;

	/*
	if you need cache, use these 3 variables.
	*/
	UInt32 blockIndex; /* it can have any value before first call (if outBuffer = 0) */
	Byte *outBuffer; /* it must be 0 before first call for each new archive. */
	size_t outBufferSize;  /* it can have any value before first call (if outBuffer = 0) */

	size_t offset;
	size_t outSizeProcessed;

	Byte *outBufferProcessed;
};


const char* LZMA_URL = "7z:";

static pair<string, FILEex *> cache;

int fclose(FILEex *& stream) { 
//	delete stream; 
	stream = NULL; 
	return 0; 
}

FILEex * fopenEx(const char * filename, const char * mode) {
	FILEex *f = cache.second;
	if (strcmp(cache.first.c_str(), filename) == 0) {
		return f;
	}
	else {
		cache.first.clear();
		cache.second = NULL;
		delete f;
		f = NULL;
	}
		
	if (strncmp(filename, GZIP_URL, strlen(GZIP_URL)) == 0) {
		filename = filename + strlen(GZIP_URL);
		f = new GZIPFile();
	} 
	else if (strncmp(filename, LZMA_URL, strlen(LZMA_URL)) == 0) {
		filename = filename + strlen(LZMA_URL);
		f = new ArchivedLZMAFile();
	} 
	else if (strcmp(filename + strlen(filename) - strlen(GZIP_SUFFIX), GZIP_SUFFIX) == 0) {
		f = new GZIPFile();
	} 
	else {
		f = new PlainFile();
	}
	if (f && f->open(filename, mode)) {
		cache = make_pair(filename, f);
		return f;
	}
	delete f;
	return NULL;
}

void getFirstAndSecondElementInLine(const string &line, string &_line, ITYPE &_freq) {
	size_t len = line.size();
	// Take first element and put it into _line
	// Take second element and put it into _freq
	vector<string> ele;
	getElementsFromLine(line, len, 2, ele);
	_line = ele[0];
	_freq = atoi(ele[1].c_str());
}

bool getFirstAndSecondElementInLine(FILEex* f, string& _line, ITYPE& _freq) {
	string tmp;
	if (f->read_line(tmp)) {
		getFirstAndSecondElementInLine(tmp, _line, _freq);
		return true;
	}
	return false;
}

typedef int64_t qint64;

class GzipUtil {
public:
	GzipUtil(FILE* io);
	~GzipUtil();
	qint64 uncompress(char* outBuff, qint64 outSize);
	qint64 compress(const char* inBuff, qint64 inSize, bool finish = false);
	bool isCompressing() const { return doCompression; }
	qint64 getPos() const;
private:
	static const int CHUNK = 16384;
	z_stream strm;
	char buf[CHUNK];
	FILE* io;
	bool doCompression;
	qint64 curPos; // position of uncompressed file
};

GzipUtil::GzipUtil(FILE* io) : io(io), curPos(0)
{
	//#ifdef _DEBUG
	memset(buf, 0xDD, CHUNK);
	//#endif

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

	int ret = 
		/* enable zlib and gzip decoding with automatic header detection */
		inflateInit2(&strm, 32 + 15);
	assert(ret == Z_OK);
}

GzipUtil::~GzipUtil()
{
		inflateEnd(&strm);
}

qint64 GzipUtil::getPos() const {
	return curPos;
}

qint64 GzipUtil::uncompress(char* outBuff, qint64 outSize)
{
	/* Based on gun.c (example from zlib, copyrighted (C) 2003, 2005 Mark Adler) */
	strm.avail_out = outSize;
	strm.next_out = (Bytef*)outBuff;
	do {
		/* run inflate() on input until output buffer is full */
		if (strm.avail_in == 0) {
			// need more input
			strm.avail_in = fread(buf, 1, CHUNK, io);
			strm.next_in = (Bytef*)buf;
		}
		if (strm.avail_in == uint32_t(-1)) {
			// TODO log error
			return -1;
		}
		if (strm.avail_in == 0)
			break;

		int ret = inflate(&strm, Z_SYNC_FLUSH);
		assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
		switch (ret) {
		case Z_NEED_DICT:
		case Z_DATA_ERROR:
		case Z_MEM_ERROR:
			return -1;
		case Z_STREAM_END:
		{
			qint64 readBytes = 0;
			readBytes = outSize - strm.avail_out;
			inflateReset(&strm);
			inflateInit2(&strm, 32 + 15);

			return readBytes;
		}
		case Z_BUF_ERROR:
		case Z_FINISH:
			curPos += outSize - strm.avail_out;
			return outSize - strm.avail_out;
		}
		if (strm.avail_out != 0 && strm.avail_in != 0) {
			assert(0);
			break;
		}
	} while (strm.avail_out != 0);
	curPos += outSize - strm.avail_out;

	return outSize - strm.avail_out;
}

GZIPFile::GZIPFile() 
	: FILEex(), io(NULL), z(NULL), buf(NULL), rewinded(0) {}

GZIPFile::~GZIPFile() {
	delete z;
	z = NULL;
	if (buf) {
		delete[] buf->rawData();
		delete buf;
		buf = NULL;
	}
	if (io) fclose(io);
	io = NULL;
}

bool GZIPFile::open(const char * filename, const char * mode) {
	if (strcmp(mode, "r") == 0) {
		io = fopen(filename, mode);
		if (io) {
			z = new GzipUtil(io);
			buf = new RingBuffer(new char[BUFLEN], BUFLEN);
			return true;
		}
		perror(NULL);
		return false;
	}
	else {
		cerr << "Unsupported file open mode for gzip archive: " << mode << "\n";
		return false;
	}
}

void GZIPFile::reset() {
	char *bf = buf->rawData();
	delete buf;
	delete z;
	z = new GzipUtil(io);
	buf = new RingBuffer(bf, BUFLEN);
	rewinded = 0;
	fseek(io, 0, SEEK_SET);
}

int GZIPFile::seek(long inFileOffset, int whence) {
	if (whence == SEEK_SET) {
		long relative = inFileOffset - z->getPos() - rewinded;
		if (relative <= 0) {
			if (-relative <= buf->length()) {
				rewinded = -relative;
				return true;
			}
			// else fallback via reset
		}
		return seek(inFileOffset - z->getPos(), SEEK_CUR);
	}
	else if (whence == SEEK_CUR) {
		qint64 nBytes = inFileOffset; 
		nBytes -= rewinded;
		if (nBytes <= 0) {
			if (-nBytes <= buf->length()) {
				rewinded = -nBytes;
				return true;
			}
			return false;
		}
		rewinded = 0;
		char* tmp = new char[nBytes];
		qint64 skipped = read(tmp, 1, nBytes);
		delete[] tmp;

		return skipped == nBytes;
	}
	else {
		fprintf(stderr, "SEEK_END in gzip archive is not supported yet!!!");
		exit(-1);
		return -1;
	}

	reset();
	return seek(inFileOffset, whence);
}

size_t GZIPFile::read(void * ptr, size_t size, size_t count) {
	char* data = (char*)ptr;
	size = count * size;
	// first use data put back to buffer if any
	int cached = 0;
	if (rewinded != 0) {
		assert(rewinded > 0 && rewinded <= buf->length());
		cached = buf->read(data, size, buf->length() - rewinded);
		if (cached == size) {
			rewinded -= size;
			return size;
		}
		assert(cached < size);
		rewinded = 0;
	}
	size = z->uncompress(data + cached, size - cached);
	if (size == -1) {
		return -1;
	}
	buf->append(data + cached, size);

	return size + cached;
}

#define TMP_SZ 1024

bool GZIPFile::read_line(string& line) {

	char delims[] = "\n\r";
	line.clear();
	char tmp[TMP_SZ + 1];
	char* mt = NULL;

	while (true) {
		int l = read(tmp, 1, TMP_SZ);
		if (l <= 0) {
			break;
		}
		tmp[l] = '\0';

		char *sep = strtok_safe(tmp, delims, &mt);
		if (NULL == sep) {
			continue;
		}
		if (strlen(sep) < l)
		{
			line.append(sep);
			seek(strlen(sep) + 1 - l, SEEK_CUR);
			return true;
		}
		else {
			line.append(tmp, l);
			if (l < TMP_SZ) {
				break;
			}
		}
	}

	return line.length() != 0;
}


ArchivedLZMAFile::ArchivedLZMAFile()
	: FILEex(),
	  res(0),
	  entry_idx(numeric_limits<uint32_t>::max()),
	  blockIndex(0xFFFFFFFF),
	  outBuffer(NULL),
	  outBufferSize(0),
	  offset(0),
	  outSizeProcessed(0),
	  outBufferProcessed(NULL)
{
	memset(&allocImp, 0, sizeof(ISzAlloc));
	memset(&allocTempImp, 0, sizeof(ISzAlloc));
	memset(&archiveStream, 0, sizeof(CFileInStream));
	memset(&lookStream, 0, sizeof(CLookToRead2));
	memset(&db, 0, sizeof(CSzArEx));
}

ArchivedLZMAFile::~ArchivedLZMAFile() { 
	ISzAlloc_Free(&allocImp, outBuffer);
	SzArEx_Free(&db, &allocImp);
	ISzAlloc_Free(&allocImp, lookStream.buf);
	File_Close(&archiveStream.file);
}

bool ArchivedLZMAFile::open(const char * filename, const char * mode) {
	if (strcmp(mode, "r") == 0) {
		const char* sep = strchr(filename, '!');
		if (sep && sep[1] == '/') {
			string aname(filename, sep - filename);
			return init(aname.c_str(), sep + 2);
		}
		else {
			cerr << "Malformed archive entry url: " << filename << "\n";
			return false;
		}
	}
	else {
		cerr << "Unsupported file open mode for 7z archive: " << mode << "\n";
		return false;
	}
}


#define kInputBufSize ((size_t)1 << 18)

static const ISzAlloc g_Alloc = { SzAlloc, SzFree };

static int Buf_EnsureSize(CBuf *dest, size_t size)
{
	if (dest->size >= size)
		return 1;
	Buf_Free(dest, &g_Alloc);
	return Buf_Create(dest, size, &g_Alloc);
}

static void UInt64ToStr(UInt64 value, char *s, int numDigits)
{
	char temp[32];
	int pos = 0;
	do
	{
		temp[pos++] = (char)('0' + (unsigned)(value % 10));
		value /= 10;
	} while (value != 0);

	for (numDigits -= pos; numDigits > 0; numDigits--)
		*s++ = ' ';

	do
		*s++ = temp[--pos];
	while (pos);
	*s = '\0';
}
#ifndef _WIN32
#define _USE_UTF8
#endif

#ifdef _WIN32
static UINT g_FileCodePage = CP_ACP;
#endif


/* #define _USE_UTF8 */

#ifdef _USE_UTF8

#define _UTF8_START(n) (0x100 - (1 << (7 - (n))))

#define _UTF8_RANGE(n) (((UInt32)1) << ((n) * 5 + 6))

#define _UTF8_HEAD(n, val) ((Byte)(_UTF8_START(n) + (val >> (6 * (n)))))
#define _UTF8_CHAR(n, val) ((Byte)(0x80 + (((val) >> (6 * (n))) & 0x3F)))

static size_t Utf16_To_Utf8_Calc(const UInt16 *src, const UInt16 *srcLim)
{
	size_t size = 0;
	for (;;)
	{
		UInt32 val;
		if (src == srcLim)
			return size;

		size++;
		val = *src++;

		if (val < 0x80)
			continue;

		if (val < _UTF8_RANGE(1))
		{
			size++;
			continue;
		}

		if (val >= 0xD800 && val < 0xDC00 && src != srcLim)
		{
			UInt32 c2 = *src;
			if (c2 >= 0xDC00 && c2 < 0xE000)
			{
				src++;
				size += 3;
				continue;
			}
		}

		size += 2;
	}
}

static Byte *Utf16_To_Utf8(Byte *dest, const UInt16 *src, const UInt16 *srcLim)
{
	for (;;)
	{
		UInt32 val;
		if (src == srcLim)
			return dest;

		val = *src++;

		if (val < 0x80)
		{
			*dest++ = (char)val;
			continue;
		}

		if (val < _UTF8_RANGE(1))
		{
			dest[0] = _UTF8_HEAD(1, val);
			dest[1] = _UTF8_CHAR(0, val);
			dest += 2;
			continue;
		}

		if (val >= 0xD800 && val < 0xDC00 && src != srcLim)
		{
			UInt32 c2 = *src;
			if (c2 >= 0xDC00 && c2 < 0xE000)
			{
				src++;
				val = (((val - 0xD800) << 10) | (c2 - 0xDC00)) + 0x10000;
				dest[0] = _UTF8_HEAD(3, val);
				dest[1] = _UTF8_CHAR(2, val);
				dest[2] = _UTF8_CHAR(1, val);
				dest[3] = _UTF8_CHAR(0, val);
				dest += 4;
				continue;
			}
		}

		dest[0] = _UTF8_HEAD(2, val);
		dest[1] = _UTF8_CHAR(1, val);
		dest[2] = _UTF8_CHAR(0, val);
		dest += 3;
	}
}

static SRes Utf16_To_Utf8Buf(CBuf *dest, const UInt16 *src, size_t srcLen)
{
	size_t destLen = Utf16_To_Utf8_Calc(src, src + srcLen);
	destLen += 1;
	if (!Buf_EnsureSize(dest, destLen))
		return SZ_ERROR_MEM;
	*Utf16_To_Utf8(dest->data, src, src + srcLen) = 0;
	return SZ_OK;
}

#endif

static SRes Utf16_To_Char(CBuf *buf, const UInt16 *s
#ifndef _USE_UTF8
	, UINT codePage = g_FileCodePage
#endif
	)
{
	unsigned len = 0;
	for (len = 0; s[len] != 0; len++);

#ifndef _USE_UTF8
	{
		unsigned size = len * 3 + 100;
		if (!Buf_EnsureSize(buf, size))
			return SZ_ERROR_MEM;
		{
			buf->data[0] = 0;
			if (len != 0)
			{
				char defaultChar = '_';
				BOOL defUsed;
				unsigned numChars = 0;
				numChars = WideCharToMultiByte(codePage, 0, (LPCWCH)s, len, (char *)buf->data, size, &defaultChar, &defUsed);
				if (numChars == 0 || numChars >= size)
					return SZ_ERROR_FAIL;
				buf->data[numChars] = 0;
			}
			return SZ_OK;
		}
	}
#else
	return Utf16_To_Utf8Buf(buf, s, len);
#endif
}

int ArchivedLZMAFile::seek(long inFileOffset, int whence) {
	if (whence != SEEK_SET) {
		fprintf(stderr, "seeking 7z archive is not supported yet!!!");
		exit(-1);
		return -1;
	}

	if (outBufferProcessed == outBuffer + offset + inFileOffset) {
		return 0;
	}

	if (outBufferProcessed && outSizeProcessed) {
		const Byte *newOutBufferProcessed = outBuffer + offset + inFileOffset;
		const size_t delta = outBufferProcessed - newOutBufferProcessed;
		outBufferProcessed -= delta;
		outSizeProcessed += delta;
	} else {
		res = SzArEx_Extract(&db, &lookStream.vt, entry_idx,
							 &blockIndex, &outBuffer, &outBufferSize,
							 &offset, &outSizeProcessed,
							 &allocImp, &allocTempImp);

		outBufferProcessed = outBuffer + offset + inFileOffset;
		outSizeProcessed -= inFileOffset;
	}

	return 0;
}

size_t ArchivedLZMAFile::read(void * ptr, size_t size, size_t count) {

	size_t el = size;
	size = count * size;
	Byte *dest = (Byte*)ptr;
	while (res == SZ_OK) {
		if (outBufferProcessed && outSizeProcessed) {
			// use leftover from prev decompression first
			if (outSizeProcessed >= size) {
				memcpy(dest, outBufferProcessed, size);
				outSizeProcessed -= size;
				outBufferProcessed += size;
				return count;
			}
			else {
				memcpy(dest, outBufferProcessed, outSizeProcessed);
				dest += outSizeProcessed;
				size -= outSizeProcessed;
				outBufferProcessed = 0;
				return outSizeProcessed / size;
			}
		}
		if (outBufferProcessed) {
			return 0; // EOF ???
		}
		res = SzArEx_Extract(&db, &lookStream.vt, entry_idx,
			&blockIndex, &outBuffer, &outBufferSize,
			&offset, &outSizeProcessed,
			&allocImp, &allocTempImp);
		outBufferProcessed = outBuffer + offset;
		if (outSizeProcessed == 0) {
			//end of archive?
			break;
		}
	}
	return 0;
}


bool ArchivedLZMAFile::read_line(string& line) {

	char delims[] = "\n\r";
	char *sep = NULL, *mt = NULL;
	Byte terminator = '\0';
	line.clear();

	while (res == SZ_OK) {
		if (outBufferProcessed && outSizeProcessed) {
			// use leftover from prev decompression first

			// ensure proper line end
			Byte* the_end = outBufferProcessed + outSizeProcessed;
			if (outBuffer + outBufferSize > the_end) {
				*the_end = '\0';
			}
			else if (*(--the_end) != '\0') {
				terminator = *the_end;
				*the_end = '\0';
			}
			// look for line sep
			sep = strtok_safe((char*)outBufferProcessed, delims, &mt);
			if (sep != NULL) //FIXME check len instead
			{
				line.append(sep);
				outBufferProcessed += line.length() + 1;
				outSizeProcessed -= line.length() + 1;
				if (terminator != '\0') {
					*the_end = terminator;
					if (outSizeProcessed == 1) {
						line.append(1, (char)terminator);
						outSizeProcessed = 0;
					}
				}
				return true;
			}
			else {
				if (terminator != '\0') {
					*the_end = terminator;
					if (outSizeProcessed == 1) {
						line.append(1, (char)terminator);
						outSizeProcessed = 0;
						return true;
					}
				}
				// no more lines
				outSizeProcessed = 0;
			}
		}
		if (outBufferProcessed) {
			return 0; // EOF ???
		}
		res = SzArEx_Extract(&db, &lookStream.vt, entry_idx,
			&blockIndex, &outBuffer, &outBufferSize,
			&offset, &outSizeProcessed,
			&allocImp, &allocTempImp);
		outBufferProcessed = outBuffer + offset;
		if (outSizeProcessed == 0) {
			//end of archive?
			break;
		}
	}

	return false;
}


static int strcmp16(const char *p, const UInt16 *name)
{
	CBuf buf;
	int res = 1;
	Buf_Init(&buf);
	if (Utf16_To_Char(&buf, name) == SZ_OK) {
		//fprintf(stderr, "Found entry: %s", (const char *)buf.data);
		res = strcmp(p, (const char *)buf.data);
		Buf_Free(&buf, &g_Alloc);
	}
	return res;
}

bool ArchivedLZMAFile::init(const char* fname, const char* entry) {
	allocImp = g_Alloc;
	allocTempImp = g_Alloc;

	if (InFile_Open(&archiveStream.file, fname))
	{
		fprintf(stderr, "can not open 7z file: %s\n", fname);
		return false;
	}

	FileInStream_CreateVTable(&archiveStream);
	LookToRead2_CreateVTable(&lookStream, False);
	lookStream.buf = NULL;

	res = SZ_OK;

	{
		lookStream.buf = (Byte*)ISzAlloc_Alloc(&allocImp, kInputBufSize);
		if (!lookStream.buf)
			res = SZ_ERROR_MEM;
		else
		{
			lookStream.bufSize = kInputBufSize;
			lookStream.realStream = &archiveStream.vt;
			LookToRead2_Init(&lookStream);
		}
	}

	CrcGenerateTable();

	SzArEx_Init(&db);

	if (res == SZ_OK)
	{
		UInt16 *temp = NULL;
		size_t tempSize = 0;

		res = SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp);


		 for (UInt32 i = 0; i < db.NumFiles; i++)
		{
			// const CSzFileItem *f = db.Files + i;
			size_t len;
			unsigned isDir = SzArEx_IsDir(&db, i);
			if (isDir)
				continue;
			len = SzArEx_GetFileNameUtf16(&db, i, NULL);
			// len = SzArEx_GetFullNameLen(&db, i);

			if (len > tempSize)
			{
				SzFree(NULL, temp);
				tempSize = len;
				temp = (UInt16 *)SzAlloc(NULL, tempSize * sizeof(temp[0]));
				if (!temp)
				{
					res = SZ_ERROR_MEM;
					break;
				}
			}

			SzArEx_GetFileNameUtf16(&db, i, temp);
			if (0 == strcmp16(entry, temp)) {
				entry_idx = i;
				break;
			}

			/*
			if (SzArEx_GetFullNameUtf16_Back(&db, i, temp + len) != temp)
			{
			res = SZ_ERROR_FAIL;
			break;
			}
			*/
		}
		 SzFree(NULL, temp);
	}

	if (res == SZ_OK)
	{
		if (entry_idx == numeric_limits<uint32_t>::max()) {
			fprintf(stderr, "Bad target configuration, 7z archive entry not found: %s", entry);
			res = SZ_ERROR_DATA;
		}
		else {
			return true;
		}
	}

	if (res == SZ_ERROR_UNSUPPORTED)
		fprintf(stderr, "decoder doesn't support this archive\n");
	else if (res == SZ_ERROR_MEM)
		fprintf(stderr, "can not allocate memory\n");
	else if (res == SZ_ERROR_CRC)
		fprintf(stderr, "CRC error\n");
	else
	{
		char s[32];
		UInt64ToStr(res, s, 0);
		fprintf(stderr, "Error decoding 7z archive: %s\n", s);
	}
	return false;
}
