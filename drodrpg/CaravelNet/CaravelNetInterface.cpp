// $Id: CaravelNetInterface.cpp 8019 2007-07-14 22:30:11Z trick $

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

//CaravelNetInterface.cpp.
//Implementation of CCaravelNetInterface

#include "CaravelNetInterface.h"
#include "md5.h"
#include "../DRODLib/Db.h"
#include "../DRODLib/DbPlayers.h"
#include "../DRODLib/DbXML.h"
#include "../DRODLib/GameConstants.h"
#include "../DRODLib/PlayerStats.h"
#include "../DRODLib/DbSavedGames.h"
#include <BackEndLib/Assert.h>
#include <BackEndLib/Base64.h>
#include <BackEndLib/Files.h>
#include <BackEndLib/Ports.h>
#include <SDL.h>

#include <stdio.h>

#ifdef WIN32 
#	define snprintf _snprintf
#endif

const UINT cMD5_HASH_SIZE = 32;
const UINT cBUF_SIZE = 256;

const string cNetStr = "http://forum.caravelgames.com/drodrpg12.php";

//
//Public methods.
//

//*****************************************************************************
CCaravelNetInterface::CCaravelNetInterface()
	: CNetInterface()
	, wHoldHandle(0)
{
}

//*****************************************************************************
CCaravelNetInterface::~CCaravelNetInterface()
{
	// Finish up any pending CaravelNet requests, so as to get a
	// valid Key for the next time the app is run.
	ClearActiveAction();

	ClearCNetHolds();
	ClearResults();
}

//*****************************************************************************
bool CCaravelNetInterface::Busy() const
//Returns: true if waiting for a transaction to finish
{
	if (CCaravelNetInterface::wCurrentHandle && CInternet::GetStatus(CCaravelNetInterface::wCurrentHandle) < 0)
		return true;
	if (this->wHoldHandle && CInternet::GetStatus(this->wHoldHandle) < 0)
		return true;

	return false; //not busy
}

//*****************************************************************************
void CCaravelNetInterface::ClearActiveAction()
//Waits for any outstanding query to complete so new key can be received
//before submitting another query.  Store results.
{
	if (CCaravelNetInterface::wCurrentHandle)
	{
		//Process any special handles that are waiting.
		if (this->wHoldHandle)
		{
			WaitUntilHoldsReceived();
			return;
		}

		//Hold onto this value, because it gets reset by GetResults below.
		const UINT handle = CCaravelNetInterface::wCurrentHandle;

		while (CInternet::GetStatus(handle) < 0)
			SDL_Delay(20);	//wait until response

		//Save results for later lookup.
		CStretchyBuffer* pBuf = GetResults(handle);
		this->results[handle] = pBuf;
	}
}

//*****************************************************************************
void CCaravelNetInterface::Enable(const bool bVal) //[default=true]
{
	CNetInterface::Enable(bVal);

	if (!bVal)
	{
		//Cancel any outgoing requests.
		CCaravelNetInterface::wCurrentHandle = 0;
		this->wHoldHandle = 0;
		ASSERT(!Busy());
	}
}

//*****************************************************************************
string CCaravelNetInterface::GetChecksum(
//Returns: a checksum string for the specified data
//
//Params:
	CDbSavedGame *pSavedGame, //record to generate a key for
	const UINT val)  //type of key generation behavior
