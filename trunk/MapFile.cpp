#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <DbgHelp.h>
#include <string>
#include <map>
#include <hash_map>
#include <vector>
#include <algorithm>

#include "inparser.h"
#include "sutil.h"
#include "htmltable.h"

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

class BasicAddress
{
public:
	unsigned int mAddress;
	unsigned int mSection;
	std::string  mFunctionName;
	std::string  mObjectName;
	unsigned int mLength;
};

typedef std::vector< BasicAddress > BasicAddressVector;

typedef std::vector< std::string > StringVector;

struct ByType
{

	void addFunction(const std::string &str)
	{
		bool found = false;
		for (StringVector::iterator i=mFunctions.begin(); i!=mFunctions.end(); ++i)
		{
			if ( (*i) == str )
			{
				found = true;
				break;
			}
		}
		if ( !found )
		{
			mFunctions.push_back(str);
		}
	}

	void getFunctionNames(std::string &names,unsigned int maxLen)
	{
		for (StringVector::iterator i=mFunctions.begin(); i!=mFunctions.end(); ++i)
		{
			names+=(*i);
			if ( (i+1) != mFunctions.end() )
			{
			  names+=",";
			}
			if ( names.size() >= maxLen )
				break;
		}
	}

	std::string	 	mName;
	unsigned int 	mLength;
	unsigned int 	mCount;
	StringVector	mFunctions;
};

typedef stdext::hash_map< std::string, ByType > ByTypeMap;


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

	bool addBasicAddress(BasicAddress &ba)
	{
		bool ret = false;
		assert( ba.mSection == mSection );
		if ( ba.mAddress >= mAddress && ba.mAddress < (mAddress+mLength) )
		{
			if ( !mAddresses.empty() )
			{
				size_t index = mAddresses.size()-1;
				mAddresses[index].mLength = ba.mAddress - mAddresses[index].mAddress;
			}
			ba.mLength = (mAddress+mLength)-ba.mAddress; // distance from the end of this section is default length
			mAddresses.push_back(ba);

			ret = true;
		}
		return ret;
	}

	void report(NVSHARE::HtmlTable *table)
	{
		unsigned int total = 0;
   		for (BasicAddressVector::iterator i=mAddresses.begin(); i!=mAddresses.end(); i++)
   		{
   			total+=(*i).mLength;
   		}
		table->addColumn(mLength);
		table->addColumn(mName);
		table->addColumn(mClassName);
		table->addColumn(mAddresses.size());
		table->addColumn(total);
		table->nextRow();
	}

	void reportDetails(unsigned int section,NVSHARE::HtmlDocument *document)
	{
#if 0
		{
    		ByTypeMap byFunction;

    		for (BasicAddressVector::iterator i=mAddresses.begin(); i!=mAddresses.end(); i++)
    		{
    			ByTypeMap::iterator found = byFunction.find( (*i).mFunctionName );
    			if ( found == byFunction.end() )
    			{
    				ByType bt;
    				bt.mName = (*i).mObjectName;
    				bt.mLength = (*i).mLength;
    				bt.mCount = 1;
    				byFunction[ (*i).mFunctionName ] = bt;
    			}
    			else
    			{
    				(*found).second.mLength+=(*i).mLength;
    				(*found).second.mCount++;
    			}
    		}

    		char scratch[2048];
    		sprintf(scratch,"Details for section %d subsection: %s class name: %s with %d FUNCTIONS total size of %s", section, mName, mClassName, byFunction.size(), NVSHARE::formatNumber(mLength) );
    		NVSHARE::HtmlTable *table = document->createHtmlTable(scratch);
    		table->addHeader("Size,Count,Function Name,Object Name");
    		table->addSort("Sorted by size",1,false,0,false);
    		for (ByTypeMap::iterator i=byFunction.begin(); i!=byFunction.end(); ++i)
    		{
    			ByType &bt = (*i).second;
    			table->addColumn( bt.mLength );
    			table->addColumn( bt.mCount );
    			table->addColumn( (*i).first.c_str() );
    			table->addColumn( bt.mName.c_str() );
    			table->nextRow();
    		}
    		table->computeTotals();

    	}
#endif

		{
    		ByTypeMap byObject;

    		for (BasicAddressVector::iterator i=mAddresses.begin(); i!=mAddresses.end(); i++)
    		{
    			ByTypeMap::iterator found = byObject.find( (*i).mObjectName );
    			if ( found == byObject.end() )
    			{
    				ByType bt;
    				bt.mName = (*i).mFunctionName;
    				bt.mLength = (*i).mLength;
    				bt.mCount = 1;
    				bt.addFunction( (*i).mFunctionName );
    				byObject[ (*i).mObjectName ] = bt;
    			}
    			else
    			{
    				(*found).second.mLength+=(*i).mLength;
    				(*found).second.mCount++;
    				(*found).second.addFunction( (*i).mFunctionName );
    			}
    		}

    		char scratch[2048];
    		sprintf(scratch,"Details for section %d subsection: %s class name: %s with %d OBJECTS total size of %s", section, mName, mClassName, byObject.size(), NVSHARE::formatNumber(mLength) );
    		NVSHARE::HtmlTable *table = document->createHtmlTable(scratch);
    		table->addHeader("Size,Count,Object Name,Function Count");
    		table->addSort("Sorted by size",1,false,0,false);
    		for (ByTypeMap::iterator i=byObject.begin(); i!=byObject.end(); ++i)
    		{
    			ByType &bt = (*i).second;
    			table->addColumn( bt.mLength );
    			table->addColumn( bt.mCount );
    			table->addColumn( (*i).first.c_str() );
				table->addColumn( bt.mFunctions.size() );
    			table->nextRow();
    		}
    		table->computeTotals();

    	}


	}

	void getTotals(unsigned int &total,unsigned int &total_count)
	{
		total_count+=mAddresses.size();
   		for (BasicAddressVector::iterator i=mAddresses.begin(); i!=mAddresses.end(); i++)
   		{
   			total+=(*i).mLength;
   		}
	}

	unsigned int		mLength;
	unsigned int    	mSection;
	unsigned int		mAddress;
	const char			*mName;
	const char			*mClassName;
	BasicAddressVector 	mAddresses;
};

