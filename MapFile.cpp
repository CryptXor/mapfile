#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <DbgHelp.h>
#include <string>
#include <map>
#include <vector>
#include <algorithm>

#include "inparser.h"
#include "sutil.h"

#pragma comment(lib,"DbgHelp.lib")

#pragma warning(disable:4996)

enum MapFileState
{
	MFS_BEGIN,
	MFS_TIME_STAMP,
	MFS_LOAD_ADDRESS,
	MFS_START,
	MFS_SECTIONS,
	MFS_ADDRESS,
	MFS_ENTRY_POINT,
	MFS_STATIC_SYMBOLS
};

struct Section
{
	Section(const char *start,const char *length,const char *name,const char *className)
	{
		mLength = NVSHARE::GetHEX(length,0);
		mName = name;
		mClassName = className;
		const char *next;
		mSection = NVSHARE::GetHEX(start,&next);
		if ( next )
		{
			next++;
			mAddress = NVSHARE::GetHEX(next,0);
		}
		else
		{
			mAddress = 0;
		}
	}

	bool operator < (const Section &a) const
	{
		return a.mLength < mLength;
	}

	void report(unsigned int &total,unsigned int &total_code,unsigned int &total_data)
	{
		printf("%-25s : %-10s : Length: %10s  Section: %04X Address: %08X\n", mName, mClassName, NVSHARE::formatNumber((int)mLength), mSection, mAddress );
		total+=mLength;
		if ( strcmp(mClassName,"DATA") == 0 )
			total_data+= mLength;
		else
			total_code+=mLength;
	}

	unsigned int	mLength;
	unsigned int    mSection;
	unsigned int	mAddress;
	const char		*mName;
	const char		*mClassName;
};

typedef std::vector< Section > SectionVector;

class MapFile : public NVSHARE::InPlaceParserInterface
{
public:
	MapFile(const char *fname)
	{
		mLastSection = 0;
		mLastAddress = 0;

		NVSHARE::InPlaceParser ipp(fname);
		mState = MFS_BEGIN;
		ipp.Parse(this);
		std::sort( mSections.begin(), mSections.end() );
		printf("Map File Contains %d sections\n", mSections.size() );
		printf("=====================================\n");
		unsigned int total=0;
		unsigned int total_code=0;
		unsigned int total_data=0;
		for (SectionVector::iterator i=mSections.begin(); i!=mSections.end(); ++i)
		{
			(*i).report(total,total_code,total_data);
		}
		printf("=====================================\n");
		printf("Total Code:      %12s\n", NVSHARE::formatNumber(total_code));
		printf("Total Data:      %12s\n", NVSHARE::formatNumber(total_data));
		printf("Total Code+Data: %12s\n", NVSHARE::formatNumber(total));

	}

  	virtual int ParseLine(int lineno,int argc,const char **argv)   // return TRUE to continue parsing, return FALSE to abort parsing process
	{
		switch ( mState )
		{
			case MFS_BEGIN:
				mExeName = argv[0];
				printf("Executable name is: %s\n", mExeName.c_str() );
				mState = MFS_TIME_STAMP;
				break;
			case MFS_SECTIONS:
				if ( argc == 4 )
				{
					const char *start = argv[0];
					const char *length = argv[1];
					const char *name   = argv[2];
					const char *className = argv[3];
					Section section(start,length,name,className);
					mSections.push_back(section);

				}
				else
				{
					printf("Unexpected number of arguments found in the sections area at line %d\n", lineno);
				}
				break;
			case MFS_ADDRESS:
				if ( argc >= 4 )
				{
					const char *address = argv[0];
					const char *next;
					unsigned int section = NVSHARE::GetHEX(address,&next);
					unsigned int adr = 0;
					unsigned int len = 0;
					if ( next )
					{
						adr     = NVSHARE::GetHEX(next+1,0);
					}
					if ( section != mLastSection )
					{
						mLastAddress = 0;
					}
					else
					{
						len = adr - mLastAddress;
					}

					mLastAddress = adr;
					mLastSection = section;

					char scratch[1024];
					const char *name = argv[1];
					if ( name[0] == '?' )
					{
						UnDecorateSymbolName( name, scratch, 1024, 0);
						name = scratch;
					}

					const char *objName = argv[3];

					printf("%12s : %-50s : %s\n", NVSHARE::formatNumber(len), objName, name );
				}
				break;
			case MFS_STATIC_SYMBOLS:
#if 0
				{
					static int lastArgc = 0;
					if ( argc != lastArgc )
					{
						printf("Static Symbol: %d args\n", argc );
						for (int i=0; i<argc; i++)
						{
							printf("%d='%s'\n", i+1, argv[i] );
						}
						printf("\n");
						lastArgc = argc;
					}
				}
#endif
				break;
		}
		return 0;
	}