const
{
#define CS_RENEW    (0) //if a key exists, refresh it -- otherwise reset it
#define CS_GENERATE (1) //make a key for a (presumably) new game state
#define appendNumStr(i) text += _itoa((i), temp, 10); text += ' '

	ASSERT(pSavedGame);
	if (!pSavedGame) //robustness check
		return string();

	//If no checksum currently exists for the record, then fail unless a
	//request to generate a new key was received.
	if (pSavedGame->checksumStr.size() != cMD5_HASH_SIZE && val != CS_GENERATE)
		return string();

	//Checksum salt (i.e. a hidden value added to the text).
	static const string salt = "GS9";

	//Text for which a checksum gets generated
	string text = salt;

	//Serialize the game state.
	PlayerStats st;
	st.Unpack(pSavedGame->stats);

	//If the data stored in a saved game record is from an older game version,
	//we want to generate the format of text string used in that older version.
	char temp[12];
	switch (pSavedGame->wVersionNo)
	{
		//Add new serialization info here for future game version info, if needed.
		case 500:
			appendNumStr(st.shovels);
			appendNumStr(st.scoreHP);
			appendNumStr(st.scoreATK);
			appendNumStr(st.scoreDEF);
			appendNumStr(st.scoreGOLD);
			appendNumStr(st.scoreXP);
			appendNumStr(st.scoreYellowKeys);
			appendNumStr(st.scoreGreenKeys);
			appendNumStr(st.scoreBlueKeys);
			appendNumStr(st.scoreSkeletonKeys);
			appendNumStr(st.scoreShovels);
			appendNumStr(st.beamVal);
			appendNumStr(st.firetrapVal);
			appendNumStr(st.queenSpawnID);
			appendNumStr(st.mudSpawnID);
			appendNumStr(st.tarSpawnID);
			appendNumStr(st.gelSpawnID);
			appendNumStr(st.mudSwapID);
			appendNumStr(st.tarSwapID);
			appendNumStr(st.gelSwapID);
		//no break
		case 404: case 405:
			appendNumStr(st.HP);
			appendNumStr(st.ATK);
			appendNumStr(st.DEF);
			appendNumStr(st.GOLD);
			appendNumStr(st.XP);
			appendNumStr(st.speed);
			appendNumStr(st.yellowKeys);
			appendNumStr(st.greenKeys);
			appendNumStr(st.blueKeys);
			appendNumStr(st.skeletonKeys);
			appendNumStr(st.sword);
			appendNumStr(st.shield);
			appendNumStr(st.accessory);
			appendNumStr(st.totalMoves);
			appendNumStr(st.totalTime);
			appendNumStr(st.monsterHPmult);
			appendNumStr(st.monsterATKmult);
			appendNumStr(st.monsterDEFmult);
			appendNumStr(st.monsterGRmult);
			appendNumStr(st.monsterXPmult);
			appendNumStr(st.itemMult);
			appendNumStr(st.itemHPmult);
			appendNumStr(st.itemATKmult);
			appendNumStr(st.itemDEFmult);
			appendNumStr(st.itemGRmult);
			appendNumStr(st.hotTileVal);
			appendNumStr(st.explosionVal);
		break;

		default: return string(); //game version is invalid
	}

	text += salt;

	string checksum = MD5::getChecksum(text);
	return checksum;

#undef CS_RENEW
#undef CS_GENERATE
#undef appendNumStr
}

//*****************************************************************************
bool CCaravelNetInterface::VerifyChecksum(CDbSavedGame *pSavedGame, const UINT val) const
//Returns: whether the checksum string for the specified data is verified as accurate
{
	ASSERT(pSavedGame);
	if (!pSavedGame) //robustness check
		return false;

	//Does the checksum string currently in this saved game record match
	//the checksum generated for this saved game's state?
	//If not, there's a bug, data error, or someone tampered with the record.
	//In any event, we can't accept the veracity of this record.
	const string str = GetChecksum(pSavedGame, val);
	return pSavedGame->checksumStr.compare(str) == 0;
}

//*****************************************************************************
int CCaravelNetInterface::GetStatus(
//Returns: Error code returned if handle response received (0 indicating success),
// else -1 if response not received
//
//Params:
	const UINT handle) // (in) handle of the download to query the status of
{
	//Check whether thread has already completed and results were stored.
	std::map<UINT, CStretchyBuffer*>::iterator iter = this->results.find(handle);
	if (iter != this->results.end())
		return CURLE_OK;	//assume result was received properly if it's in 'results'

	if (!IsEnabled())
		return CURLE_COULDNT_CONNECT; //don't cause indefinite waiting for response if disabled

	return CInternet::GetStatus(handle);
}

//*****************************************************************************
bool CCaravelNetInterface::ChatLogout()
//Sends a chat logout notification from this user.
//
//Returns: always 0 since response is not required and will be ignored
{
	CPostData* pPost;
	if (!(pPost = CCaravelNetInterface::SetCredentials())) return false;

	const string url = cNetStr + "?action=chatlogout";
	return CInternet::HttpGet(url, NULL, pPost); //ignore response from server (no handle returned)
}

//*****************************************************************************
UINT CCaravelNetInterface::DownloadDemo(
//Send a request to the server to download a specific demo
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
	const long demoID) // (in)  The id of the demo to download
{
	ClearActiveAction();

	CPostData* pPost;
	if (!(pPost = CCaravelNetInterface::SetCredentials())) return false;

	char cSizeBuf[cBUF_SIZE];
	string url = cNetStr + "?action=dldemo&id=";
	url += writeInt32(cSizeBuf, sizeof(cSizeBuf), (int)demoID);

	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, &handle, pPost);
	return CCaravelNetInterface::wCurrentHandle = handle;
}