typedef std::vector< Section > SectionVector;

struct SectionBase
{
	SectionBase(void)
	{
		mTotalLength = 0;
		mTotalCode = 0;
		mTotalData = 0;
	}

	void addSection(const Section &s)
	{
		if ( strcmp(s.mClassName,"DATA") == 0 )
			mTotalData+=s.mLength;
		else
			mTotalCode+=s.mLength;
		mTotalLength+=s.mLength;
		mSections.push_back(s);
	}

	void addBasicAddress(BasicAddress &ba)
	{
		bool found = false;
		for (SectionVector::iterator i=mSections.begin(); i!=mSections.end(); ++i)
		{
			if ( (*i).addBasicAddress(ba) )
			{
				found = true;
				break;
			}
		}
		found = true;
	}

	void reportSections(unsigned int section,NVSHARE::HtmlTable *table)
	{
		unsigned int total=0;
		unsigned int total_count=0;
		for (SectionVector::iterator i=mSections.begin(); i!=mSections.end(); ++i)
		{
			(*i).getTotals(total,total_count);
		}

		table->addColumn(section);
		table->addColumn(mTotalLength);
		table->addColumn(mTotalCode);
		table->addColumn(mTotalData);
		table->addColumn(mSections.size());
		table->addColumn(total_count);
		table->addColumn(total);

		table->nextRow();
	}

	void reportDetails(unsigned int section,NVSHARE::HtmlDocument *document)
	{
		char scratch[2048];
		sprintf(scratch,"Details for section %d with %d subsections.", section, mSections.size() );
		NVSHARE::HtmlTable *table = document->createHtmlTable(scratch);
		table->addHeader("Size,Sub-Section Name,Class Name,Address Count,Address Size");
		table->addSort("Sorted by subsection size", 1, false, 0, false );
		for (SectionVector::iterator i=mSections.begin(); i!=mSections.end(); i++)
		{
			(*i).report(table);
		}
		table->computeTotals();
		for (SectionVector::iterator i=mSections.begin(); i!=mSections.end(); i++)
		{
			(*i).reportDetails(section,document);
		}
	}

