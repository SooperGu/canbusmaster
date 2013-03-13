/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file	CAN_i_VIEW/CAN_i_VIEW.cpp
 * \author	D Southworth
 * \copyright Copyright (c) 2013, Robert Bosch Automotive Service Solutions.
 *
 * Implementation of Ci_VIEW
 */

/* C++ includes */
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <array>

/* Project includes */
#include "CAN_i-VIEW_stdafx.h"
#include "CAN_i-VIEW.h"

#define USAGE_EXPORT
#include "CAN_i-VIEW_Extern.h"

using namespace std;

// CCAN_i_VIEW_DLL
BEGIN_MESSAGE_MAP(CCAN_i_VIEW_DLL, CWinApp)
END_MESSAGE_MAP()

/**
 * CCAN_i_VIEW_DLL construction
 */
CCAN_i_VIEW_DLL::CCAN_i_VIEW_DLL()
{
}

/**
 * The one and only CCAN_i_VIEW_DLL object
 */
CCAN_i_VIEW_DLL theDLL;

/**
 * CCAN_i_VIEW_DLL initialization
 */
BOOL CCAN_i_VIEW_DLL::InitInstance()
{
	CWinApp::InitInstance();
	return TRUE;
}

/**
 * Application Log
 */
static Base_WrapperErrorLogger*	g_pLog = NULL;

/***********************
 * VCI HW Class Members
 * An instance of this class defines a Channel
 */
UNUM32 VCI::m_NextId = 0;

/**
 * VCI HW Class Contructor.
 */
VCI::VCI(	const string&	Name,
		UNUM32		TypeId,
		UNUM32		CAN ) :
	m_Id(m_NextId++),	
	m_Name( Name ),
	m_VCiIF( NULL ),
	m_TypeId(TypeId),
	m_CAN(CAN),
	m_TxErrorCounter(0),
	m_RxErrorCounter(0),
	m_PeakRxErrorCounter(0),
	m_PeakTxErrorCounter(0)
{
	switch( TypeId){
	case MAN_T200:
		m_Firmware="MAN iView";
		break;
	case SPX_MIDRANGE:
		m_Firmware="SPX MidRange";
		break;
	case IVIEW_PRIMA:
		m_Firmware="iView Prima";
		break;
	default:
		m_Firmware="Unknown";
		break;
	}
}

/** Connect
 * \brief Connect to either CAN 0 or CAN 1
 * \return T_PDU_ERROR
 * \retval PDU_STATUS_NOERROR		Function call successful.
 * \retval PDU_ERR_FCT_FAILED		Function call failed.
 */
T_PDU_ERROR VCI::Connect()
{
	T_PDU_ERROR Err = PDU_ERR_FCT_FAILED;

	UNUM32 PinHi, PinLo;
	if( m_CAN==0 ){
		PinHi = 6;
		PinLo = 14;
	} else {
		PinHi = 3;
		PinLo = 11;
	}
	if( m_VCiIF ){
		Err = m_VCiIF->Connect( (UNUM32)VCI_PROTO_RAW,
			(UNUM32)VCI_PHYS_ISO11898_2, PinHi, PinLo,
			(UNUM32)VCI_TERM_NONE, m_Baudrate, (UNUM32)0 );
		if( Err == PDU_STATUS_NOERROR ){
			Err = m_VCiIF->IOCtl(VCI_FIOTXNOTIFY, VCI_TX_ECHO);
		}
		if( Err == PDU_STATUS_NOERROR ){
			Err = m_VCiIF->IOCtl(VCI_FIOTIMESTAMP, (UNUM32)0);
		}
	}
	return Err;
}

/***********************
 * Clients and Client Buffers Class Members
 */
DWORD Client::m_NextClientID = 0x200;

BOOL Client::BufferExists(
		CBaseCANBufFSE*	pBuf)
{
	if ((m_ClientBuf.size() > 0)
			&& (m_ClientBuf.find( pBuf ) != m_ClientBuf.end()) )
		return TRUE;
	return FALSE;
}

BOOL Client::AddClientBuffer(
		CBaseCANBufFSE*	pBuf)
{
	if (BufferExists(pBuf)){
		return FALSE;
	}
	m_ClientBuf.insert( pBuf );
	return TRUE;
}

