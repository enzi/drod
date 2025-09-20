// $Id$

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
#include <json/json.h>

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
	virtual UINT DownloadRecords(const CNetRoom& room, const bool bSendIfConquered);
	virtual UINT DownloadStyle(const WCHAR* styleName);
	virtual UINT HasRegisteredAccess();
	virtual void MatchCNetHolds();
	virtual UINT RateHold(const WCHAR* holdName, const float fDifficulty,
			const float fOverall);
	virtual UINT RateMod(const WCHAR* name, const float fRating);
	virtual UINT RequestNewKey(const string& strUser);
	virtual UINT SendChatText(const SendData& text, const int lastID, const UINT roomID);
	virtual UINT SendChatText(const vector<SendData>& texts, const int lastID, const UINT roomID);
	virtual UINT UploadChallengeDemo(const CNetRoom& room, const string& demo);
	virtual UINT UploadDemo(const CNetRoom& room, const WSTRING& playerName, const string& demo,
			const UINT wMoveCount, const UINT dwTimeElapsed, const UINT dwCreatedTime);
	virtual UINT UploadDemos(const string& text);
	virtual bool UploadExploredRooms(const string& buffer);
	virtual bool UploadExploredRoom(const CNetRoom& room);

	// Cloud stuff
	virtual UINT CloudUploadData(CloudDataType type, const CNetRoom& room, const int version, const std::string& data);
	virtual UINT CloudUploadProgress(const UINT dwHoldID);
	virtual UINT CloudDownloadProgress(const UINT dwHoldID);
	virtual UINT CloudUploadMyHold(const UINT dwHoldId);
	virtual UINT CloudUploadGeneralHold(const UINT dwHoldId);
	virtual UINT CloudDownloadGeneralHold(const Json::Value&);
	virtual UINT CloudSetHoldInstalled(const UINT dwHoldId, bool bInstalled);

	virtual UINT CloudDownloadHold(/*id*/) {return true;}
	virtual UINT CloudInitialize();
	virtual UINT CloudGetPlayer(const WCHAR* pUsername = NULL, const WCHAR* pKey = NULL);
	virtual UINT CloudQueueSize() const;

	virtual bool IsHoldInCloudQueue(const UINT holdID) const;
	//

	virtual CNetResult* GetResults(const UINT handle);
	virtual int  GetStatus(const UINT handle);

	virtual vector<CNetMedia*>& GetCNetMedia();
	virtual CIDSet& GetBetaHolds();
	virtual CIDSet& GetLocalHolds();
	virtual CIDSet& GetUpdatedHolds();
	virtual bool IsLoggedIn();

	virtual void SetLastNotice(const UINT dwNoticeID) { this->dwLastNotice = dwNoticeID; }
	virtual void QueryNotices(vector<CNetNotice> &notices, UINT typeMask, UINT lastId) const;

	static UINT getRequest(const string& url, CPostData* pPost);
	static UINT getRequest(const string& url, Json::Value& json);

protected:
	void AddHoldsOnCNet();
	void ClearResults();
	static string getCompressedBuffer(const string& text);
	static string getCompressedEncodedBuffer(const string& text);
	virtual bool SetCredentials(Json::Value& json, bool bVerify=true);
	virtual bool SetCredentials(Json::Value& json, const string& strUser, const string& strKey, bool bVerify = true);
	virtual bool VerifyKeyFormat(const string& strKey);
	void WaitUntilHoldsReceived();
	void ParseNotices(const Json::Value& root);
	void ParseVersion(const Json::Value& root);
	void AddToCloudQueue(const Json::Value& json);

	//Set of holds.
	HOLD::HOLDTagType	ParseTagField(const char *str) const;
	Json::Value			CNetHoldBuffer;
	UINT wHoldHandle;	//when awaiting set of CaravelNet holds
	UINT dwLastNotice;
	
	UINT dwDownloadHoldId;

	std::map<UINT, CNetResult*> results;	//not examined yet
	std::map<UINT, CNetNotice> notices;		//not displayed yet
	std::map<UINT, UINT> serverHoldIdToListboxId;

	UINT GetCloudVersionID(const char* settingsKey, const UINT id) const;
	bool SetCloudVersionID(const char* settingsKey, const UINT id, const UINT version) const;
	void SetDownloadHold(const UINT dwServerHoldId);
	UINT GetDownloadHold() const;

private:
	void CloudIdle();
};

#endif //...#ifndef CARAVELNETINTERFACE_H
