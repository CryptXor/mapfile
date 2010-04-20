#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <DbgHelp.h>
#include <string>
#include <set>
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

struct Summary
{
	Summary(void)
	{
		mPrimarySize = 0;
		mPrimaryCount = 0;

		mSecondarySize = 0;
		mSecondaryCount = 0;

		mSecondaryGreaterSize = 0;
		mSecondaryGreaterCount = 0;

		mPrimaryGreaterSize = 0;
		mPrimaryGreaterCount = 0;

		mSameSize = 0;
		mSameCount = 0;

		mSGPrimarySize = 0;
		mSGSecondarySize = 0;

		mPGPrimarySize = 0;
		mPGSecondarySize = 0;

		mSGPrimaryCount = 0;
		mSGSecondaryCount = 0;

		mPGPrimaryCount = 0;
		mPGSecondaryCount = 0;
	}

	void addToTable(NVSHARE::HtmlTable *table)
	{
		table->addColumn("[1] Primary Only");
		table->addColumn(mPrimarySize);
		table->addColumn(mPrimaryCount);
		table->addColumn(0);
		table->addColumn(0);
		table->addColumn(-mPrimarySize);
		table->addColumn(-mPrimaryCount);
		table->nextRow();

		table->addColumn("[2] Secondary Only");
		table->addColumn(0);
		table->addColumn(0);
		table->addColumn(mSecondarySize);
		table->addColumn(mSecondaryCount);
		table->addColumn(mSecondarySize);
		table->addColumn(mSecondaryCount);
		table->nextRow();

		table->addColumn("[3] Secondary Greater");
		table->addColumn(mSGPrimarySize);
		table->addColumn(mSGPrimaryCount);
		table->addColumn(mSGSecondarySize);
		table->addColumn(mSGSecondaryCount);
		table->addColumn(mSecondaryGreaterSize);
		table->addColumn(mSecondaryGreaterCount);
		table->nextRow();

		table->addColumn("[4] Primary Greater");
		table->addColumn(mPGPrimarySize);
		table->addColumn(mPGPrimaryCount);

		table->addColumn(mPGSecondarySize);
		table->addColumn(mPGSecondaryCount);

		table->addColumn(-mPrimaryGreaterSize);
		table->addColumn(-mPrimaryGreaterCount);
		table->nextRow();

		table->addColumn("[5] Same Size");
		table->addColumn(mSameSize);
		table->addColumn(mSameCount);
		table->addColumn(mSameSize);
		table->addColumn(mSameCount);
		table->addColumn(0);
		table->addColumn(mSameCount);
		table->nextRow();


	}
	int	mPrimarySize;
	int mPrimaryCount;
	int mSecondarySize;
	int mSecondaryCount;

	int mSGPrimarySize;
	int mSGSecondarySize;
	int mSGPrimaryCount;
	int mSGSecondaryCount;
	int mSecondaryGreaterSize;
	int mSecondaryGreaterCount;

	int mPGPrimarySize;
	int mPGSecondarySize;
	int mPGPrimaryCount;
	int mPGSecondaryCount;
	int mPrimaryGreaterSize;
	int mPrimaryGreaterCount;
	int mSameSize;
	int mSameCount;
};

enum FunctionReport
{
	FR_ONLY_PRIMARY,
	FR_ONLY_SECONDARY,
	FR_PRIMARY_GREATER,
	FR_SECONDARY_GREATER,
	FR_SAME,
};

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
		mSubSectionName = "";
		mClassName = "";
		mObjectSize = 0;
		mObjectAddressCount = 0;
		mObjectFunctionCount = 0;
	}

	unsigned int mSection;
	const char * mSubSectionName;
	const char * mClassName;
	int mObjectSize;
	unsigned int mObjectAddressCount;
	std::string	mObjectName;
	unsigned int mObjectFunctionCount;

	void addToTable(NVSHARE::HtmlTable *table)
	{
		if ( mObjectSize > 0 )
		{
			table->addColumn(mSection);
			table->addColumn(mSubSectionName);
			table->addColumn(mClassName);
			table->addColumn(mObjectSize);
			table->addColumn(mObjectAddressCount);
			table->addColumn(mObjectName.c_str());
			table->addColumn(mObjectFunctionCount);
			table->nextRow();
		}
	}
};