void Client::WriteClientBuffers(
	STCANDATA& CanMsg )
{
	Buffers_t::iterator Itr = m_ClientBuf.begin();
	for (;Itr != m_ClientBuf.end(); Itr++){
		(*Itr)->WriteIntoBuffer(&CanMsg);
	}
}

BOOL Client::RemoveClientBuffer(
		CBaseCANBufFSE*	pBuf)
{
	if (BufferExists(pBuf)){
		m_ClientBuf.erase( pBuf );
		return TRUE;
	}
	return FALSE;
}

void Client::RemoveClientBuffers()
{
	m_ClientBuf.clear();
}

static CDIL_CAN_i_VIEW* g_DIL_CAN_i_VIEW = NULL;

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Returns the interface to controller
 */
USAGEMODE HRESULT GetIDIL_CAN_Controller(void** ppvInterface)
{
	HRESULT hResult = S_OK;

	if ( NULL == g_DIL_CAN_i_VIEW ){
		if ((g_DIL_CAN_i_VIEW = new CDIL_CAN_i_VIEW) == NULL){
			hResult = S_FALSE;
		}
	}
	*ppvInterface = (void*)g_DIL_CAN_i_VIEW;
	return hResult;
}

/***********************
 * CDIL_CAN_i_VIEW Class Members
 */

CDIL_CAN_i_VIEW::CDIL_CAN_i_VIEW() :
	m_CreateCCommTCP(NULL),
	m_CreateCVCiViewIF(NULL),
	m_hDll( NULL ),
	m_CurrState(STATE_DRIVER_SELECTED),
	m_hOwnerWnd(NULL),
	m_nChannels(0)
{
	m_Channel.assign(NULL);
}

~CDIL_CAN_i_VIEW::CDIL_CAN_i_VIEW()
{
	CAN_PerformClosureOperations();
}

void CDIL_CAN_i_VIEW::GetSystemErrorString()
{
	LPVOID	MsgBuf;
	DWORD	Result = 0;
	Result = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &MsgBuf,
		0,
		NULL );

	if (Result <= 0){
		m_Err = "system error message retrieval operation failed";
	} else {
		m_Err = T2A((LPTSTR)MsgBuf);
		LocalFree(MsgBuf);
	}
}

/**
 * \brief mDNS Resolver
 * \details
 * 	Receive notifications for servers providing the requested service
 */
void CDIL_CAN_i_VIEW::mDNSResolver(
	mDNS_Event_t	State,
	std::string&	Service,
	std::string&	/* Type */,
	std::string&	/* Hostname */,
	std::string&	Address,
	UNUM16		Port,
	std::map< std::string, std::string >& TxtList )
{
	UNUM32		ModuleTypeId=0;
	EnterCriticalSection(&m_Mutex);
	if( State == MDNS_SERVICE_ADD ){
		FromStringInMap(API_DNS_SD_TXT_MOD_TYP_ID,
				TxtList, ModuleTypeId);
		for( int CAN=0; CAN<2; CAN++ ){
			/*
			 * Create new channel and a pipe.
			 * Create an interface class with the Pipe.
			 * Give the interface to the channel and store
			 * it in the map
			 */
			pVCI_t pVCI = new VCI( Service, ModuleTypeId, CAN );
			CComm* Pipe = m_CreateCCommTCP(
				inet_addr(Address.c_str()), htons(Port) );
			CVCiViewIF* pVCiIF = m_CreateCVCiViewIF(0,this,Pipe);
			pVCI->VCiIF( pVCiIF );
			m_VCI[pVCI->Id()] = pVCI;
		}
	}
	LeaveCriticalSection(&m_Mutex);
}

/* USEFUL MACROS AND FUNCTIONS: END */

/**
 * \return TRUE if a registered client exists else FALSE
 *
 * Checks for the existance of the client by its name.
 */
pClient_t CDIL_CAN_i_VIEW::GetClient(
		string	ClientName )
{
	pClientMap_t::iterator pClientItr;

	for (pClientItr = m_Clients.begin(); pClientItr != m_Clients.end();
			pClientItr++){
		if ( ClientName == pClientItr->second->m_ClientName ){
			return pClientItr->second;
		}
	}
	return NULL;
}

/**
 * \return pointer to a client
 *
 * Find a client by it's Id from the seletect set.
 */
