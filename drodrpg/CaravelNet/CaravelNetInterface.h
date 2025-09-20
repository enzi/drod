// $Id: CaravelNetInterface.h 8019 2007-07-14 22:30:11Z trick $

/* ***** BEGIN LICENSE BLOCK *****
* Version: MPL 1.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is Deadly Rooms of Death.
*
* The Initial Developer of the Original Code is
* Caravel Software.
* Portions created by the Initial Developer are Copyright (C) 2002, 2005
* Caravel Software. All Rights Reserved.
*
* Contributor(s): Matt Schikore (Schik)
*
* ***** END LICENSE BLOCK ***** */

#ifndef CARAVELNETINTERFACE_H
#define CARAVELNETINTERFACE_H

#include "../DRODLib/NetInterface.h"
#include <expat.h>

//Enumeration of supported hold fields.
namespace HOLD
{
	enum HOLDTagType
	{
		First_Tag=0,
		Version=First_Tag,  //DROD version
		GID_Created,        //timestamp
		Author,             //author name (ASCII)
		CNetName,           //author's CaravelNet name (ASCII)
		HoldId,             //CaravelNet ID
		LastUpdated,        //timestamp
		Difficulty,         //CaravelNet rating
		Rating,             //CaravelNet rating
		NumVotes,           //CaravelNet
		FileSize,           //user readable size
		FileSizeBytes,      //logical size
		NameMessageB64,     //hold name (Unicode)
		OriginalAuthorB64,  //author name GID (Unicode)
		Beta,               //hold is in beta
		MyDifficulty,       //player's rating of hold
		MyRating,           //player's rating of hold
		Media,              //type of game media this record represents
		Thumbnail,          //image thumbnail displaying this record
		ThumbnailURL,       //URL of image thumbnail
		SmStatus,           //smitemaster's selection
		Tag_Count,
		Unknown_Tag
	};
}

//****************************************************************************
class CCaravelNetInterface : public CNetInterface
{
public:
	CCaravelNetInterface();
   virtual ~CCaravelNetInterface();

	virtual bool Busy() const;
	virtual void ClearActiveAction();
	virtual void Enable(const bool bVal=true);

	virtual bool ChatLogout();
	virtual UINT DownloadDemo(const long demoID);
   virtual UINT DownloadHold(const long holdID);
	virtual UINT DownloadHoldList();
	virtual UINT DownloadRecords(const string& buffer, const bool bSendIfConquered);
   virtual UINT DownloadStyle(const WCHAR* styleName);
	virtual UINT HasRegisteredAccess();
	virtual void MatchCNetHolds();
	virtual UINT RateHold(const WCHAR* holdName, const float fDifficulty,
			const float fOverall);
	virtual UINT RateMod(const WCHAR* name, const float fRating);
	virtual UINT RequestNewKey(const string& strUser);
	virtual UINT SendChatText(const SendData& text, const int lastID, const UINT roomID);
	virtual UINT SendChatText(const vector<SendData>& texts, const int lastID, const UINT roomID);
/*
	virtual UINT UploadDemo(const string& text,
			const UINT wMoveCount, const UINT dwTimeElapsed);
	virtual bool UploadDemos(const string& text);
*/
	virtual bool UploadExploredRooms(const string& buffer);
	virtual UINT UploadScore(const string& savedGameXML, const WSTRING& name, const UINT score);

	virtual CStretchyBuffer* GetResults(const UINT handle);
	virtual int  GetStatus(const UINT handle);

	virtual string GetChecksum(CDbSavedGame *pSavedGame, const UINT val=0) const;
	virtual bool   VerifyChecksum(CDbSavedGame *pSavedGame, const UINT val=0) const;

	virtual vector<CNetMedia*>& GetCNetMedia();
	virtual CIDSet& GetBetaHolds();
	virtual CIDSet& GetLocalHolds();
	virtual CIDSet& GetUpdatedHolds();
	virtual bool IsLoggedIn();

	//For XML parsing.
	void         StartElement(const XML_Char *name, const XML_Char **atts);
	void         InElement(const XML_Char *s, int len);
	void         EndElement(const char *name);

protected:
	void AddHoldsOnCNet();
	void ClearResults();
	virtual CPostData* SetCredentials(bool bVerify=true);
	virtual CPostData* SetCredentials(const string& strUser, const string& strKey, bool bVerify = true);
	virtual bool VerifyKeyFormat(const string& strKey);
	void WaitUntilHoldsReceived();

	//Set of holds.
	HOLD::HOLDTagType    ParseTagField(const char *str) const;
	CStretchyBuffer CNetHoldBuffer;
	UINT wHoldHandle;	//when awaiting set of CaravelNet holds

	std::map<UINT, CStretchyBuffer*> results;	//not examined yet
};

#endif //...#ifndef CARAVELNETINTERFACE_H