//*****************************************************************************
UINT CCaravelNetInterface::DownloadHold(
//Send a request to CaravelNet to download a specific hold
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
   const long holdID) // (in)  The id of the hold to download
{
	ClearActiveAction();
	CPostData* pPost;
	if (!(pPost = CCaravelNetInterface::SetCredentials())) return false;

	char cSizeBuf[cBUF_SIZE];
	string url = cNetStr + "?action=dlhold&id=";
	url += writeInt32(cSizeBuf, sizeof(cSizeBuf), (int)holdID);

	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, &handle, pPost);
	return CCaravelNetInterface::wCurrentHandle = handle;
}

//*****************************************************************************
UINT CCaravelNetInterface::DownloadStyle(
//Send a request to CaravelNet to download a specific style
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
   const WCHAR* styleName) // (in)  The name of the style to download
{
	ClearActiveAction();
	CPostData* pPost;
	if (!(pPost = CCaravelNetInterface::SetCredentials())) return false;

	string url = cNetStr + "?action=dlStyle";
	string b64 = Base64::encode((BYTE*)styleName, WCSlen(styleName)*sizeof(WCHAR));
	ASSERT(unsigned(b64.length()) == b64.length());
	pPost->Add("StyleName", b64);
	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, &handle, pPost);
	return CCaravelNetInterface::wCurrentHandle = handle;
}

//*****************************************************************************
UINT CCaravelNetInterface::DownloadHoldList()
//Send a request to CaravelNet for a list of available holds
//
//Returns: A handle to use to get the response, or 0 on failure.
{
	ClearCNetHolds();

	//Only query the hold list for "real" players.
	if (!g_pTheDB->Players.IsLocal(g_pTheDB->GetPlayerID())) return 0;

	if (this->wHoldHandle) return 0; //still waiting on a pending hold list request

	ClearActiveAction();
	CPostData* pPost;
	if (!(pPost = CCaravelNetInterface::SetCredentials())) return false;

	const string url = cNetStr + "?action=getholdlist";

	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, &handle, pPost);
	return CCaravelNetInterface::wCurrentHandle = this->wHoldHandle = handle;
}

//*****************************************************************************
UINT CCaravelNetInterface::DownloadRecords(
//Send a request to CaravelNet for a list of records for the specified room(s)
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
	const string& buffer,  //(in) XML data representing the room records are requested for
	const bool bSendIfConquered) //if true, tells server to only send records for
	                             //each room in question if it has been marked as
										  //conquered on the server end
{
	ClearActiveAction();
	CPostData* pPost;
	if (!(pPost = CCaravelNetInterface::SetCredentials())) return false;

	string url = cNetStr + "?action=getrecords";
	pPost->Add("RoomData", buffer);
	if (bSendIfConquered)
		pPost->Add("SendIfConquered", "1");

	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, &handle, pPost);
	return CCaravelNetInterface::wCurrentHandle = handle;
}

//*****************************************************************************
UINT CCaravelNetInterface::HasRegisteredAccess()
//Query CaravelNet to see if the username and key are valid.
//This will receive a new key from the server for use in the next query.
//
//Returns: A handle to be used to retrieve the results
{
	ClearActiveAction();
	CPostData* pPost;
	if (!(pPost = CCaravelNetInterface::SetCredentials())) return false;

	string url = cNetStr + "?action=login";
	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, &handle, pPost);
	return CCaravelNetInterface::wCurrentHandle = handle;
}

//*****************************************************************************
UINT CCaravelNetInterface::SendChatText(const SendData& text, const int lastID, const UINT roomID)
//Returns: A handle to use to get the response, or 0 on failure.
{
	vector<SendData> texts;
	texts.push_back(text);
	return SendChatText(texts, lastID, roomID);
}

//*****************************************************************************
UINT CCaravelNetInterface::SendChatText(const vector<SendData>& texts, const int lastID, const UINT roomID)
//Returns: A handle to use to get the response, or 0 on failure.
{
	ClearActiveAction();
	CPostData* pPost;
	if (!(pPost = CCaravelNetInterface::SetCredentials())) return false;

	char cSizeBuf[cBUF_SIZE];
	string url = cNetStr + "?action=";
	url += roomID ? "roomchat" : "chat"; //room vs global chat
	pPost->Add("LastID", _itoa(lastID, cSizeBuf, 10));
	for (UINT i=0; i<texts.size(); ++i)
	{
		const SendData& data = texts[i];
		if (data.text.length()) //don't upload empty texts
		{
			string indexStr = "[";
			indexStr += _itoa(i, cSizeBuf, 10);
			indexStr += "]";
			const string b64 = Base64::encode(data.text);
			ASSERT(unsigned(b64.length()) == b64.length());
			string field = "Text";
			field += indexStr;
			pPost->Add(field.c_str(), b64);

			for (CIDSet::const_iterator id=data.userIDs.begin(); id != data.userIDs.end(); ++id)
			{
				string field = "UserID";
				field += indexStr;
				field += "[]";
				pPost->Add(field.c_str(), _itoa(*id, cSizeBuf, 10));
			}
		}
	}
	if (roomID)
	{
		const CIDSet roomIDs(roomID);
		CDbRefs dbRefs(V_Rooms, roomIDs);
		string buffer = CDbXML::getXMLheader();
		g_pTheDB->Rooms.ExportXML(roomID, dbRefs, buffer, true);
		buffer += CDbXML::getXMLfooter();
		pPost->Add("RoomData", buffer);
	}

	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, &handle, pPost);
	return CCaravelNetInterface::wCurrentHandle = handle;
}