pClient_t CDIL_CAN_i_VIEW::GetClient(
		DWORD	ClientID )
{
	pClientMap_t::iterator pClientItr;

	pClientItr = m_Clients.find(ClientID);
	if ( pClientItr == m_Clients.end() ){
		return NULL;
	}
	return pClientItr->second;
}

/**
 * \return TRUE if client removed else FALSE
 *
 * Inserts the client into the selected set.
 */
BOOL CDIL_CAN_i_VIEW::AddClient(
		pClient_t	Client )
{
	pClientMap_t::iterator pClientItr;

	pClientItr = m_Clients.find(Client->m_ClientID);
	if ( pClientItr != m_Clients.end() ){
		return FALSE;
	}
	// TODO: protect against parallel access?
	m_Clients[Client->m_ClientID] =  Client;
	return TRUE;
}

/**
 * \return TRUE if client removed else FALSE
 *
 * Removes the client with client id from the selected set.
 */
BOOL CDIL_CAN_i_VIEW::RemoveClient(
		DWORD		ClientId )
{
	BOOL		Result = FALSE;

	pClientMap_t::iterator pClientItr;

	pClientItr = m_Clients.find(ClientId);
	if ( pClientItr == m_Clients.end() ){
		return FALSE;
	}
	// TODO: protect against parallel access?
	delete pClientItr->second;
	m_Clients.erase( pClientItr );
	
	return TRUE;
}

/**
 * Dialog Display Methods
 *
 */

/**
 * \return TRUE for success, FALSE for failure
 *
 * Call back function called from ConfigDialogDIL
 */
BOOL Callback_DIL_iVIEW(
		BYTE /*Argument*/,
		PSCONTROLLER_DETAILS pDatStream,
		INT /*Length*/)
{
	return (g_DIL_CAN_i_VIEW->CAN_SetConfigData( pDatStream, 0) == S_OK);
}

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Displays the configuration dialog for controller
 */
int DisplayConfigurationDlg(
		HWND			/*hParent*/,
		DILCALLBACK		/*ProcDIL*/,
		PSCONTROLLER_DETAILS	pControllerDetails,
		UINT			nCount )
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	int nResult = WARNING_NOTCONFIRMED;

	SCONTROLLER_DETAILS sController[defNO_OF_CHANNELS];

	for(int i =0 ; i < defNO_OF_CHANNELS; i++){
		sController[i] = pControllerDetails[i];
	}

	CChangeRegisters_CAN_iVIEW ouChangeRegister(NULL, pControllerDetails, nCount);
	ouChangeRegister.DoModal();
	nResult = ouChangeRegister.nGetInitStatus();
	return nResult;
}

/**
 * \return Operation Result. 0 incase of no errors. Failure Error codes otherwise.
 *
 * This function will popup hardware selection dialog and gets the user selection of channels.
 *
 */
int CDIL_CAN_i_VIEW::ListHardwareInterfaces(
		HWND		hParent,
		DWORD		/*dwDriver*/,
		INTERFACE_HW*	psInterfaces,
		int*		pnSelList,
		int&		nCount )
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CWnd objMainWnd;
	objMainWnd.Attach(hParent);
	CHardwareListing HwList(psInterfaces, nCount, pnSelList, &objMainWnd);
	INT nRet = HwList.DoModal();
	objMainWnd.Detach();

	if ( nRet == IDOK){
		nCount = HwList.nGetSelectedList(pnSelList);
		return 0;
	} else {
		return -1;
	}
}

/* CDIL_CAN_i_VIEW function definitions */

/**
 * \return Returns S_OK for success, S_FALSE for failure
 *
 * Sets the application params.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_SetAppParams(
		HWND				hWndOwner,
		Base_WrapperErrorLogger*	pLog )
{
	HRESULT hResult = S_FALSE;

	if ((pLog != NULL)) {
		m_hOwnerWnd = hWndOwner;	// Owner window handle
		g_pLog = pLog;		// Log interface pointer
		hResult = S_OK;			// All okay
	} else {
		m_Err = "Null argument value(s) in SetAppParams";
	}
	return hResult;
}

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Displays the controller configuration dialog.
 * Fields are initialized with values supplied by InitData.
 * InitData will be updated with the user selected values.
 */

