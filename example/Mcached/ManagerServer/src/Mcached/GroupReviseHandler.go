package Mcached

import (
	"encoding/json"
	"io/ioutil"
	"net/http"
	"go.etcd.io/etcd/clientv3/concurrency"
)

const (
	CmdCreateCacheGroup 		= 	0
	CmdActivateGroup			= 	1
	CmdDeleteCacheGroup			= 	2
	CmdAddSlotToGroup			= 	3
	CmdMoveSlotDone				= 	4
	CmdDelSlotFromGroup			= 	5
	CmdMoveSlotStart			= 	6
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
)

type GroupReviseHandler struct {
	server *ManagerServer
}

type GroupReviseRequest struct {
	Cmdtype					uint32	`json:"cmd_type"`
	SourceGroupId			uint64	`json:"source_id"`
	DestGroupId				uint64	`json:"dest_id"`
	SlotId 					uint64	`json:"slot_id"`
}

type GroupReviseResponse struct {
	Succeed					bool	`json:"succeed"`
	Ecode 					int32	`json:"ecode"`
	Msg 					string	`json:"msg"`
}

func GetRedirectRequestUrl(request *http.Request, redirect_host string) string {
	if request == nil {
		return ""
	}
	return redirect_host + request.RequestURI
}

func (handler *GroupReviseHandler) ServeHTTP(response http.ResponseWriter, request *http.Request) {
	isleader, leader, err := handler.server.IsLeader()
	if err != nil{
		if err == concurrency.ErrElectionNoLeader {
			// Service Unavailable
			handler.server.logger.Println("Service Unavailable")
			response.WriteHeader(503)
		} else {
			// Internal Server Error
			handler.server.logger.Println("Internal Server Error")
			response.WriteHeader(500)
		}
		return
	}
	
	if isleader == false {
		if leader == "" {
			handler.server.logger.Println("Internal Server Error leader status!")
			response.WriteHeader(500)
		} else {
			handler.server.logger.Println("Request was redireted!")
			http.Redirect(response, request, GetRedirectRequestUrl(request, leader), 307)
		}
		return
	}

	Httpres := &GroupReviseResponse {
		Succeed : false,
		Msg : "",
	}

	grr := &GroupReviseRequest {}
	body, _ := ioutil.ReadAll(request.Body)
	if err := json.Unmarshal(body, grr); err != nil {
		response.WriteHeader(500)
		handler.server.logger.Println("json.Unmarshal Request failed:", err)
		if error_res, err := json.Marshal(Httpres); err == nil {
			response.Write(error_res)
		}
		return
	}

	switch grr.Cmdtype {
	case CmdCreateCacheGroup:
		handler.HandleCreateCacheGroup(grr, Httpres)
	case CmdActivateGroup:
		handler.HandleActivateCacheGroup(grr, Httpres)
	case CmdDeleteCacheGroup:
		handler.HandleDeleteCacheGroup(grr, Httpres)
	case CmdAddSlotToGroup:
		handler.HandleAddSlot(grr, Httpres)
	case CmdDelSlotFromGroup:
		handler.HandleDelSlot(grr, Httpres)
	case CmdMoveSlotDone:
		handler.HandleMoveSlot(grr, Httpres)
	case CmdMoveSlotStart:
		handler.HandleSlotStartMove(grr, Httpres)
	default:
		Httpres.Msg = "Cmd not found!"
		Httpres.Succeed = false
		Httpres.Ecode = Error_CmdNotFound
	}

	if error_res, err := json.Marshal(Httpres); err == nil {
		response.Write(error_res)
	} else {
		response.WriteHeader(500)
		handler.server.logger.Println("Marshal failed!")
	}
}

func (handler *GroupReviseHandler) HandleActivateCacheGroup(request *GroupReviseRequest, response *GroupReviseResponse) {
	Item := &CacheGroupItem {
		GroupId	: request.SourceGroupId,
		SlotsIndex : make([]uint64, 0),
	}

	if succ, err, ecode := handler.server.ActivateCachedGroup(Item); err == nil {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = "ok"
	} else {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = err.Error()
	}
}

func (handler *GroupReviseHandler) HandleCreateCacheGroup(request *GroupReviseRequest, response *GroupReviseResponse) {
	Item := &CacheGroupItem {
		GroupId	: 0,
		SlotsIndex : make([]uint64, 0),
	}

	if succ, err, ecode := handler.server.CreateCachedGroup(Item); err == nil {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = "ok"
	} else {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = err.Error()
	}
}

func (handler *GroupReviseHandler) HandleDeleteCacheGroup(request *GroupReviseRequest, response *GroupReviseResponse) {
	if succ, err, ecode := handler.server.DeleteCachedGroup(request.SourceGroupId); err == nil {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = "ok"
	} else {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = err.Error()
	}
}

func (handler *GroupReviseHandler) HandleDelSlot(request *GroupReviseRequest, response *GroupReviseResponse) {
	if succ, err, ecode := handler.server.DeleteSlotfromGroup(request.SlotId, request.SourceGroupId); err == nil {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = "ok"
	} else {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = err.Error()
	}
}

func (handler *GroupReviseHandler) HandleAddSlot(request *GroupReviseRequest, response *GroupReviseResponse) {
	if succ, err, ecode := handler.server.AddSlotToGroup(request.SlotId, request.SourceGroupId); err == nil {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = "ok"
	} else {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = err.Error()
	}
}

func (handler *GroupReviseHandler) HandleMoveSlot(request *GroupReviseRequest, response *GroupReviseResponse) {
	if succ, err, ecode := handler.server.MoveSlotToGroup(request.SlotId, request.SourceGroupId, request.DestGroupId); err == nil {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = "ok"
	} else {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = err.Error()
	}
}

func (handler *GroupReviseHandler) HandleSlotStartMove(request *GroupReviseRequest, response *GroupReviseResponse) {
	if succ, err, ecode := handler.server.StartMoveSlotToGroup(request.SlotId, request.SourceGroupId, request.DestGroupId); err == nil {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = "ok"
	} else {
		response.Succeed = succ
		response.Ecode = ecode
		response.Msg = err.Error()
	}
}