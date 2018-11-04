package Mcached

import (
	"context"
	"encoding/json"
	"errors"
	"go.etcd.io/etcd/clientv3"
	"io/ioutil"
	"log"
	"net/http"
	"sync"
	"time"
)

const (
	CmdQuerySlots  = 0
	CmdQueryGroups = 1
)

type QueryRequest struct {
	Cmdtype uint32 `json:"cmd_type"`
	Request string `json:"request"`
}

type QueryResponse struct {
	Succeed bool   `json:"succeed"`
	Ecode   int    `json:"ecode"`
	Msg     string `json:"msg"`
	Ext     string `json:"ext"`
}

type QuerySlotRequest struct {
	PageSize  int `json:"page_size"`
	PageIndex int `json:"page_index"`
}

type QuerySlotResponse struct {
	Slots []SlotItem `json:"slots"`
}

type QueryGroupRequest struct {
	PageSize  int `json:"page_size"`
	PageIndex int `json:"page_index"`
}

type QueryGroupResponse struct {
	Groups []CacheGroupItem `json:"groups"`
}

type QueryInfos struct {
	UpdateListTicker *time.Ticker

	SlotList      map[uint64]*SlotItem
	SlotIdList    []uint64
	SlotListMutex sync.Mutex

	GroupList      map[uint64]*CacheGroupItem
	GroupIdList    []uint64
	GroupListMutex sync.Mutex

	ctx    context.Context
	server *ManagerServer
	// must be pointer to ManagerServer's logger
	logger      *log.Logger
	etcd_client *clientv3.Client
}

func NewQueryInfos(server *ManagerServer) (*QueryInfos, error) {
	query := &QueryInfos{
		UpdateListTicker: time.NewTicker(SlotsListTickerTimeout),

		SlotList:   make(map[uint64]*SlotItem),
		SlotIdList: make([]uint64, 0),

		GroupList:   make(map[uint64]*CacheGroupItem),
		GroupIdList: make([]uint64, 0),

		ctx:         server.ctx,
		server:      server,
		logger:      server.logger,
		etcd_client: server.etcdClient,
	}
	return query, nil
}

func (query *QueryInfos) Run() {
	go func() {
		for {
			select {
			case <-query.ctx.Done():
				return
			case <-query.UpdateListTicker.C:
				query.DoUpdateSlotList()
				query.DoUpdateGroupList()
			}
		}
	}()
}

func (query *QueryInfos) DoUpdateGroupList() {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	group_succ := true
	group_map := make(map[uint64]*CacheGroupItem)
	group_resp, err := query.etcd_client.Get(ctx, CachedGroupPrefix, clientv3.WithPrefix())
	group_id_list := make([]uint64, 0)
	if err != nil {
		query.logger.Println("Update cache group failed! error[", err, "]")
		group_succ = false
	} else {
		for _, group := range group_resp.Kvs {
			var item CacheGroupItem
			if err := json.Unmarshal(group.Value, &item); err != nil {
				query.logger.Println("Unmarshal slot item : ", err)
				continue
			}
			group_map[item.GroupId] = &item
			group_id_list = append(group_id_list, item.GroupId)
		}
	}
	query.GroupListMutex.Lock()
	defer query.GroupListMutex.Unlock()
	if group_succ {
		query.logger.Println("DoUpdateSlotsCache update group:", len(group_map))
		query.GroupList = group_map
		HeapSort(group_id_list)
		query.GroupIdList = group_id_list
	}
}

func (query *QueryInfos) DoUpdateSlotList() {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	slot_succ := true
	slot_resp, err := query.etcd_client.Get(ctx, SlotsPrefix, clientv3.WithPrefix())
	slot_item_map := make(map[uint64]*SlotItem)
	slot_index_list := make([]uint64, 0)
	if err != nil {
		query.logger.Println("Update slot items error[", err, "]")
		slot_succ = false
	} else {
		for _, slot := range slot_resp.Kvs {
			var item SlotItem
			if err := json.Unmarshal(slot.Value, &item); err != nil {
				query.logger.Println("Unmarshal slot item : ", err)
				continue
			}
			slot_item_map[item.Index] = &item
			slot_index_list = append(slot_index_list, item.Index)
		}
	}
	query.SlotListMutex.Lock()
	defer query.SlotListMutex.Unlock()
	if slot_succ {
		query.logger.Println("DoUpdateSlotsCache update slot:", len(slot_item_map))
		query.SlotList = slot_item_map
		HeapSort(slot_index_list)
		query.SlotIdList = slot_index_list
	}
}

func (query *QueryInfos) ServeHTTP(response http.ResponseWriter, request *http.Request) {
	lr_ctx_nosafe := query.server.PeekLeaderContext()
	if lr_ctx_nosafe.IsLeader == false {
		if lr_ctx_nosafe.LeaderVal == "" {
			query.logger.Println("Internal Server Error leader status!")
			response.WriteHeader(500)
		} else {
			query.logger.Println("Request was redireted!")
			http.Redirect(response, request, GetRedirectRequestUrl(request, lr_ctx_nosafe.LeaderVal), 307)
		}
		return
	}

	query_request := &QueryRequest{}
	body, _ := ioutil.ReadAll(request.Body)
	log.Printf("Body:[%s]\n", body)
	if err := json.Unmarshal(body, query_request); err != nil {
		response.WriteHeader(500)
		query_response := &QueryResponse{
			Succeed: false,
			Ecode:   Error_InvaildArgs,
			Msg:     err.Error(),
		}
		if error_res, err := json.Marshal(query_response); err == nil {
			response.Write(error_res)
		}
		return
	}

	query.DoSlotRequest(query_request, response, request)
}