HRESULT CDIL_CAN_i_VIEW::CAN_DisplayConfigDlg(
	PSCONTROLLER_DETAILS	InitData,
	int&			Length )
{
	VALIDATE_VALUE_RETURN_VAL(m_CurrState, STATE_HW_INTERFACE_SELECTED, ERR_IMPROPER_STATE);
	VALIDATE_POINTER_RETURN_VAL(InitData, WARN_INITDAT_NCONFIRM);

	USES_CONVERSION;

	INT Result = WARN_INITDAT_NCONFIRM;
	PSCONTROLLER_DETAILS psContrlDets = (PSCONTROLLER_DETAILS)InitData;
	//First initialize with existing hw description
	for (INT i = 0; i < min(Length, (INT)m_nChannels); i++){
		psContrlDets[i].m_omHardwareDesc = m_Channel[i]->Name();
	}
	if (m_nChannels > 0){
		Result = DisplayConfigurationDlg(m_hOwnerWnd, Callback_DIL_iVIEW,
			psContrlDets, m_nChannels);
		switch (Result){
		case WARNING_NOTCONFIRMED:
			Result = WARN_INITDAT_NCONFIRM;
			break;
		case INFO_INIT_DATA_CONFIRMED:
			Length = sizeof(SCONTROLLER_DETAILS) * defNO_OF_CHANNELS;
			Result = CAN_SetConfigData(psContrlDets, Length);
			if (Result == S_OK){
				Result = INFO_INITDAT_CONFIRM_CONFIG;
			}
			break;
		case INFO_RETAINED_CONFDATA:
			Result = INFO_INITDAT_RETAINED;
			break;
		case ERR_CONFIRMED_CONFIGURED: // Not to be addressed at present
		case INFO_CONFIRMED_CONFIGURED:// Not to be addressed at present
		default:
			break;
		}
	}
	return Result;
}

/**
 * Register Client
 */
HRESULT CDIL_CAN_i_VIEW::CAN_RegisterClient(
		BOOL	Register,
		DWORD&	ClientID,
		char*	ClientName )
{
	USES_CONVERSION;
	HRESULT hResult = S_FALSE;

	EnterCriticalSection(&m_Mutex);

	if (Register){
		if (m_Clients.size() < MAX_CLIENT_ALLOWED){
			pClient_t pClient= GetClient(ClientName);
			if (!pClient){
				pClient = new Client(ClientName);
				ClientID = pClient->m_ClientID;
				AddClient( pClient );
				hResult = S_OK;
			} else {
				ClientID = pClient->m_ClientID;
				hResult = ERR_CLIENT_EXISTS;
			}
		} else {
			hResult = ERR_NO_MORE_CLIENT_ALLOWED;
		}
	} else {
		if (RemoveClient(ClientID)){
			hResult = S_OK;
		} else {
			hResult = ERR_NO_CLIENT_EXIST;
		}
	}
	LeaveCriticalSection(&m_Mutex);
	return hResult;
}

/**
 * \return Returns S_OK for success, S_FALSE for failure
 *
 * Registers the buffer pBufObj to the client ClientID
 */