//*****************************************************************************
/*
UINT CCaravelNetInterface::UploadDemo(
//Send a buffer to CaravelNet of a recorded demo.
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
	const string& text,
	const UINT wMoveCount, const UINT dwTimeElapsed)	//in 10ms increments
{
	ClearActiveAction();
	if (!CCaravelNetInterface::SetCookies()) return 0;

   CStretchyBuffer buffer(text);

   BYTE* pCompressed = NULL;
   ULONG pCompressedSize=0;
   buffer.Compress(pCompressed, pCompressedSize);

   buffer.Set(pCompressed, pCompressedSize);
	delete[] pCompressed;
   buffer.Encode();
   string b64 = Base64::encode((BYTE*)buffer, buffer.Size());

   string url = cNetStr + "?action=UploadDemo";
   char cSizeBuf[1024];
   ASSERT(unsigned(b64.length()) == b64.length());
   sprintf(cSizeBuf, "%u", unsigned(b64.length()));
   CInternet::AddPostData("Demo", b64);
   CInternet::AddPostData("Size", cSizeBuf);
   sprintf(cSizeBuf, "%u", wMoveCount);
   CInternet::AddPostData("MoveCount", cSizeBuf);
   sprintf(cSizeBuf, "%u", (UINT)dwTimeElapsed);
   CInternet::AddPostData("RealTime", cSizeBuf);
	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, &handle);
	return CCaravelNetInterface::wCurrentHandle = handle;
}

/-*****************************************************************************
bool CCaravelNetInterface::UploadDemos(
//Send a buffer to CaravelNet of one or more recorded demos.
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
	const string& text)
{
	ClearActiveAction();
	if (!CCaravelNetInterface::SetCookies()) return 0;

   CStretchyBuffer buffer(text);

   BYTE* pCompressed = NULL;
   ULONG pCompressedSize=0;
   buffer.Compress(pCompressed, pCompressedSize);

   string b64 = Base64::encode((BYTE*)pCompressed, pCompressedSize);
	delete[] pCompressed;

   string url = cNetStr + "?action=UploadDemos";
   char cSizeBuf[1024];
   ASSERT(unsigned(b64.length()) == b64.length());
   sprintf(cSizeBuf, "%u", unsigned(b64.length()));
   CInternet::AddPostData("Demos", b64);
   CInternet::AddPostData("Size", cSizeBuf);
	return CInternet::HttpGet(url);
}
*/

//*****************************************************************************
bool CCaravelNetInterface::UploadExploredRooms(const string& buffer)
//Send a buffer to CaravelNet of a player's set of explored+conquered rooms (in a saved game).
//
//Returns: A handle to use to get the response, or 0 on failure.
{
	ClearActiveAction();
	CPostData* pPost;
	if (!(pPost = CCaravelNetInterface::SetCredentials())) return false;

	CStretchyBuffer textBuffer(buffer);

	BYTE* pCompressed = NULL;
	ULONG pCompressedSize=0;
	textBuffer.Compress(pCompressed, pCompressedSize);

	string b64 = Base64::encode((BYTE*)pCompressed, pCompressedSize);
	delete[] pCompressed;

	const string url = cNetStr + "?action=UploadSavedGames";
	ASSERT(unsigned(b64.length()) == b64.length());
	pPost->Add("Player", b64);
	return CInternet::HttpGet(url, NULL, pPost);
}

