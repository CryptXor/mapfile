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

class MapFile;
struct SectionBase;

struct SubSectionData
{
	unsigned int mSection;
	unsigned int mSectionSize;
	const char * mSubSectionName;
	const char * mClassName;
	unsigned int mAddressCount;
	unsigned int mAddressSize;

	void addToTable(NVSHARE::HtmlTable *table)
	{
		table->addColumn(mSection);
		table->addColumn(mSectionSize);
		table->addColumn(mSubSectionName);
		table->addColumn(mClassName);
		table->addColumn(mAddressCount);
		table->addColumn(mAddressSize);
		table->nextRow();
	}
};

struct ObjectData
{
	ObjectData(void)
	{
		mSection = 0;
		mSectionSize = 0;
		mSubSectionName = "";
		mClassName = "";
		mObjectSize = 0;
		mObjectAddressCount = 0;
		mObjectName = "";
		mObjectFunctionCount = 0;
	}
	unsigned int mSection;
	unsigned int mSectionSize;
	const char * mSubSectionName;
	const char * mClassName;
	int mObjectSize;
	unsigned int mObjectAddressCount;
	const char * mObjectName;
	unsigned int mObjectFunctionCount;

	void addToTable(NVSHARE::HtmlTable *table)
	{
		table->addColumn(mSection);
		table->addColumn(mSectionSize);
		table->addColumn(mSubSectionName);
		table->addColumn(mClassName);
		table->addColumn(mObjectSize);
		table->addColumn(mObjectAddressCount);
		table->addColumn(mObjectName);
		table->addColumn(mObjectFunctionCount);
		table->nextRow();
	}
};

struct FunctionData
{
	unsigned int mSection;
	unsigned int mFunctionSize;
	unsigned int mFunctionCount;
	const char * mFunctionName;
	const char * mObjectName;
};

typedef std::vector< SubSectionData > SubSectionDataVector;
typedef std::vector< ObjectData > ObjectDataVector;
typedef std::vector< FunctionData > FunctionDataVector;

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
	BasicAddress(void)
	{
		mMatched = false;
	}
	unsigned int mAddress;
	unsigned int mSection;
	std::string  mFunctionName;
	std::string  mObjectName;
	unsigned int mLength;
	bool	     mMatched;
};

typedef std::vector< BasicAddress > BasicAddressVector;

typedef std::vector< std::string > StringVector;

const char * getUndecoratedName(const char *name)
{
	static char scratch[1024];
	if ( name[0] == '?' )
	{
		UnDecorateSymbolName( name, scratch, 1024, 0);
		name = scratch;
	}
	return name;
}

struct SectionData
{
	SectionData(void)
	{
		mMatching = 0;
		mSecondMap = false;
	}
	~SectionData(void)
	{
		delete mMatching;
	}

	unsigned int	mSection;
	unsigned int	mTotalLength;
	unsigned int	mTotalCode;
	unsigned int	mTotalData;
	unsigned int	mSectionCount;
	unsigned int	mTotalCount;
	unsigned int	mTotal;
	bool			mSecondMap;


	void	addToDualTable(NVSHARE::HtmlTable *table)
	{
		table->addColumn(mSection);

		int sizeDiff = mTotalLength;
		int sectionDiff = mSectionCount;
		if ( mMatching )
		{
			sizeDiff = (int)mMatching->mTotalLength - (int)mTotalLength;
			sectionDiff = (int)mMatching->mSectionCount - (int)mSectionCount;
		}
   		table->addColumn(sizeDiff);
   		table->addColumn(sectionDiff);

		if ( mMatching || !mSecondMap )
		{
    		table->addColumn(mTotalLength);
    		table->addColumn(mTotalCode);
    		table->addColumn(mTotalData);
    		table->addColumn(mSectionCount);
    		table->addColumn(mTotalCount);
    		table->addColumn(mTotal);
    	}
    	else
    	{
    		table->addColumn(0);
    		table->addColumn(0);
    		table->addColumn(0);
    		table->addColumn(0);
    		table->addColumn(0);
    		table->addColumn(0);
    	}
    	if ( mMatching )
    	{
    		table->addColumn(mMatching->mTotalLength);
    		table->addColumn(mMatching->mTotalCode);
    		table->addColumn(mMatching->mTotalData);
    		table->addColumn(mMatching->mSectionCount);
    		table->addColumn(mMatching->mTotalCount);
    		table->addColumn(mMatching->mTotal);
    	}
    	else
    	{
    		table->addColumn(0);
    		table->addColumn(0);
    		table->addColumn(0);
    		table->addColumn(0);
    		table->addColumn(0);
    		table->addColumn(0);
    	}

		table->nextRow();
	}