HRESULT CDIL_CAN_i_VIEW::CAN_ManageMsgBuf(
		BYTE		Action,
		DWORD		ClientID,
		CBaseCANBufFSE*	pBufObj)
{
	HRESULT hResult = S_FALSE;

	if (ClientID != NULL){
		pClient_t Client = GetClient(ClientID);
		if (Client){
			if (Action == MSGBUF_ADD){
				if (pBufObj != NULL){
					if (Client->AddClientBuffer(pBufObj)){
						hResult = S_OK;
					} else {
						hResult = ERR_BUFFER_EXISTS;
					}
				}
			} else if (Action == MSGBUF_CLEAR){
				//Remove only buffer mentioned
				if (pBufObj != NULL){
					Client->RemoveClientBuffer(pBufObj);
				} else { //Remove all
					Client->RemoveClientBuffers();
				}
				hResult = S_OK;
			}
		} else {
			hResult = ERR_NO_CLIENT_EXIST;
		}
	} else {
		if (Action == MSGBUF_CLEAR){
			//Remove all msg buffers
			for (UINT i = 0; i < m_Clients.size(); i++){
				CAN_ManageMsgBuf(MSGBUF_CLEAR,
					m_Clients[i]->m_ClientID, NULL);
			}
			hResult = S_OK;
		}
	}
	return hResult;
}

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Stops the controller.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_StopHardware(void)
{
	VALIDATE_VALUE_RETURN_VAL(m_CurrState, STATE_CONNECTED, ERR_IMPROPER_STATE);

	HRESULT hResult = S_OK;
	EnterCriticalSection(&m_Mutex);
	for (UINT i = 0; i < m_nChannels; i++){
		pVCI_t pVCI = m_VCI[m_SelectedVCI[i]];
		if( pVCI && pVCI->VCiIF()->Connected() ){
			pVCI->VCiIF()->Disconnect();
		} else {
			HRESULT hResult = S_FALSE;
		}
	}
	if (hResult == S_OK){
		m_CurrState = STATE_HW_INTERFACE_SELECTED;
	}
	LeaveCriticalSection(&m_Mutex);
	return hResult;
}

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Sets the controller configuration data supplied by InitData.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_SetConfigData(
		PSCONTROLLER_DETAILS	InitData,
		int			Length )
{
	// First disconnect the node
	CAN_StopHardware();

	for (UINT i = 0; i < m_nChannels; i++){
		pVCI_t VCI = m_Channel[i];
		if (!VCI)
			continue;
		std::stringstream	Stream;
		UNUM32			Baudrate;
		Stream << InitData->m_omStrBaudrate;
		Stream >> Baudrate;
		VCI->Baudrate( Baudrate );
	}

	return S_OK;
}

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Starts the controller.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_StartHardware(void)
{
	VALIDATE_VALUE_RETURN_VAL(m_CurrState, STATE_HW_INTERFACE_SELECTED, ERR_IMPROPER_STATE);
	HRESULT hResult = S_OK;

	EnterCriticalSection(&m_Mutex);

	for (UINT i = 0; i < m_nChannels; i++){
		pVCI_t pVCI = m_VCI[m_SelectedVCI[i]];
		if( pVCI && !pVCI->VCiIF()->Connected() ){
			if (pVCI->Connect() != PDU_STATUS_NOERROR){
				hResult = S_FALSE;
			}
		}
	}
	if ( hResult == S_OK ){
		m_CurrState = STATE_CONNECTED;
	}
	m_TimeStamp = 0;
	LeaveCriticalSection(&m_Mutex);
	return hResult;
}

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Resets the controller.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_ResetHardware(void)
{
	// Clear the transmitable message list
	// Now disconnect the node
	return CAN_StopHardware();
}

/**
 * Sends STCAN_MSG structure from the client dwClientID.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_SendMsg(
		DWORD			ClientId,
		const STCAN_MSG&	CanMsg )
{
	HRESULT		hResult = S_FALSE;
	UNUM8		Data[VCI_CAN_ADDR_SZ+VCI_MAX_CAN_SZ];
	can_record_s*	CANRec = (can_record_s*)Data;
	pVCI_t		VCI = m_Channel[CanMsg.m_ucChannel-1];
	if( !VCI )
		S_FALSE;

	UNUM32 CANId = CanMsg.m_unMsgID;
	UNUM32 Flags = CanMsg.m_ucEXTENDED ? VCI_CAN_29BIT_ID : 0;
	//Remote frame : CanData.m_uDataInfo.m_sCANMsg.m_ucRTR ? VCI_CAN_RTR_ID) : 0;

	vci_can_record_constructor( CANRec, CANId, CanMsg.m_ucDataLen,
		CanMsg.m_ucData );

	//LOG_MESSAGE( g_pLog, "RxData" );

	VCI->VCiIF()->SendDataRec( Data, CanMsg.m_ucDataLen+VCI_CAN_ADDR_SZ,
		Flags, ClientId, (UNUM32)0 );

	return S_OK;
}

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Returns the controller status. hEvent will be registered
 * and will be set whenever there is change in the controller
 * status.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_GetCntrlStatus(
		const HANDLE&	/*hEvent*/,
		UINT&		 unCntrlStatus)
{
	HRESULT hResult = S_OK;

	return hResult;
}

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Gets the time mode mapping of the hardware. CurrSysTime
 * will be updated with the system time ref.
 * TimeStamp will be updated with the corresponding timestamp.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_GetTimeModeMapping(
		SYSTEMTIME&	CurrSysTime,
		UINT64&		TimeStamp,
		LARGE_INTEGER*	QueryTickCount)
{
	CurrSysTime = m_CurrSysTime;
	TimeStamp = m_TimeStamp;

	if(QueryTickCount != NULL){
		*QueryTickCount = m_QueryTickCount;
	}
	return S_OK;
}