//*****************************************************************************
UINT CCaravelNetInterface::UploadScore(const string& savedGameXML, const WSTRING& name, const UINT score)
//Uploads the user's saved game and score for a named score checkpoint within the specified hold.
{
	ClearActiveAction();
	CPostData* pPost;
	if (!(pPost = CCaravelNetInterface::SetCredentials())) return false;

	//Score.
	char cSizeBuf[cBUF_SIZE];
	string url = cNetStr + "?action=UploadScore&score=";
	url += writeInt32(cSizeBuf, sizeof(cSizeBuf), score);

	//Compress and encode saved game XML.
	CStretchyBuffer textBuffer(savedGameXML);
	BYTE* pCompressed = NULL;
	ULONG pCompressedSize=0;
	textBuffer.Compress(pCompressed, pCompressedSize);

	string b64 = Base64::encode((BYTE*)pCompressed, pCompressedSize);
	delete[] pCompressed;

	ASSERT(unsigned(b64.length()) == b64.length());
	pPost->Add("SavedGame", b64);
	sprintf(cSizeBuf, "%u", unsigned(b64.length()));
	pPost->Add("Size", cSizeBuf);

	//Encode score checkpoint name.
	b64 = Base64::encode((const BYTE*)name.c_str(), name.size());
	ASSERT(unsigned(b64.length()) == b64.length());
	pPost->Add("ScoreName", b64);

	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, &handle, pPost);
	return CCaravelNetInterface::wCurrentHandle = handle;
}

//*****************************************************************************
UINT CCaravelNetInterface::RateHold(
//Rates the hold with specified ID.
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
	const WCHAR* holdName, const float fDifficulty, const float fOverall)
{
	ClearActiveAction();
	CPostData* pPost;
	if (!(pPost = CCaravelNetInterface::SetCredentials())) return false;

   string b64 = Base64::encode((BYTE*)holdName, WCSlen(holdName)*sizeof(WCHAR));

	string url = cNetStr + "?action=RateHold";
   char cSizeBuf[cBUF_SIZE];
	ASSERT(unsigned(b64.length()) == b64.length());
	pPost->Add("HoldName", b64);
   sprintf(cSizeBuf, "%f", fDifficulty);
   pPost->Add("Difficulty", cSizeBuf);
   sprintf(cSizeBuf, "%f", fOverall);
   pPost->Add("Overall", cSizeBuf);

	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, &handle, pPost);
	return CCaravelNetInterface::wCurrentHandle = handle;
}

//*****************************************************************************
UINT CCaravelNetInterface::RateMod(
//Rates the mod with specified name.
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
	const WCHAR* name, const float fRating)
{
	ClearActiveAction();
	CPostData* pPost;
	if (!(pPost = CCaravelNetInterface::SetCredentials())) return false;

   string b64 = Base64::encode((BYTE*)name, WCSlen(name)*sizeof(WCHAR));

   char cSizeBuf[cBUF_SIZE];
	string url = cNetStr + "?action=RateMod";
	pPost->Add("Name", b64);
   sprintf(cSizeBuf, "%f", fRating);
   pPost->Add("Rating", cSizeBuf);

	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, &handle, pPost);
	return CCaravelNetInterface::wCurrentHandle = handle;
}

//*****************************************************************************
UINT CCaravelNetInterface::RequestNewKey(const string& strUser)
//Send a request to CaravelNet to send the user a new key
//
//Returns: A handle to use to get the response, or 0 on failure.
{
	ClearActiveAction();
	if (!IsEnabled())
		return 0; //don't request a key when disabled

	CCaravelNetInterface::SetCookies(strUser, "", false);
	CPostData* pPost = CCaravelNetInterface::SetCredentials(strUser, "", false);

	const string url = cNetStr + "?action=reqnewkey";

	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, &handle, pPost);
	return CCaravelNetInterface::wCurrentHandle = handle;
}