	void	addToTable(NVSHARE::HtmlTable *table)
	{
		table->addColumn(mSection);
		table->addColumn(mTotalLength);
		table->addColumn(mTotalCode);
		table->addColumn(mTotalData);
		table->addColumn(mSectionCount);
		table->addColumn(mTotalCount);
		table->addColumn(mTotal);

		table->nextRow();

	}

	bool addMatching(const SectionData &sd)
	{
		bool ret = false;
		if ( sd.mSection == mSection )
		{
			mMatching = new SectionData;
			*mMatching = sd;
			ret = true;
		}
		return ret;
	}
	SectionData	*mMatching;
};

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

	void report(SubSectionDataVector &slist)
	{
		unsigned int total = 0;
   		for (BasicAddressVector::iterator i=mAddresses.begin(); i!=mAddresses.end(); i++)
   		{
   			total+=(*i).mLength;
   		}

		SubSectionData sd;
		sd.mSection = mSection;
		sd.mSectionSize = mLength;
		sd.mSubSectionName = mName;
		sd.mClassName = mClassName;
		sd.mAddressCount = mAddresses.size();
		sd.mAddressSize  = total;
		slist.push_back(sd);
	}

	void reportDetails(unsigned int section,FunctionDataVector &flist,ObjectDataVector &olist)
	{
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

    		for (ByTypeMap::iterator i=byFunction.begin(); i!=byFunction.end(); ++i)
    		{
    			ByType &bt = (*i).second;

				FunctionData fd;
				fd.mSection = section;
				fd.mFunctionSize = bt.mLength;
				fd.mFunctionCount = bt.mCount;
				fd.mFunctionName = (*i).first.c_str();
				fd.mObjectName   = bt.mName.c_str();

				flist.push_back(fd);

    		}
    	}

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


    		for (ByTypeMap::iterator i=byObject.begin(); i!=byObject.end(); ++i)
    		{
    			ByType &bt = (*i).second;

				ObjectData od;

				od.mSection			= section;
				od.mSectionSize		= mLength;
				od.mSubSectionName	= mName;
				od.mClassName		= mClassName;
				od.mObjectSize		= bt.mLength;
				od.mObjectAddressCount = bt.mCount;
				od.mObjectName = (*i).first.c_str();
				od.mObjectFunctionCount = bt.mFunctions.size();
				olist.push_back(od);
    		}
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

	BasicAddress * findBasicAddress(const BasicAddress &ba)
	{
		BasicAddress *ret = 0;
		for (BasicAddressVector::iterator i=mAddresses.begin(); i!=mAddresses.end(); ++i)
		{
			BasicAddress *b = &(*i);
			if ( strcmp(b->mFunctionName.c_str(),ba.mFunctionName.c_str()) == 0 &&
				 strcmp(b->mObjectName.c_str(),ba.mObjectName.c_str()) == 0 )
			{
				ret = b;
				break;
			}
		}
		return ret;
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

	}

	SectionBase(unsigned int sectionNumber)
	{
		mSectionNumber = sectionNumber;
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

	void reportSections(unsigned int section,SectionData &sd)
	{
		unsigned int total=0;
		unsigned int total_count=0;
		for (SectionVector::iterator i=mSections.begin(); i!=mSections.end(); ++i)
		{
			(*i).getTotals(total,total_count);
		}

		sd.mSection 	= section;
		sd.mTotalLength = mTotalLength;
		sd.mTotalCode   = mTotalCode;
		sd.mTotalData   = mTotalData;
		sd.mSectionCount = mSections.size();
		sd.mTotalCount   = total_count;
		sd.mTotal        = total;

	}

	void reportDetails(unsigned int section,SubSectionDataVector &slist,ObjectDataVector &olist,FunctionDataVector &flist)
	{
		for (SectionVector::iterator i=mSections.begin(); i!=mSections.end(); i++)
		{
			(*i).report(slist);
		}
		for (SectionVector::iterator i=mSections.begin(); i!=mSections.end(); i++)
		{
			(*i).reportDetails(section,flist,olist);
		}
	}

	Section * findSection(const Section &section)
	{
		Section *ret = 0;
		for (SectionVector::iterator i=mSections.begin(); i!=mSections.end(); ++i)
		{
			Section &s = (*i);
			if ( strcmp(s.mName,section.mName) == 0 )
			{
				ret = &s;
				break;
			}
		}
		return ret;
	}

	unsigned int    mSectionNumber;
	unsigned int    mTotalLength;
	unsigned int    mTotalCode;
	unsigned int    mTotalData;
	SectionVector	mSections;
};