/**
 * Gets last occured error and puts inside acErrorStr.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_GetLastErrorString(string& acErrorStr)
{
	acErrorStr = m_Err;
	return S_OK;
}

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Performs intial operations.
 * Initializes filter, queue, controller config with default values.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_PerformInitOperations(void)
{
	VALIDATE_VALUE_RETURN_VAL(m_CurrState,
		STATE_DRIVER_LOADED, ERR_IMPROPER_STATE);

	InitializeCriticalSection(&m_Mutex);
	m_CmDNS = m_CreatemDNS( API_MDNS_SERVICE_TYPE, this );
	m_CmDNS->Start();
	//Initialize the selected channel items array to -1
	for ( UINT i = 0; i< CHANNEL_ALLOWED; i++ ){
		m_SelectedVCI[i] = -1;
	}
	return S_OK;
	m_CurrState = STATE_DRIVER_SELECTED;
}

/**
 * Function to get Controller status
 */
HRESULT CDIL_CAN_i_VIEW::CAN_GetCurrStatus(s_STATUSMSG& StatusData)
{
	StatusData.wControllerStatus = NORMAL_ACTIVE;
	return S_OK;
}

/**
 * Close doen the iView interface driver.
 * Remove all the clients and buffers.
 * Stop the mDNS browser deselect the interfaces and
 * delete all the registered VCIs.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_PerformClosureOperations(void)
{
	VALIDATE_VALUE_RETURN_VAL(m_CurrState,
		STATE_DRIVER_SELECTED, ERR_IMPROPER_STATE);
	pClientMap_t Clients = m_Clients;
	pClientMap_t::iterator CItr = Clients.begin();

	for (; CItr != Clients.end(); CItr++ ){
		DWORD Id = CItr->first;
		CAN_RegisterClient(FALSE, Id, NULL);
	}

	if (m_CmDNS){
		m_CmDNS->Stop();
		delete m_CmDNS;
	}

	if( m_CurrState == STATE_CONNECTED ){
		CAN_StopHardware();
	}
	CAN_DeselectHwInterface();

	EnterCriticalSection(&m_Mutex);
	pVCIMap_t::iterator VItr = m_VCI.begin();
	for (; VItr != m_VCI.end(); VItr++ ){
		delete VItr->second->VCiIF();
		delete VItr->second;
	}
	LeaveCriticalSection(&m_Mutex);
	DeleteCriticalSection(&m_Mutex);
	m_CurrState = STATE_DRIVER_LOADED;
	return S_OK;
}

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Lists the hardware interface available. sSelHwInterface
 * will contain the user selected hw interface.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_ListHwInterfaces(
		INTERFACE_HW_LIST&	sSelHwInterface,
		INT&			nCount)
{
	INTERFACE_HW HWIF[defNO_OF_CHANNELS];

	EnterCriticalSection(&m_Mutex);

	pVCIMap_t::iterator Itr = m_VCI.begin();
	nCount = m_VCI.size();

	for ( int i=0; Itr != m_VCI.end(); Itr++, i++ ){
		ostringstream Tmp, Tmp1;
		HWIF[i].m_dwIdInterface = Itr->second->Id();
		HWIF[i].m_bytNetworkID = (unsigned char)Itr->second->CAN();
		HWIF[i].m_dwVendor = Itr->second->TypeId();
		HWIF[i].m_acDeviceName = Itr->second->Firmware();
		Tmp << Itr->second->Name() << " CAN " << dec << Itr->second->CAN();
		HWIF[i].m_acDescription = Tmp.str();
		Tmp1 << " CAN " << dec << Itr->second->CAN();
		HWIF[i].m_acNameInterface = Tmp1.str();
	}
	LeaveCriticalSection(&m_Mutex);

	if( ListHardwareInterfaces( m_hOwnerWnd, 0, HWIF,
			m_SelectedVCI, nCount ) != 0 ){
		return HW_INTERFACE_NO_SEL;
	}

	EnterCriticalSection(&m_Mutex);
	m_nChannels = nCount;
	for ( int i=0; i < nCount; i++ ){
		pVCI_t VCI = m_VCI[m_SelectedVCI[i]];
		m_Channel[i] = VCI;
		VCI->VCiIF()->Id( i+1 ); // Channels Id's start at 1
		sSelHwInterface[i] = HWIF[VCI->Id()];
	}
	LeaveCriticalSection(&m_Mutex);

	m_CurrState = STATE_HW_INTERFACE_LISTED;
	return S_OK;
}

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Selects the hardware interface selected by the user.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_SelectHwInterface(
		const INTERFACE_HW_LIST&	sSelHwInterface,
		INT				nSize )
{
	USES_CONVERSION;
	VALIDATE_POINTER_RETURN_VAL(m_hDll, S_FALSE);
	VALIDATE_VALUE_RETURN_VAL(m_CurrState, STATE_HW_INTERFACE_LISTED, ERR_IMPROPER_STATE);
	/* Check for the success */
	m_CurrState = STATE_HW_INTERFACE_SELECTED;
	return S_OK;
}

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Deselects the selected hardware interface.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_DeselectHwInterface(void)
{
	VALIDATE_VALUE_RETURN_VAL(m_CurrState, STATE_HW_INTERFACE_SELECTED, ERR_IMPROPER_STATE);
	HRESULT hResult = S_OK;
	hResult = CAN_ResetHardware();
	m_CurrState = STATE_HW_INTERFACE_LISTED;
	return hResult;
}