	virtual bool preParseLine(int lineno,const char * line)
    {
		bool ret = false;

		if ( mState == MFS_TIME_STAMP )
		{
			const char *str = strstr(line,"Timestamp");
			if ( str )
			{
				const char *paren = strstr(line,"(");
				if ( paren )
				{
					mTimeStamp = paren;
					mState = MFS_LOAD_ADDRESS;
					ret = true;
					printf("TimeStamp: %s\n", mTimeStamp.c_str() );
				}
				else
				{
					printf("WARNING: Missing an open parenthesis in %s at line %d\n", line, lineno );
				}
			}
		}
		else if ( mState == MFS_LOAD_ADDRESS )
		{
			const char *str = strstr(line,"Preferred load address");
			if ( str )
			{
				printf("Encountered preferred load address at line %d\n", lineno );
				mState = MFS_START;
				ret = true;
			}
			else
			{
				printf("WARNING: expected to find 'Preferred load address at line %d, not '%s'\n", lineno, line );
			}
		}
		else if ( mState == MFS_START )
		{
			const char *str = strstr(line,"Start");
			if ( str )
			{
				printf("Encountered Start address section at line %d.\n", lineno );
				mState = MFS_SECTIONS;
				ret = true;
			}
			else
			{
				printf("WARNING: Expected to encounter that 'Start' sections line.  Instead hit '%s' at line %d\n", line, lineno );
			}
		}
		else if ( mState == MFS_SECTIONS )
		{
			const char * str = strstr(line,"Address");
			if ( str )
			{
				printf("Encountered the addresses section at line %d\n", lineno );
				mState = MFS_ADDRESS;
				ret = true;
			}
		}
		else if ( mState == MFS_ADDRESS )
		{
			const char * str = strstr(line,"entry point at");
			if ( str )
			{
				printf("Encountered entry point at line %d\n", lineno);
				mState = MFS_ENTRY_POINT;
				ret = true;
			}
		}
		else if ( mState == MFS_ENTRY_POINT )
		{
			const char *str = strstr(line,"Static symbols");
			if ( str )
			{
				printf("Encountered static symbols marker at line %d\n", lineno);
				mState = MFS_STATIC_SYMBOLS;
				ret = true;
			}
			else
			{
				printf("WARNING: Encountered unexpected data '%s' at line '%d' was expecting the 'Static symbols' marker.\n", line, lineno );
			}
		}
        return ret;
	}; // optional chance to pre-parse the line as raw data.  If you return 'true' the line will be skipped assuming you snarfed it.

	MapFileState	mState;
	std::string	mExeName;
	std::string mTimeStamp;
	SectionVector	mSections;
	unsigned int	mLastSection;
	unsigned int	mLastAddress;
};

void main(int argc,const char **argv)
{
	if ( argc == 2 )
	{
		MapFile mf(argv[1]);
	}
	else
	{
		printf("Usage: MapFile <mapfileName>\r\n");
	}
}