//*****************************************************************************
CStretchyBuffer* CCaravelNetInterface::GetResults(
//Retrieve the buffer that corresponds to the given handle
//
//Returns: A buffer sent as response to the specified request, or NULL on failure.
//
//Params:
	const UINT handle) // (in) handle of the download to get the results of
{
	//Check for cached result first.
	CStretchyBuffer *pBuffer;
	std::map<UINT, CStretchyBuffer*>::iterator iter = this->results.find(handle);
	if (iter != this->results.end())
	{
		pBuffer = iter->second;
		this->results.erase(iter);
		return pBuffer;
	}

	//Get query results.
	const int status = GetStatus(handle);
	pBuffer = CInternet::GetResults(handle);
	CCaravelNetInterface::wCurrentHandle = 0;

	//If there was a receiving error, mention it,
	//but retrieve new key if possible from received buffer before failing.
	if (status > 0)
		CInternet::OutputError(handle);

	//Check for bad received buffer.
	if (!pBuffer || pBuffer->Size() < cMD5_HASH_SIZE) {
		delete pBuffer;
		return NULL;
	}

	//Save the new password into the current user's profile.
	CDbPlayer *pPlayer = g_pTheDB->GetCurrentPlayer();
	if (!pPlayer)
	{
		ASSERT(!"ERROR - Key lost?  GetResults was called with no current player.");
	} else {
		WSTRING wstrKey;
		char newKey[cMD5_HASH_SIZE+1];

		memcpy(newKey, (BYTE*)*pBuffer, cMD5_HASH_SIZE);
		newKey[cMD5_HASH_SIZE] = 0;

		if (VerifyKeyFormat(newKey))
		{
			AsciiToUnicode(newKey, wstrKey);
			pPlayer->CNetPasswordText = wstrKey.c_str();
			pPlayer->Update();
		}
		delete pPlayer;

		pBuffer->RemoveBytes(0, cMD5_HASH_SIZE);
	}

	//Now that the new key has been retrieved, we can fail if there was an error.
	if (status > 0)
	{
		delete pBuffer;
		return NULL;
	}

	return pBuffer;
}

//*****************************************************************************
void CCaravelNetInterface::MatchCNetHolds()
//Mark which CNet holds are installed locally and also
//those that have a newer version online.
{
	this->betaHolds.clear();
	this->localHolds.clear();
	this->updatedHolds.clear();

	CDbPlayer *pPlayer = g_pTheDB->GetCurrentPlayer();

	for (UINT wIndex=this->cNetMedia.size(); wIndex--; )
	{
		CNetMedia& holdData = *(this->cNetMedia[wIndex]);

		//Is the current player the CaravelNet author of this item?
		if (pPlayer && !WCScmp((const WCHAR*)pPlayer->CNetNameText, (const WCHAR*)holdData.CNetNameText))
			holdData.bPlayerIsAuthor = true;

		//Only consider holds.
		if (holdData.mediaType != MT_Hold)
			continue;

		const UINT dwHoldID = g_pTheDB->Holds.GetHoldID(holdData.Created,
				holdData.HoldNameText, holdData.OrigAuthorText);
		holdData.localHoldID = dwHoldID;
		if (!dwHoldID) continue;

		CDbHold *pLocalHold = g_pTheDB->Holds.GetByID(dwHoldID, true);
		ASSERT(pLocalHold);

		//Holds only match if they are the exact same version,
		//or if local hold is a newer version of a CaravelNet hold in beta.
		if (holdData.LastModified == pLocalHold->LastUpdated ||
				(holdData.LastModified < pLocalHold->LastUpdated && holdData.bBeta))
			this->localHolds += dwHoldID;

		//Compare timestamps to decide which hold is newer.
		//If forum hold is newer, mark it as updated.
		if (holdData.LastModified > pLocalHold->LastUpdated)
			this->updatedHolds += dwHoldID;

		//Track holds in beta.
		if (holdData.bBeta)
			this->betaHolds += dwHoldID;

		delete pLocalHold;
	}

	delete pPlayer;
}

//*****************************************************************************
//Ensure data received are current before returning anything.
vector<CNetMedia*>& CCaravelNetInterface::GetCNetMedia()
{
	WaitUntilHoldsReceived();
	return CNetInterface::GetCNetMedia();
}

CIDSet& CCaravelNetInterface::GetBetaHolds()
{
	WaitUntilHoldsReceived();
	return CNetInterface::GetBetaHolds();
}

CIDSet& CCaravelNetInterface::GetLocalHolds()
{
	WaitUntilHoldsReceived();
	return CNetInterface::GetLocalHolds();
}

CIDSet& CCaravelNetInterface::GetUpdatedHolds()
{
	WaitUntilHoldsReceived();
	return CNetInterface::GetUpdatedHolds();
}

//*****************************************************************************
bool CCaravelNetInterface::IsLoggedIn()
//Returns: Whether user is logged in to CaravelNet
{
	const vector<CNetMedia*>& media = GetCNetMedia();
	return !media.empty();
}

//
// Protected methods.
//

//*****************************************************************************
//
//Expat callback entrypoints (private)
//

void HoldStartElement_cb (
	void *pObject, // (in) Pointer to caller object
	const XML_Char *name, const XML_Char **atts)
{
	((CCaravelNetInterface*)pObject)->StartElement(name, atts);
}

void HoldInElement_cb (
	void *pObject, // (in) Pointer to caller object
	const XML_Char* s, int len)
{
	((CCaravelNetInterface*)pObject)->InElement(s, len);
}