/**
 * \return S_OK for success, S_FALSE for failure
 *
 * Loads BOA related libraries. Updates BOA API pointers
 */
HRESULT CDIL_CAN_i_VIEW::CAN_LoadDriverLibrary(void)
{
	if ( m_hDll != NULL ){
		g_pLog->vLogAMessage(A2T(__FILE__), __LINE__, _T( iVIEW_DLL " already loaded" ));
		return DLL_ALREADY_LOADED;
	}
	m_hDll = LoadLibrary( iVIEW_DLL );

	if ( m_hDll == NULL ){
		g_pLog->vLogAMessage(A2T(__FILE__), __LINE__, _T( iVIEW_DLL " loading failed" ));
		return ERR_LOAD_DRIVER;
	}
	m_CreatemDNS = (CreatemDNS_t)GetProcAddress(m_hDll,"CreatemDNS");
	m_CreateCCommTCP = (CreateCCommTCP_t)GetProcAddress(m_hDll,"CreateCCommTCP");
	m_CreateCVCiViewIF = (CreateCVCiViewIF_t)GetProcAddress(m_hDll,"CreateCVCiViewIF");
	m_CurrState = STATE_DRIVER_LOADED;
	return S_OK;
}

/**
 * \return Returns S_OK for success, S_FALSE for failure
 *
 * Unloads the driver library.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_UnloadDriverLibrary(void)
{
	if ( m_hDll != NULL ){
		FreeLibrary( m_hDll );
		m_hDll = NULL;
	}
	m_CurrState = STATE_DRIVER_UNLOADED;
	return S_OK;
}

/**
 * Gets the controller param eContrParam of the channel.
 * Value stored in lParam.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_GetControllerParams(
		LONG&		lParam,
		UINT		nChannel,
		ECONTR_PARAM	eContrParam)
{
	HRESULT hResult = S_OK;

	switch (eContrParam){
	case NUMBER_HW:
		lParam = CHANNEL_ALLOWED;
		break;

	case NUMBER_CONNECTED_HW:
		lParam = CHANNEL_ALLOWED;
		break;

	case DRIVER_STATUS:
		lParam = TRUE;
		break;

	case HW_MODE:
		lParam = defCONTROLLER_ACTIVE;
		break;

	case CON_TEST:
		lParam = (LONG)-1;
		break;

	default:
		hResult = S_FALSE;
		break;
	}
	return hResult;
}

/**
 * Sets the controller param eContrParam of the channel.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_SetControllerParams(
		int		nValue,
		ECONTR_PARAM	eContrparam)
{
	switch(eContrparam){
	case HW_MODE:
		switch(nValue){
		case defMODE_ACTIVE:
			for (UINT i = 0; i < m_nChannels; i++ ){
				/*
				OCI_CANControllerProperties val;
				val.mode = OCI_CONTROLLER_MODE_RUNNING;
				(*(sBOA_PTRS.m_sOCI.setCANControllerProperties))(sg_asChannel[i].m_OCI_HwHandle,
					&val);
				*/
			}
			break;
		case defMODE_PASSIVE:
			for (UINT i = 0; i < m_nChannels; i++){
				/*
				OCI_CANControllerProperties val;
				val.mode  = OCI_CONTROLLER_MODE_SUSPENDED;
				(*(sBOA_PTRS.m_sOCI.setCANControllerProperties))(sg_asChannel[i].m_OCI_HwHandle,
					&val);
				*/
			}
			break;
		}
	}
	return S_OK;
}