struct FunctionData
{
	unsigned int mSection;
	int mFunctionSize;
	unsigned int mFunctionCount;
	std::string mFunctionName;
	std::string mObjectName;

	void addToTable(NVSHARE::HtmlTable *table)
	{
		if ( mFunctionSize > 0 )
		{
			table->addColumn(mSection);
			table->addColumn(mFunctionSize);
			table->addColumn(mFunctionCount);
			table->addColumn(mFunctionName.c_str());
			table->addColumn(mObjectName.c_str());
			table->nextRow();
		}
	}
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

	bool operator <(const BasicAddress &s) const
	{
		return mAddress < s.mAddress;
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
				fd.mFunctionName = (*i).first;
				fd.mObjectName   = bt.mName;

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
				od.mSubSectionName	= mName;
				od.mClassName		= mClassName;
				od.mObjectSize		= bt.mLength;
				od.mObjectAddressCount = bt.mCount;
				od.mObjectName = (*i).first;
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

	void computeSizes(void)
	{
		if ( !mAddresses.empty() )
		{
			std::sort(mAddresses.begin(), mAddresses.end() );
			int count = mAddresses.size();
			for (int i=0; i<(count-1); i++)
			{
				BasicAddress &ba = mAddresses[i];
				ba.mLength = mAddresses[i+1].mAddress - ba.mAddress;
			}
			BasicAddress &ba = mAddresses[count-1];
			ba.mLength = mLength - (ba.mAddress - mAddress);
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

	void computeSizes(void)
	{
		for (SectionVector::iterator i=mSections.begin(); i!=mSections.end(); ++i)
		{
			(*i).computeSizes();
		}
	}

	unsigned int    mSectionNumber;
	unsigned int    mTotalLength;
	unsigned int    mTotalCode;
	unsigned int    mTotalData;
	SectionVector	mSections;
};

typedef stdext::hash_map< unsigned int, SectionBase > SectionBaseMap;

typedef std::vector< SectionData > SectionDataVector;

struct SubSectionDifference
{
	SubSectionDifference(SubSectionData *sd)
	{
		mPrimary = sd;
		mSecondary = NULL;
		mIsSecond = false;
	}

	bool operator <(const SubSectionDifference &s) const
	{
		if ( mPrimary->mSection < s.mPrimary->mSection ) return true;
		if ( mPrimary->mSection > s.mPrimary->mSection ) return false;
	    return strcmp( mPrimary->mSubSectionName, s.mPrimary->mSubSectionName ) < 0;
	}

	void addSecondary(SubSectionData *sd)
	{
		mSecondary = sd;
	}

	void addToTable(NVSHARE::HtmlTable *table)
	{
		if ( mSecondary )
		{
			if ( mPrimary->mSectionSize != mSecondary->mSectionSize )
			{
    			table->addColumn(mPrimary->mSection);
	    		table->addColumn((int)mSecondary->mSectionSize - (int)mPrimary->mSectionSize);
	    		table->addColumn(mPrimary->mSubSectionName);
	    		table->addColumn(mPrimary->mClassName);
	    		table->addColumn((int)mSecondary->mAddressCount - (int)mPrimary->mAddressCount);
	    		table->addColumn((int)mSecondary->mAddressSize - (int)mPrimary->mAddressSize );
	    		if ( mSecondary->mSectionSize > mPrimary->mSectionSize )
	    		{
    				table->addColumn("DIFF:SECONDARY GREATER");
	    		}
	    		else
	    		{
    				table->addColumn("DIFF:PRIMARY GREATER");
	    		}
				table->nextRow();
			}
		}
		else
		{
    		table->addColumn(mPrimary->mSection);
    		table->addColumn(mPrimary->mSectionSize);
    		table->addColumn(mPrimary->mSubSectionName);
    		table->addColumn(mPrimary->mClassName);
    		table->addColumn(mPrimary->mAddressCount);
    		table->addColumn(mPrimary->mAddressSize);
    		if ( mIsSecond )
    		{
    			table->addColumn("SECONDARY");
    		}
    		else
    		{
    			table->addColumn("PRIMARY");
    		}
			table->nextRow();
		}
	}

	SubSectionData	*mPrimary;
	SubSectionData	*mSecondary;
	bool			 mIsSecond;
};

typedef std::set< SubSectionDifference > SubSectionDifferenceSet;

struct ObjectDataDifference
{

	ObjectDataDifference(ObjectData *p)
	{
		mPrimary = p;
		mSecondary = NULL;
		mIsSecond = false;
	}


	bool operator <(const ObjectDataDifference &s) const
	{
		// see if it is in the same section
		if ( mPrimary->mSection < s.mPrimary->mSection ) return true;
		if ( mPrimary->mSection > s.mPrimary->mSection ) return false;
		// see if it is in the same sub-section
	    int v = strcmp( mPrimary->mSubSectionName, s.mPrimary->mSubSectionName );
	    if ( v != 0 ) return v < 0;
	    // see if it is in the same object file
	    return strcmp( mPrimary->mObjectName.c_str(), s.mPrimary->mObjectName.c_str() ) < 0;
	}

	void addSecondary(ObjectData *sd)
	{
		mSecondary = sd;
	}

	void addToTable(NVSHARE::HtmlTable *table)
	{
		if ( mSecondary )
		{
			if ( mPrimary->mObjectSize != mSecondary->mObjectSize )
			{

    			table->addColumn(mPrimary->mSection);
    			table->addColumn(mPrimary->mSubSectionName);
    			table->addColumn(mPrimary->mClassName);
    			table->addColumn((int)mSecondary->mObjectSize - (int)mPrimary->mObjectSize);
    			table->addColumn((int)mSecondary->mObjectAddressCount - (int)mPrimary->mObjectAddressCount);
    			table->addColumn(mPrimary->mObjectName.c_str());
    			table->addColumn((int)mSecondary->mObjectFunctionCount - (int)mPrimary->mObjectFunctionCount);

	    		if ( mSecondary->mObjectSize > mPrimary->mObjectSize )
	    		{
    				table->addColumn("DIFF:SECONDARY GREATER");
	    		}
	    		else
	    		{
    				table->addColumn("DIFF:PRIMARY GREATER");
	    		}

				table->nextRow();
			}
		}
		else
		{

			table->addColumn(mPrimary->mSection);
			table->addColumn(mPrimary->mSubSectionName);
			table->addColumn(mPrimary->mClassName);
			table->addColumn(mPrimary->mObjectSize);
			table->addColumn(mPrimary->mObjectAddressCount);
			table->addColumn(mPrimary->mObjectName.c_str());
			table->addColumn(mPrimary->mObjectFunctionCount);

    		if ( mIsSecond )
    		{
    			table->addColumn("SECONDARY");
    		}
    		else
    		{
    			table->addColumn("PRIMARY");
    		}
			table->nextRow();
		}
	}

	ObjectData	*mPrimary;
	ObjectData	*mSecondary;
	bool		 mIsSecond;

};

typedef std::set< ObjectDataDifference > ObjectDataDifferenceSet;

struct FunctionDataDifference
{

	FunctionDataDifference(FunctionData *p)
	{
		mPrimary = p;
		mSecondary = NULL;
		mIsSecond = false;
	}


	bool operator <(const FunctionDataDifference &s) const
	{
		// see if it is in the same section
		if ( mPrimary->mSection < s.mPrimary->mSection ) return true;
		if ( mPrimary->mSection > s.mPrimary->mSection ) return false;
		// only compare the object name if there is more than one funtion of this name present.
#if 0
		if ( mPrimary->mFunctionCount > 1 || s.mPrimary->mFunctionCount > 1 )
		{
			int v = strcmp( mPrimary->mObjectName.c_str(), s.mPrimary->mObjectName.c_str() );
			if ( v != 0 ) return v < 0;
		}
#endif
	    return strcmp( mPrimary->mFunctionName.c_str(), s.mPrimary->mFunctionName.c_str() ) < 0;
	}

	void addSecondary(FunctionData *sd)
	{
		mSecondary = sd;
	}

	// accumulate totals into the summary result structure
	void addToSummary(Summary &s)
	{
		if ( mSecondary )
		{
			if ( mSecondary->mFunctionSize == mPrimary->mFunctionSize ) // if they are the same size, accumulate the same size totals
			{
				s.mSameSize+=mSecondary->mFunctionSize;
				s.mSameSize+=mSecondary->mFunctionCount;
			}
			else if ( mSecondary->mFunctionSize > mPrimary->mFunctionSize ) // if the secondary is greater than the primary, accmulate totals
			{
				s.mSGPrimarySize+=mPrimary->mFunctionSize;
				s.mSGPrimaryCount+=mPrimary->mFunctionCount;
				s.mSGSecondarySize+=mSecondary->mFunctionSize;
				s.mSGSecondaryCount+=mSecondary->mFunctionCount;
				s.mSecondaryGreaterSize+= (mSecondary->mFunctionSize - mPrimary->mFunctionSize );
				s.mSecondaryGreaterCount+= (mSecondary->mFunctionCount - mPrimary->mFunctionCount );
			}
			else // primary is greater than the secondary, accmulate totals
			{
				s.mPGPrimarySize+=mPrimary->mFunctionSize;
				s.mPGPrimaryCount+=mPrimary->mFunctionCount;

				s.mPGSecondarySize+=mSecondary->mFunctionSize;
				s.mPGSecondaryCount+=mSecondary->mFunctionCount;

				s.mPrimaryGreaterSize+= (mPrimary->mFunctionSize - mSecondary->mFunctionSize );
				s.mPrimaryGreaterCount+= (mPrimary->mFunctionCount - mSecondary->mFunctionCount );
			}
		}
		else
		{
			if ( mIsSecond )
			{
				s.mSecondarySize+=mPrimary->mFunctionSize;
				s.mSecondaryCount+=mPrimary->mFunctionCount;
			}
			else
			{
				s.mPrimarySize+=mPrimary->mFunctionSize;
				s.mPrimaryCount+=mPrimary->mFunctionCount;
			}
		}
	}

	void addToTable(NVSHARE::HtmlTable *table,FunctionReport type)
	{

		bool display = false;
		switch ( type )
		{
			case FR_SAME:
				if ( mSecondary && mPrimary->mFunctionSize == mSecondary->mFunctionSize )
				{
					display = true;
				}
				break;
			case FR_ONLY_PRIMARY:
				if ( mSecondary == NULL && !mIsSecond )
				{
					display = true;
				}
				break;
			case FR_ONLY_SECONDARY:
				if ( mSecondary == NULL && mIsSecond )
				{
					display = true;
				}
				break;
			case FR_PRIMARY_GREATER:
				if ( mSecondary && mSecondary->mFunctionSize < mPrimary->mFunctionSize )
				{
					display = true;
				}
				break;
			case FR_SECONDARY_GREATER:
				if ( mSecondary && mSecondary->mFunctionSize > mPrimary->mFunctionSize )
				{
					display = true;
				}
				break;
		}
		if ( display )
		{
    		if ( mSecondary )
    		{
    			{

        			table->addColumn(mPrimary->mSection);
					if ( type == FR_SAME )
					{
        				table->addColumn(mPrimary->mFunctionSize);
        				table->addColumn(mPrimary->mFunctionCount);
					}
					else
					{
        				table->addColumn((int)mSecondary->mFunctionSize - (int)mPrimary->mFunctionSize);
        				table->addColumn((int)mSecondary->mFunctionCount - (int)mPrimary->mFunctionCount);
        			}

        			table->addColumn(mPrimary->mFunctionName.c_str());
        			table->addColumn(mPrimary->mObjectName.c_str());

    	    		if ( mSecondary->mFunctionSize > mPrimary->mFunctionSize )
    	    		{
        				table->addColumn("DIFF:SECONDARY GREATER");
    	    		}
    	    		else if ( mSecondary->mFunctionSize ==  mPrimary->mFunctionSize )
    	    		{
        				table->addColumn("SAME");
    	    		}
    	    		else
    	    		{
        				table->addColumn("DIFF:PRIMARY GREATER");
    	    		}

    				table->nextRow();
    			}
    		}
    		else
    		{

    			table->addColumn(mPrimary->mSection);
    			table->addColumn(mPrimary->mFunctionSize);
    			table->addColumn(mPrimary->mFunctionCount);
    			table->addColumn(mPrimary->mFunctionName.c_str());
    			table->addColumn(mPrimary->mObjectName.c_str());

        		if ( mIsSecond )
        		{
        			table->addColumn("SECONDARY");
        		}
        		else
        		{
        			table->addColumn("PRIMARY");
        		}
    			table->nextRow();
    		}
    	}
	}

	FunctionData	*mPrimary;
	FunctionData	*mSecondary;
	bool		 	mIsSecond;

};

typedef std::set< FunctionDataDifference > FunctionDataDifferenceSet;

class MapFile : public NVSHARE::InPlaceParserInterface, public NVSHARE::InPlaceParser
{
public:
	MapFile(const char *fname)
	{
		NVSHARE::InPlaceParser::SetFile(fname);
		mState = MFS_BEGIN;
		NVSHARE::InPlaceParser::Parse(this);
		for (SectionBaseMap::iterator i=mSections.begin(); i!=mSections.end(); ++i)
		{
			(*i).second.computeSizes();
		}
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

//**************************************************
//*** Gather other data
//**************************************************

  		SubSectionDataVector subSectionsA;
  		ObjectDataVector	 objectsA;
		FunctionDataVector functionsA;
		for (SectionBaseMap::iterator i=mSections.begin(); i!=mSections.end(); ++i)
		{
			SectionBase &s = (*i).second;
			s.reportDetails((*i).first,subSectionsA,objectsA,functionsA);
		}

		printf("Gathering data for comparison.\r\n");

  		SubSectionDataVector subSectionsB;
  		ObjectDataVector	 objectsB;
		FunctionDataVector functionsB;
		for (SectionBaseMap::iterator i=mf.mSections.begin(); i!=mf.mSections.end(); ++i)
		{
			SectionBase &s = (*i).second;
			s.reportDetails((*i).first,subSectionsB,objectsB,functionsB);
		}

//**************************************************
// Compute sub-section differences
//**************************************************

		// ok, now sub-section difference report...
		// First insert all of the subsections from the base map file into an STL set.
		printf("Computing sub-section differences.\r\n");

		SubSectionDifferenceSet subSet;

		for (SubSectionDataVector::iterator i=subSectionsA.begin(); i!=subSectionsA.end(); ++i)
		{
			SubSectionData &sd = (*i);
			SubSectionDifference sdiff(&sd);
			subSet.insert( sdiff );
		}

		// Now, for each subsection in the second map, see if it matches the existing one.
		for (SubSectionDataVector::iterator i=subSectionsB.begin(); i!=subSectionsB.end(); ++i)
		{
			SubSectionData &sd = (*i);
			SubSectionDifference sdiff(&sd);

			SubSectionDifferenceSet::iterator found = subSet.find(sdiff);
			if ( found == subSet.end() )
			{
				sdiff.mIsSecond = true;
				subSet.insert( sdiff );
			}
			else
			{
				(*found).addSecondary(&sd);
			}
		}


		printf("Generating sub-section differences table.\r\n");

		table = mDocument->createHtmlTable("Sub-Section Size Differences");
		table->addHeader("Section Number,Section Size,Sub-Section Name,Class Name,Address Count, Address Size,Type");
		table->addSort("Sorted by Section Size", 2, false, 0, false );
		for (SubSectionDifferenceSet::iterator i=subSet.begin(); i!=subSet.end(); ++i)
		{
			(*i).addToTable(table);
		}

		table->excludeTotals(1);
		table->computeTotals();


//**************************************************
// Compute object file differences
//**************************************************
		printf("Gathering object file differences.\r\n");

		ObjectDataDifferenceSet objectSet;

		for (ObjectDataVector::iterator i=objectsA.begin(); i!=objectsA.end(); ++i)
		{
			ObjectData &sd = (*i);
			if ( sd.mObjectSize > 0 )
			{
				ObjectDataDifference sdiff(&sd);
				objectSet.insert( sdiff );
			}
		}

		// Now, for each subsection in the second map, see if it matches the existing one.
		for (ObjectDataVector::iterator i=objectsB.begin(); i!=objectsB.end(); ++i)
		{
			ObjectData &sd = (*i);
			if ( sd.mObjectSize > 0 )
			{
    			ObjectDataDifference sdiff(&sd);
    			ObjectDataDifferenceSet::iterator found = objectSet.find(sdiff);
    			if ( found == objectSet.end() )
    			{
    				sdiff.mIsSecond = true;
    				objectSet.insert( sdiff );
    			}
    			else
    			{
    				(*found).addSecondary(&sd);
    			}
    		}
		}

//**************************************************
// Compute function differences
//**************************************************

		printf("Aggregating unrolled loop code.\r\n");

		// aggregate all 'unwound' founctions...
		int nxpTotal = 0;
		int nxpCount = 0;
		FunctionData *unwound = 0;
		FunctionData *fcatch = 0;
		FunctionData *freal = 0;
		for (FunctionDataVector::iterator i=functionsA.begin(); i!=functionsA.end(); ++i)
		{
			FunctionData &fd = (*i);
			//                                     0123456789
			if ( strncmp(fd.mFunctionName.c_str(),"__unwind$",9) == 0 )
			{
				if ( unwound )
				{
					unwound->mFunctionSize+=fd.mFunctionSize;
					unwound->mFunctionCount+=fd.mFunctionCount;
					fd.mFunctionSize = 0;
				}
				else
				{
					fd.mFunctionName = "unwound code";
					fd.mObjectName = "unwind_code";
					unwound = &fd;
				}
			}
			else
			{
				const char *scan = strstr(fd.mFunctionName.c_str(),"NxParameterized");
				if ( scan )
				{
					nxpTotal+=fd.mFunctionSize;
					nxpCount+=fd.mFunctionCount;
				}
			}

			//                                     0123456789
			if ( strncmp(fd.mFunctionName.c_str(),"__catch$",8) == 0 )
			{
				if ( fcatch )
				{
					fcatch->mFunctionSize+=fd.mFunctionSize;
					fcatch->mFunctionCount+=fd.mFunctionCount;
					fd.mFunctionSize = 0;
				}
				else
				{
					fd.mFunctionName = "catch code";
					fd.mObjectName = "catch_code";
					fcatch = &fd;
				}
			}

			//                                     0123456
			if ( strncmp(fd.mFunctionName.c_str(),"__real@",7) == 0 )
			{
				if ( freal )
				{
					freal->mFunctionSize+=fd.mFunctionSize;
					freal->mFunctionCount+=fd.mFunctionCount;
					fd.mFunctionSize = 0;
				}
				else
				{
					fd.mFunctionName = "@real code";
					fd.mObjectName = "@real_code";
					freal = &fd;
				}
			}



		}
		printf("Aggregation results for PRIMARY .MAP file\r\n");
		if ( nxpTotal > 0 )
		{
			printf("Functions with the string 'NxParameterized' in them total %s in %s items.\r\n", NVSHARE::formatNumber(nxpTotal), NVSHARE::formatNumber(nxpCount));
		}

		if ( unwound )
		{
			printf("Aggregated a total of %s bytes in %s unrolled loops.\r\n", NVSHARE::formatNumber(unwound->mFunctionSize), NVSHARE::formatNumber(unwound->mFunctionCount) );
		}

		nxpTotal = 0;
		nxpCount = 0;
		unwound = 0;
		fcatch = 0;
		freal = 0;
		for (FunctionDataVector::iterator i=functionsB.begin(); i!=functionsB.end(); ++i)
		{
			FunctionData &fd = (*i);
			//                                     0123456789
			if ( strncmp(fd.mFunctionName.c_str(),"__unwind$",9) == 0 )
			{
				if ( unwound )
				{
					unwound->mFunctionSize+=fd.mFunctionSize;
					unwound->mFunctionCount+=fd.mFunctionCount;
					fd.mFunctionSize = 0;
				}
				else
				{
					fd.mFunctionName = "unwound code";
					fd.mObjectName = "unwind_code";
					unwound = &fd;
				}
			}
			else
			{
				const char *scan = strstr(fd.mFunctionName.c_str(),"NxParameterized");
				if ( scan )
				{
					nxpTotal+=fd.mFunctionSize;
					nxpCount+=fd.mFunctionCount;
				}
			}

			//                                     0123456789
			if ( strncmp(fd.mFunctionName.c_str(),"__catch$",8) == 0 )
			{
				if ( fcatch )
				{
					fcatch->mFunctionSize+=fd.mFunctionSize;
					fcatch->mFunctionCount+=fd.mFunctionCount;
					fd.mFunctionSize = 0;
				}
				else
				{
					fd.mFunctionName = "catch code";
					fd.mObjectName = "catch_code";
					fcatch = &fd;
				}
			}

			if ( strncmp(fd.mFunctionName.c_str(),"__real@",7) == 0 )
			{
				if ( freal )
				{
					freal->mFunctionSize+=fd.mFunctionSize;
					freal->mFunctionCount+=fd.mFunctionCount;
					fd.mFunctionSize = 0;
				}
				else
				{
					fd.mFunctionName = "@real code";
					fd.mObjectName = "@real_code";
					freal = &fd;
				}
			}


		}
		printf("Aggregation results for SECONDARY .MAP file\r\n");
		if ( nxpTotal > 0 )
		{
			printf("Functions with the string 'NxParameterized' in them total %s in %s items.\r\n", NVSHARE::formatNumber(nxpTotal), NVSHARE::formatNumber(nxpCount));
		}

		if ( unwound )
		{
			printf("Aggregated a total of %s bytes in %s unrolled loops.\r\n", NVSHARE::formatNumber(unwound->mFunctionSize), NVSHARE::formatNumber(unwound->mFunctionCount) );
		}


		printf("Gathering function differences.\r\n");

		FunctionDataDifferenceSet functionSet;

		for (FunctionDataVector::iterator i=functionsA.begin(); i!=functionsA.end(); ++i)
		{
			FunctionData &sd = (*i);
			if ( sd.mFunctionSize > 0 )
			{
				FunctionDataDifference sdiff(&sd);
				functionSet.insert( sdiff );
			}
		}

		// Now, for each subsection in the second map, see if it matches the existing one.
		for (FunctionDataVector::iterator i=functionsB.begin(); i!=functionsB.end(); ++i)
		{
			FunctionData &sd = (*i);
			if ( sd.mFunctionSize > 0 )
			{
    			FunctionDataDifference sdiff(&sd);
    			FunctionDataDifferenceSet::iterator found = functionSet.find(sdiff);
    			if ( found == functionSet.end() )
    			{
    				sdiff.mIsSecond = true;
    				functionSet.insert( sdiff );
    			}
    			else
    			{
    				(*found).addSecondary(&sd);
    			}
    		}
		}

		printf("Generating function summary table.\r\n");
		table = mDocument->createHtmlTable("Summary Report of the Function/Symbol table differences between the two .MAP files");
		//                      1          2            3             4              5                6                7
		table->addHeader("Summary/Type,Primary/Size,Primary/Count,Secondary/Size,Secondary/Count,Difference/Size,Difference/Count");
		Summary s;
		table->addSort("Sorted by Summary Type", 1, true, 0, false );
		for (FunctionDataDifferenceSet::iterator i=functionSet.begin(); i!=functionSet.end(); ++i)
		{
			(*i).addToSummary(s);
		}
		s.addToTable(table);
		table->computeTotals();

		{
			printf("Generating object file differences table.\r\n");
			table = mDocument->createHtmlTable("Difference in Object Files between the two .MAP files");
			table->addHeader("Section Number,Sub-Section Name,Class Name,Object Size,Object Address Entries,Object Name,Object Function Count,Type");
			table->addSort("Sorted by Object Size", 4, false, 0, false );
			for (ObjectDataDifferenceSet::iterator i=objectSet.begin(); i!=objectSet.end(); ++i)
			{
				(*i).addToTable(table);
			}

			table->excludeTotals(1);
			table->computeTotals();
		}


		printf("Generating function primary table.\r\n");
		table = mDocument->createHtmlTable("Functions/Symbols which only exist int the Primary Map File");
		table->addHeader("Section,Function Size,Function Count,Function Name,Object Name,Type");
		table->addSort("Sorted by Function Size", 2, false, 0, false );
		for (FunctionDataDifferenceSet::iterator i=functionSet.begin(); i!=functionSet.end(); ++i)
		{
			(*i).addToTable(table,FR_ONLY_PRIMARY);
		}

		table->computeTotals();

		printf("Generating function secondary table.\r\n");
		table = mDocument->createHtmlTable("Function/Symbols which only exist in the Secondary Map File");
		table->addHeader("Section,Function Size,Function Count,Function Name,Object Name,Type");
		table->addSort("Sorted by Function Size", 2, false, 0, false );
		for (FunctionDataDifferenceSet::iterator i=functionSet.begin(); i!=functionSet.end(); ++i)
		{
			(*i).addToTable(table,FR_ONLY_SECONDARY);
		}

		table->computeTotals();

		printf("Generating function primary greater table.\r\n");
		table = mDocument->createHtmlTable("Function/Symbols which are Larger in the Primary MAP File");
		table->addHeader("Section,Function Size,Function Count,Function Name,Object Name,Type");
		table->addSort("Sorted by Function Size", 2, false, 0, false );
		for (FunctionDataDifferenceSet::iterator i=functionSet.begin(); i!=functionSet.end(); ++i)
		{
			(*i).addToTable(table,FR_PRIMARY_GREATER);
		}

		table->computeTotals();

		printf("Generating function secondary greater table.\r\n");
		table = mDocument->createHtmlTable("Function/Suymbols which are larger in the Secondary MAP File");
		table->addHeader("Section,Function Size,Function Count,Function Name,Object Name,Type");
		table->addSort("Sorted by Function Size", 2, false, 0, false );
		for (FunctionDataDifferenceSet::iterator i=functionSet.begin(); i!=functionSet.end(); ++i)
		{
			(*i).addToTable(table,FR_SECONDARY_GREATER);
		}

		table->computeTotals();


		printf("Generating functions same size table.\r\n");
		table = mDocument->createHtmlTable("Functions/Symbols which are the Same Size between both the Primary and Secondary .MAP files");
		table->addHeader("Section,Function Size,Function Count,Function Name,Object Name,Type");
		table->addSort("Sorted by Function Size", 2, false, 0, false );
		for (FunctionDataDifferenceSet::iterator i=functionSet.begin(); i!=functionSet.end(); ++i)
		{
			(*i).addToTable(table,FR_SAME);
		}

		table->computeTotals();



		{
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
		}

		{
			size_t len;
			const char *mem = mDocument->saveDocument(len,NVSHARE::HST_CSV);
			if ( mem )
			{
				printf("Saving to '%s'\n", "output.csv" );
				FILE *fph = fopen("output.csv", "wb");
				if ( fph )
				{
					fwrite( mem, len, 1, fph);
					fclose(fph);
				}
				mDocument->releaseDocumentMemory(mem);
			}
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

		table = mDocument->createHtmlTable("Object File Sizes");

	    //                      1              2           3              4           5
		table->addHeader("Section Number,Sub-Section Name,Class Name,Object Size,Object Address Entries,Object Name,Object Function Count");
		table->addSort("Sorted by Object Size", 4, false, 0, false );
		for (ObjectDataVector::iterator i=objects.begin(); i!=objects.end(); ++i)
		{
			(*i).addToTable(table);
		}

		table->excludeTotals(1);
		table->computeTotals();

		table = mDocument->createHtmlTable("Function Sizes");

		NVSHARE::HtmlTable *nxparam = mDocument->createHtmlTable("Functions with 'NxParameterized' in the name");


		nxparam->addHeader("Section,Function Size,Function Count,Function Name,Object Name");
		nxparam->addSort("Sorted by Function Size", 2, false, 0, false );
		// aggregate all 'unwound' founctions...
		int nxpTotal = 0;
		int nxpCount = 0;
		FunctionData *unwound = 0;
		for (FunctionDataVector::iterator i=functions.begin(); i!=functions.end(); ++i)
		{
			FunctionData &fd = (*i);
			//                                     0123456789
			if ( strncmp(fd.mFunctionName.c_str(),"__unwind$",9) == 0 )
			{
				if ( unwound )
				{
					unwound->mFunctionSize+=fd.mFunctionSize;
					unwound->mFunctionCount+=fd.mFunctionCount;
					fd.mFunctionSize = 0;
				}
				else
				{
					fd.mFunctionName = "unwound code";
					unwound = &fd;
				}
			}
			else
			{
				const char *scan = strstr(fd.mFunctionName.c_str(),"NxParameterized");
				if ( scan )
				{
					nxpTotal+=fd.mFunctionSize;
					nxpCount+=fd.mFunctionCount;
					fd.addToTable(nxparam);
				}
			}
		}
		if ( nxpTotal > 0 )
		{
			printf("Functions with the string 'NxParameterized' in them total %s in %s items.\r\n", NVSHARE::formatNumber(nxpTotal), NVSHARE::formatNumber(nxpCount));
    		nxparam->excludeTotals(1);
    		nxparam->computeTotals();
		}


	    //                    1          2           3              4           5
		table->addHeader("Section,Function Size,Function Count,Function Name,Object Name");
		table->addSort("Sorted by Function Size", 2, false, 0, false );
		for (FunctionDataVector::iterator i=functions.begin(); i!=functions.end(); ++i)
		{
			(*i).addToTable(table);
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
#if 1
					const char *str = strstr(name,"scalar deleting destructor");
					if ( str )
					{
						char *stomp = (char *)str;
						stomp[0] = 'v';
						stomp[1] = 'e';
						stomp[2] = 'c';
						stomp[3] = 't';
						stomp[4] = 'o';
						stomp[5] = 'r';
					}
#endif

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
