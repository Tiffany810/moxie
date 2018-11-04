package Mcached;

import (
	"time"
)

const (
	Error_NoError				= 0
	Error_SlotNotInThisGroup 	= 1
	Error_SlotHasInThisGroup	= 2
	Error_SlotInAdjusting		= 3
	Error_SlotNotInAdjusting	= 4
	Error_SlotHasOwnGroup		= 5
	Error_GroupIsNotActivated	= 6
	Error_GroupIsActivated		= 7
	Error_GroupNotEmpty			= 8
	Error_TxnCommitFailed		= 9
	Error_SlotJsonFormatErr		= 10
	Error_GroupJsonFormatErr	= 11
	Error_SlotSourceIdError		= 12
	Error_SlotDstIdError		= 13
	Error_GetMaxGroupIdFailed	= 14
	Error_CmdNotFound			= 15
	Error_SlotOrGroupIsZero		= 16
	Error_ServerInternelError 	= 17
	Error_RequestParseFailed	= 18
	Error_InvaildArgs			= 19
	Error_UnknownServerName		= 20
	Error_NotLeader				= 21
	Error_InvaildServerName		= 22
)

const (
	HtmlPath						= "./index/"
	IndexHtmlFile					= "./index/index.html"
)

// Http handler path
const (
	KeepAlivePrefix					= "/mcached/keepalive/"
	SlotKeeperPrefix				= "/mcached/slotkeeper/"
	CachedGroupKeeperPrefix			= "/mcached/groupkeeper/"
	NameResolverPrefix				= "/mcached/nameresolver/"
	QueryInfosPrefix				= "/mcached/queryinfos/"
)

// other meta data
const (
	SlotsPrefix						= "/Mcached/Slots/"
	CachedGroupPrefix				= "/Mcached/CachedGroup/"
	GroupIdPrefix					= "/Mcached/GroupId/"		
	ManagerMasterPrefix				= "/Mcached/ManagerMaster/"
	MaxGroupIdKey					= "/Mcached/GroupId/MaxGroupId"		
	GroupRevisePrefix				= "/Mcached/GroupRevise/"
	KeepAliveWorkRootDef			= "/Mcached/EndPoints/"
	ObserveResolverPrefix			= "/Mcached/EndPoints/Observe/"
	CachedGroupResolverPrefix		= "/Mcached/EndPoints/DataServer/"
)

const (
	EtcdTxnRequestTimeout			= 5 * time.Second
)

const (
	KeepAliveLease					= 5
)

const (
	Msg_OK							= "Ok"
	Msg_Ok_Value_Ext				= "Ok! value was set int ext."
)

const (
	MaxGroupIdInitValue				= 0

	SlotsListTickerTimeout			= 10 * time.Second
	GroupListTickerTimeout			= 10 * time.Second

	McachedSlotsStart				= 1
	McachedSlotsEnd					= 1025
	CacheGroupIdStart				= 1
)

type CacheGroupItem struct {
	GroupId						uint64	    `json:"group_id"`
	Activated					bool 		`json:"is_activated"`
	SlotsIndex					[]uint64	`json:"slots"`
}

type SlotItem struct {
	Index 						uint64 `json:"index"`
	Groupid 					uint64 `json:"group_id"`
	IsAdjust 					bool   `json:"is_adjust"`
	DstGroupid 					uint64 `json:"dst_froup_id"`
}