void HoldEndElement_cb (
	void *pObject, // (in) Pointer to caller object
	const XML_Char* name)
{
	((CCaravelNetInterface*)pObject)->EndElement(name);
}

//*****************************************************************************
void CCaravelNetInterface::AddHoldsOnCNet()
//Adds holds published on CaravelNet to holds list box.
{
	ClearCNetHolds();

	if (this->CNetHoldBuffer.Size() <= 10) return;	//invalid buffer

	//Parser init.
	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, this);
	XML_SetElementHandler(parser, HoldStartElement_cb, HoldEndElement_cb);
	XML_SetCharacterDataHandler(parser, HoldInElement_cb);

	// Add the size of the hash preceding the XML
	char *buf = ((char*)(BYTE*)this->CNetHoldBuffer);
	const UINT size = this->CNetHoldBuffer.Size();

	//Parse the XML.
	if (XML_Parse(parser, buf, size, true) == XML_STATUS_ERROR)
	{
		//Some problem occurred.
		char errorStr[256];
		_snprintf(errorStr, 256,
				"Holds Parse Error: %s at line %u\n",
				XML_ErrorString(XML_GetErrorCode(parser)),
				(unsigned int)XML_GetCurrentLineNumber(parser));
		CFiles Files;
		Files.AppendErrorLog((char *)errorStr);
		Files.AppendErrorLog(buf);
	}
	XML_ParserFree(parser);

	MatchCNetHolds();
}

//*****************************************************************************
void CCaravelNetInterface::ClearResults()
{
	for (std::map<UINT, CStretchyBuffer*>::iterator iter = results.begin();
		iter != results.end(); ++iter)
	{
		delete iter->second;
	}
	results.clear();
}

//*****************************************************************************
void CCaravelNetInterface::WaitUntilHoldsReceived()
//When requested, wait until hold list is received.
{
	if (!this->wHoldHandle) return;

	while (CInternet::GetStatus(this->wHoldHandle) < 0)
		SDL_Delay(20);

	CStretchyBuffer* pBuffer = GetResults(this->wHoldHandle);
	this->wHoldHandle = 0;
	if (pBuffer)
	{
		this->CNetHoldBuffer = *pBuffer;
		delete pBuffer;
		AddHoldsOnCNet();
	}
	// else the call failed.  Don't try again.
}

//*****************************************************************************
CPostData* CCaravelNetInterface::SetCredentials(bool bVerify)
//Set the cookies to be the current user's name and key
//
//Returns: false if there is no current player, if the player's key is not
// in a valid format, of if the player turned off CaravelNet connectivity
{
	if (!IsEnabled())
		return NULL; //don't allow initing any requests when disabled

	CDbPlayer *pPlayer = g_pTheDB->GetCurrentPlayer();
	if (!pPlayer)
		return NULL;

	//Only allow if player has Internet connectivity set.
	if (pPlayer->Settings.GetVar(useInternetStr, false) == false)
	{
		delete pPlayer;
		return NULL;
	}

	const string strName = UnicodeToUTF8((const WCHAR*)pPlayer->CNetNameText);
	const string strKey = UnicodeToUTF8((const WCHAR*)pPlayer->CNetPasswordText);
	delete pPlayer;

	return SetCredentials(strName, strKey, bVerify);
}

//*****************************************************************************
CPostData* CCaravelNetInterface::SetCredentials(const string& strUser, const string& strKey, bool bVerify)
{
	if (bVerify && !VerifyKeyFormat(strKey)) return NULL;

	CPostData* pPostData = new CPostData();
	pPostData->Add("UserInfoCookie[UserName]", strUser.c_str());
	pPostData->Add("UserInfoCookie[Key]", strKey.c_str());

	// Also add the version, as every single request should do this
	string str;
	UnicodeToUTF8(wszVersionReleaseNumber, str);
	pPostData->Add("Version", str.c_str());

	// Also add the specific hold data version
	pPostData->Add("HoldVersion", std::to_string(VERSION_NUMBER));

#ifdef STEAMBUILD
	pPostData->Add("steam", "1");
#endif

	return pPostData;
}


//*****************************************************************************
bool CCaravelNetInterface::VerifyKeyFormat(
//Checks a key to make sure it's in the correct format.
//
//Returns: true if it's valid, false otherwise
//
//Params:
	const string& strKey) // (in)  The key to check
{
	// If the key isn't the right size, it's not valid.
	if (strKey.length() != cMD5_HASH_SIZE)
		return false;
	for (string::const_iterator iter = strKey.begin(); iter != strKey.end(); iter++) {
		// can only contain alphanumeric characters.
		if (!isalnum(*iter))
			return false;
	}
	return true;
}