/**
 * Gets the error counter for corresponding channel.
 */
HRESULT CDIL_CAN_i_VIEW::CAN_GetErrorCount(
		SERROR_CNT&	sErrorCnt,
		UINT		/*nChannel*/,
		ECONTR_PARAM	/*eContrParam*/)
{
	memset(&sErrorCnt, 0, sizeof(SERROR_CNT));
	return S_OK;
}

/**
* \brief         Applies FilterType(PASS/STOP) filter for corresponding
*                channel. Frame ids are supplied by punMsgIds.
* \param[in]     FilterType, holds one of the FILTER_TYPE enum value.
* \param[in]     Channel, is TYPE_CHANNEL
* \param[in]     punMsgIds, is UINT*
* \param[in]     nLength, is UINT
* \return        S_OK for success, S_FALSE for failure
* \authors       Arunkumar Karri
* \date          07.10.2011 Created
*/
HRESULT CDIL_CAN_VectorXL::CAN_FilterFrames(
		FILTER_TYPE	FilterType,
		TYPE_CHANNEL	Channel,
		UINT*		pMsgIds,
		UINT )
{
	return S_OK;
}

/**
 * VCI Rx Data
 * Process incomming frames.
 */
T_PDU_ERROR CDIL_CAN_i_VIEW::RxData(
		UNUM32				Id,
		vci_data_record_with_data*	VCIRec )
{
	can_record_s* CANRec=(can_record_s*)VCIRec->data;
	UNUM32 CANId = ntohl( CANRec->address );
	UNUM32 Flags = htonl(VCIRec->flags);
	UNUM32 ClientId = htonl(VCIRec->header.user_data);
	UNUM16 Len = ntohs( VCIRec->header.data_length )-(VCI_DATA_DATA_SZ+VCI_CAN_DATA_SZ);
	UNUM32 TS = htonl(VCIRec->timestamp) / 100;
	/*
	 * Set the base timestamp on first message
	 */
	if( m_TimeStamp==0 ){
		m_TimeStamp = TS;
		GetLocalTime(&m_CurrSysTime);
		QueryPerformanceCounter(&m_QueryTickCount);
	}

	STCANDATA CanData;
	STCAN_MSG& CanMsg = CanData.m_uDataInfo.m_sCANMsg;
	//LOG_MESSAGE( g_pLog, "RxData" );

	CanData.m_lTickCount.QuadPart = TS;

	CanMsg.m_ucChannel = (unsigned char)Id;
	CanMsg.m_unMsgID = CANId;
	CanMsg.m_ucDataLen = (unsigned char)Len;
	CanMsg.m_ucEXTENDED = (Flags & VCI_CAN_29BIT_ID)? 1 : 0;
	CanMsg.m_bCANFD = false;
	CanMsg.m_ucRTR = /*(Flags & VCI_CAN_RTR_ID) ? 1 :*/ 0;
	memcpy(&CanMsg.m_ucData, CANRec->data, Len);

	EnterCriticalSection(&m_Mutex);
	/*
	 * If the frame is a Tx echo, the originating client see's
	 * the frame as a Tx, the rest as an Rx.
	 */
	pClientMap_t::iterator pClientItr = m_Clients.begin();

	for (; pClientItr != m_Clients.end(); pClientItr++){
		if( ( Flags & VCI_TX_ECHO ) && ClientId == pClientItr->first ){
			CanData.m_ucDataType = TX_FLAG;
		} else {
			CanData.m_ucDataType = RX_FLAG;
		}
		pClientItr->second->WriteClientBuffers(CanData);
	}
	LeaveCriticalSection(&m_Mutex);

	return PDU_STATUS_NOERROR;
}
/**
 * VCI Rx Event
 * Process incomming events
 */
T_PDU_ERROR CDIL_CAN_i_VIEW::RxEvent(
		UNUM32				Id,
		vci_event_record_s*		VCIEvent )
{
	LOG_MESSAGE( g_pLog, "RxEvent" );
	return PDU_STATUS_NOERROR;
}