	unsigned int    mTotalLength;
	unsigned int    mTotalCode;
	unsigned int    mTotalData;
	SectionVector	mSections;
};

typedef stdext::hash_map< unsigned int, SectionBase > SectionBaseMap;

class MapFile : public NVSHARE::InPlaceParserInterface
{
public:
	MapFile(const char *fname)
	{


		NVSHARE::InPlaceParser ipp(fname);
		mState = MFS_BEGIN;
		ipp.Parse(this);

		NVSHARE::HtmlTableInterface *html = NVSHARE::getHtmlTableInterface();
		char scratch[2048];
		sprintf(scratch,"Executable: %s %s contains %d sections.", mExeName.c_str(), mTimeStamp.c_str(), mSections.size() );
		mDocument = html->createHtmlDocument(scratch);

		NVSHARE::HtmlTable *table = mDocument->createHtmlTable("Section Sizes");

		table->addHeader("Section Number,Section Size,Code Size,Data Size,Sub-Section Count,Address Count,Address Size");
		table->addSort("Sorted by Section Size", 2, false, 0, false );
		for (SectionBaseMap::iterator i=mSections.begin(); i!=mSections.end(); ++i)
		{
			SectionBase &s = (*i).second;
			s.reportSections((*i).first,table);
		}

		table->excludeTotals(1);
		table->computeTotals();

		for (SectionBaseMap::iterator i=mSections.begin(); i!=mSections.end(); ++i)
		{
			SectionBase &s = (*i).second;
			s.reportDetails((*i).first, mDocument);
		}

		size_t len;
		const char *mem = mDocument->saveDocument(len,NVSHARE::HST_SIMPLE_HTML);
		if ( mem )
		{
			printf("Saving to '%s'\n", "output.html" );
			FILE *fph = fopen("output.html", "wb");
			if ( fph )
			{
				fwrite( mem, len, 1, fph);
				fclose(fph);
			}
			mDocument->releaseDocumentMemory(mem);
		}

		html->releaseHtmlDocument(mDocument);

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

					SectionBaseMap::iterator found = mSections.find( section.mSection );
					if ( found == mSections.end() )
					{
						SectionBase sb;
						sb.addSection(section);
						mSections[ section.mSection ] = sb;
					}
					else
					{
						(*found).second.addSection(section);
					}
				}
				else
				{
					printf("Unexpected number of arguments found in the sections area at line %d\n", lineno);
				}
				break;
			case MFS_ADDRESS:
			case MFS_STATIC_SYMBOLS:
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

					char scratch[1024];
					const char *name = argv[1];
					if ( name[0] == '?' )
					{
						UnDecorateSymbolName( name, scratch, 1024, 0);
						name = scratch;
					}

					char objectName[2048];
					objectName[0] = 0;
					int index = 3;
					if ( strcmp(argv[3],"f") == 0 )
					{
						index++;
						if ( index < argc && strcmp(argv[4],"i") == 0 )
						{
							index++;
						}
					}

					for (int i=index; i<argc; i++)
					{
						strcat(objectName,argv[i]);
					}

					char *scan = objectName;
					while ( *scan )
					{
						if ( *scan == '<' )
							*scan = '[';
						else if ( *scan == '>' )
							*scan = ']';
						scan++;
					}

					BasicAddress ba;
					ba.mAddress = adr;
					ba.mSection = section;
					ba.mObjectName = objectName;
					ba.mFunctionName = name;
					ba.mLength = 0;

					addBasicAddress(ba);

				}
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

	void addBasicAddress(BasicAddress &adr)
	{
		SectionBaseMap::iterator found = mSections.find( adr.mSection );
		if ( found == mSections.end() )
		{
			if ( adr.mSection == 0 )
			{
				printf("Skipping section zero.\n");
			}
			else
			{
				assert(0);
			}
		}
		else
		{
			(*found).second.addBasicAddress(adr);
		}
	}

	MapFileState			mState;
	std::string				mExeName;
	std::string 			mTimeStamp;
	SectionBaseMap			mSections;
	NVSHARE::HtmlDocument 	*mDocument;
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