//*****************************************************************************
//
// Expat callback functions
//

using namespace HOLD;

//*****************************************************************************
HOLDTagType CCaravelNetInterface::ParseTagField(const char *str)
//Returns: enumeration corresponding to tag name
const
{
	static const char * tagStr[Tag_Count] = {
		"Version", "GID_Created", "Author", "ForumName", "HoldId",
		"LastUpdated", "Difficulty", "Rating", "NumVotes", "FileSize", "FileSizeBytes",
		"NameMessageB64", "OriginalAuthorB64", "Beta",
		"MyDifficulty", "MyRating", "Media", "Thumbnail", "ThumbnailURL",
		"Status"
	};

	for (int eTag=First_Tag; eTag<Tag_Count; ++eTag)
		if (!stricmp(str, tagStr[eTag])) //not case sensitive
			return (HOLDTagType)eTag;

	return Unknown_Tag;
}

//*****************************************************************************
void CCaravelNetInterface::StartElement(
//Expat callback function: Process XML start tag, and attributes.
//Parses the information for one hold on the forum each iteration.
//
//Params:
	const XML_Char *name, const XML_Char **atts)
{
	//Ensure the only tags handled are DROD hold data.
	if (!stricmp(name, "DROD")) return;
	if (stricmp(name, "Holds")) return;   //not case sensitive

	CNetMedia *pDatum = new CNetMedia;

	//Parse hold tag fields.
	int i;
	char *str;
	for (i = 0; atts[i]; i += 2) {
		const HOLDTagType pType = ParseTagField(atts[i]);
		if (pType == Unknown_Tag)
		{
			//Ignore unknown tag fields.
			continue;
		}
		str = (char* const)atts[i + 1];
		switch (pType)
		{
			case Version:
				pDatum->wVersion = convertToUINT(str);
			break;
			case GID_Created:
				pDatum->Created = convertToTimeT(str);
			break;
			case Author:
			{
				WSTRING data;
				string asciiStr;
				Base64::decode(str, asciiStr);
				AsciiToUnicode(asciiStr.c_str(), data);
				pDatum->AuthorText = data.c_str();
			}
			break;
			case CNetName:
			{
				WSTRING data;
				string asciiStr;
				Base64::decode(str, asciiStr);
				AsciiToUnicode(asciiStr.c_str(), data);
				pDatum->CNetNameText = data.c_str();
			}
			break;
			case HoldId:
				pDatum->lHoldID = convertToInt(str);
			break;
			case LastUpdated:
				pDatum->LastModified = convertToTimeT(str);
			break;
			case Difficulty:
				pDatum->difficulty = str;
			break;
			case Rating:
				pDatum->rating = str;
			break;
			case NumVotes:
				pDatum->numVotes = str;
			break;
         case FileSize:
            pDatum->filesize = str;
			break;
         case FileSizeBytes:
            pDatum->filesizebytes = str;
			break;
			case NameMessageB64:  //overrides NameMessage
			{
				WSTRING data;
				Base64::decode(str, data);
				pDatum->HoldNameText = data.c_str();
			}
			break;
			case OriginalAuthorB64:  //replaces semantic use of Author
			{
				WSTRING data;
				Base64::decode(str, data);
				pDatum->OrigAuthorText = data.empty() ? (const WCHAR*)pDatum->AuthorText : data.c_str();
			}
			break;
			case Beta:
				pDatum->bBeta = atoi(str) != 0;
			break;
			case MyDifficulty:
				pDatum->myDifficulty = str;
			break;
			case MyRating:
				pDatum->myRating = str;
			break;
			case Media:
				pDatum->mediaType = MediaType(atoi(str));
			break;
			case Thumbnail:
			{
				const string sstr = str;
				BYTE *data;
				const UINT size = Base64::decode(sstr,data);
				pDatum->thumbnail.Set(data, size);
				delete[] data;
			}
			break;
			case ThumbnailURL:
				pDatum->thumbnailURL = str;
			break;
			case SmStatus:
				pDatum->status = convertToInt(str);
			break;

			default:
			break; //skip any unrecognized fields
		}
	}

	this->cNetMedia.push_back(pDatum);
}

//*****************************************************************************
void CCaravelNetInterface::InElement(
//Expat callback function: Process text between XML tags.
//
//Params:
	const XML_Char* /*s*/, int /*len*/)
{
	//not needed
}

//*****************************************************************************
void CCaravelNetInterface::EndElement(
//Expat callback function: Process XML end tag.
//
//Params:
	const XML_Char* /*name*/)
{
	//nothing to do here
}
