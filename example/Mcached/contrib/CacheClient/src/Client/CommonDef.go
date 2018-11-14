package Client

const (
	KeepAlivePrefix         = "/mcached/keepalive/"
	SlotKeeperPrefix        = "/mcached/slotkeeper/"
	CachedGroupKeeperPrefix = "/mcached/groupkeeper/"
	NameResolverPrefix      = "/mcached/nameresolver/"
	QueryInfosPrefix        = "/mcached/queryinfos/"
)

const (
	ClientPoolDefaultCapacity = 1024
)

const (
	Error_NoError             = 0
	Error_SlotNotInThisGroup  = 1
	Error_SlotHasInThisGroup  = 2
	Error_SlotInAdjusting     = 3
	Error_SlotNotInAdjusting  = 4
	Error_SlotHasOwnGroup     = 5
	Error_GroupIsNotActivated = 6
	Error_GroupIsActivated    = 7
	Error_GroupNotEmpty       = 8
	Error_TxnCommitFailed     = 9
	Error_SlotJsonFormatErr   = 10
	Error_GroupJsonFormatErr  = 11
	Error_SlotSourceIdError   = 12
	Error_SlotDstIdError      = 13
	Error_GetMaxGroupIdFailed = 14
	Error_CmdNotFound         = 15
	Error_SlotOrGroupIsZero   = 16
	Error_ServerInternelError = 17
	Error_RequestParseFailed  = 18
	Error_InvaildArgs         = 19
	Error_UnknownServerName   = 20
	Error_NotLeader           = 21
	Error_InvaildServerName   = 22
	Error_NoNameServerMsg     = 23
)

type KeepAliveRequest struct {
	Cmdtype    uint32 `json:"cmd_type"`
	Master     bool   `json:"is_master"`
	Addr       string `json:"hosts"`
	Groupid    uint64 `json:"source_id"`
	ServerName string `json:"server_name"`
}

type KeepAliveResponse struct {
	Succeed bool   `json:"succeed"`
	Ecode   int32  `json:"ecode"`
	Msg     string `json:"msg"`
}