typedef stdext::hash_map< unsigned int, SectionBase > SectionBaseMap;

typedef std::vector< SectionData > SectionDataVector;

class MapFile : public NVSHARE::InPlaceParserInterface, public NVSHARE::InPlaceParser
{
public:
	MapFile(const char *fname)
	{
		NVSHARE::InPlaceParser::SetFile(fname);
		mState = MFS_BEGIN;
		NVSHARE::InPlaceParser::Parse(this);
	}

	// generate a report which represents a difference between two map files...
	void generateReport(MapFile &mf)
	{
		NVSHARE::HtmlTableInterface *html = NVSHARE::getHtmlTableInterface();
		char scratch[2048];
		sprintf(scratch,"Executable: %s %s contains %d sections.", mExeName.c_str(), mTimeStamp.c_str(), mSections.size() );
		mDocument = html->createHtmlDocument(scratch);

		NVSHARE::HtmlTable *table = mDocument->createHtmlTable("Section Sizes");

		table->addHeader("Section Number,Size Diff,Sub-Section Diff,Section SizeA,Code SizeA,Data SizeA,Sub-Section CountA,Address CountA,Address SizeA,Section SizeB,Code SizeB,Data SizeB,Sub-Section CountB,Address CountB,Address SizeB");

		table->addSort("Sorted by Section Size", 2, false, 0, false );

		SectionDataVector dlist;

		for (SectionBaseMap::iterator i=mSections.begin(); i!=mSections.end(); ++i)
		{
			SectionBase &s = (*i).second;
			SectionData sd;
			s.reportSections((*i).first,sd);
			dlist.push_back(sd);
		}

		for (SectionBaseMap::iterator i=mf.mSections.begin(); i!=mf.mSections.end(); ++i)
		{
			SectionBase &s = (*i).second;
			SectionData sd;
			s.reportSections((*i).first,sd);

			bool matched = false;

			for (SectionDataVector::iterator j=dlist.begin(); j!=dlist.end(); ++j)
			{
				if ( (*j).addMatching(sd) )
				{
					matched = true;
					break;
				}
			}
			if ( !matched )
			{
				sd.mSecondMap = true;
				dlist.push_back(sd);
			}
		}


		for (SectionDataVector::iterator j=dlist.begin(); j!=dlist.end(); ++j)
		{
			(*j).addToDualTable(table);
		}

		table->excludeTotals(1);
		table->computeTotals();


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

	void generateReport(void)
	{
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
			SectionData sd;
			s.reportSections((*i).first,sd);
			sd.addToTable(table);
		}

		table->excludeTotals(1);
		table->computeTotals();


  		SubSectionDataVector subSections;
  		ObjectDataVector	 objects;
		FunctionDataVector functions;
		for (SectionBaseMap::iterator i=mSections.begin(); i!=mSections.end(); ++i)
		{
			SectionBase &s = (*i).second;
			s.reportDetails((*i).first,subSections,objects,functions);
		}

		table = mDocument->createHtmlTable("Sub-Section Sizes");

		table->addHeader("Section Number,Section Size,Sub-Section Name,Class Name,Address Count, Address Size");
		table->addSort("Sorted by Section Size", 2, false, 0, false );
		for (SubSectionDataVector::iterator i=subSections.begin(); i!=subSections.end(); ++i)
		{
			(*i).addToTable(table);
		}

		table->excludeTotals(1);
		table->computeTotals();
#if 0
		table = mDocument->createHtmlTable("Object File Sizes");

	    //                      1              2           3              4           5
		table->addHeader("Section Number,Section Size,Sub-Section Name,Class Name,Object Size,Object Address Entries,Object Name,Object Function Count");
		table->addSort("Sorted by Object Size", 5, false, 0, false );
		for (ObjectDataVector::iterator i=objects.begin(); i!=objects.end(); ++i)
		{
			(*i).addToTable(table);
		}

		table->excludeTotals(1);
		table->computeTotals();
#endif


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
						SectionBase sb(section.mSection);
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

	SectionBase * findSectionBase(unsigned int sectionNumber)
	{
		SectionBase *ret = 0;
		SectionBaseMap::iterator found = mSections.find(sectionNumber);
		if ( found != mSections.end() )
		{
			ret = &(*found).second;
		}
		return ret;
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
		mf.generateReport();
	}
	else if ( argc == 3 )
	{
		MapFile mf1(argv[1]);
		MapFile mf2(argv[2]);
		mf1.generateReport(mf2);
	}
	else
	{
		printf("Usage: MapFile <mapfileName>\r\n");
		printf("Usage: MapFile <mapFile1> <mapFile2>\r\n");
	}
}