func (query *QueryInfos) DoSlotRequest(query_request *QueryRequest, response http.ResponseWriter, request *http.Request) {
	query_response := &QueryResponse{
		Succeed: false,
		Ecode:   Error_NoError,
		Msg:     "",
		Ext:     "",
	}

	log.Println("Request json:[", query_request.Request, "]")

	switch query_request.Cmdtype {
	case CmdQuerySlots:
		query.DoQuerySlots(query_request, query_response)
	case CmdQueryGroups:
		query.DoQueryGroups(query_request, query_response)
	default:
		query_response.Msg = "Cmd not found!"
		query_response.Succeed = false
		query_response.Ecode = Error_CmdNotFound
	}

	if error_res, err := json.Marshal(query_response); err == nil {
		response.Write(error_res)
	} else {
		response.WriteHeader(500)
	}
}

func (query *QueryInfos) DoQueryGroups(query_request *QueryRequest, query_response *QueryResponse) {
	group_query := &QueryGroupRequest{}
	if err := json.Unmarshal([]byte(query_request.Request), group_query); err != nil {
		query_response.Succeed = false
		query_response.Ecode = Error_InvaildArgs
		query_response.Msg = err.Error()
		return
	}

	group_response := &QueryGroupResponse{}
	if ret, ecode, err := query.PackageGroupsInfo(group_query, group_response); err != nil {
		query_response.Succeed = ret
		query_response.Ecode = ecode
		query_response.Msg = err.Error()
		return
	}

	if info_bytes, err := json.Marshal(group_response); err != nil {
		query_response.Succeed = false
		query_response.Ecode = Error_ServerInternelError
		query_response.Msg = err.Error()
		return
	} else {
		query_response.Succeed = true
		query_response.Ecode = Error_NoError
		query_response.Msg = Msg_Ok_Value_Ext
		query_response.Ext = string(info_bytes)
		return
	}
}

func (query *QueryInfos) checkGroupRequestIndexNoSafe(start_index, end_index int) bool {
	if start_index < 0 || end_index < 0 {
		return false
	}
	if start_index > end_index {
		return false
	}
	if start_index >= len(query.GroupIdList) || end_index > len(query.GroupIdList) {
		return false
	}
	return true
}


func (query *QueryInfos) PackageGroupsInfo(group_request *QueryGroupRequest, group_response *QueryGroupResponse) (bool, int, error) {
	query.GroupListMutex.Lock()
	defer query.GroupListMutex.Unlock()

	start_index := (group_request.PageIndex - 1) * group_request.PageSize
	end_index := group_request.PageSize + start_index

	start_index = Max(0, start_index)
	end_index = Min(end_index, len(query.GroupIdList))

	if !query.checkGroupRequestIndexNoSafe(start_index, end_index) {
		return false, Error_InvaildArgs, errors.New("Args's index invailed!")
	}

	for i := start_index; i < end_index; i++ {
		if item, ok := query.GroupList[query.GroupIdList[i]]; ok {
			group_response.Groups = append(group_response.Groups, *item)
		}
	}

	return true, Error_NoError, nil
}

func (query *QueryInfos) DoQuerySlots(query_request *QueryRequest, query_response *QueryResponse) {
	slot_query := &QuerySlotRequest{}
	if err := json.Unmarshal([]byte(query_request.Request), slot_query); err != nil {
		query_response.Succeed = false
		query_response.Ecode = Error_InvaildArgs
		query_response.Msg = err.Error()
		return
	}

	slot_response := &QuerySlotResponse{}
	if ret, ecode, err := query.PackageSlotsInfo(slot_query, slot_response); err != nil {
		query_response.Succeed = ret
		query_response.Ecode = ecode
		query_response.Msg = err.Error()
		return
	}

	if info_bytes, err := json.Marshal(slot_response); err != nil {
		query_response.Succeed = false
		query_response.Ecode = Error_ServerInternelError
		query_response.Msg = err.Error()
		return
	} else {
		query_response.Succeed = true
		query_response.Ecode = Error_NoError
		query_response.Msg = Msg_Ok_Value_Ext
		query_response.Ext = string(info_bytes)
		return
	}
}

func (query *QueryInfos) checkSlotRequestIndexNoSafe(start_index, end_index int) bool {
	if start_index < 0 || end_index < 0 {
		return false
	}
	if start_index > end_index {
		return false
	}
	if start_index >= len(query.SlotIdList) || end_index > len(query.SlotIdList) {
		return false
	}
	return true
}

func (query *QueryInfos) PackageSlotsInfo(slot_request *QuerySlotRequest, slot_response *QuerySlotResponse) (bool, int, error) {
	query.SlotListMutex.Lock()
	defer query.SlotListMutex.Unlock()

	start_index := (slot_request.PageIndex - 1) * slot_request.PageSize
	end_index := slot_request.PageSize + start_index

	start_index = Max(0, start_index)
	end_index = Min(end_index, len(query.SlotIdList))

	if !query.checkSlotRequestIndexNoSafe(start_index, end_index) {
		return false, Error_InvaildArgs, errors.New("Args's index invailed!")
	}

	for i := start_index; i < end_index; i++ {
		if item, ok := query.SlotList[query.SlotIdList[i]]; ok {
			slot_response.Slots = append(slot_response.Slots, *item)
		}
	}

	return true, Error_NoError, nil
}
