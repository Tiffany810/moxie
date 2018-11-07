package Mcached

import (
	"context"
	"encoding/json"
	"fmt"
	"go.etcd.io/etcd/clientv3"
	"io/ioutil"
	"log"
	"net/http"
)

const (
	CmdCreateCacheGroup 		= 	0
	CmdActivateGroup			= 	1
	CmdDeleteCacheGroup			= 	2
)

type CachedGroupKeeper struct {
	server 					*ManagerServer
	// must be pointer to ManagerServer's logger
	logger					*log.Logger
	etcd_client				*clientv3.Client
}

type CachedGroupRequest struct {
	Cmdtype uint32 `json:"cmd_type"`
	GroupId uint64 `json:"group_id"`
	Ext     string `json:"ext"`
}

type CachedGroupResponse struct {
	Succeed bool   `json:"succeed"`
	Ecode   int32  `json:"ecode"`
	Msg     string `json:"msg"`
	Ext     string `json:"ext"`
}

func (keeper *CachedGroupKeeper) ServeHTTP(response http.ResponseWriter, request *http.Request) {
	if origin := request.Header.Get("Origin"); origin != "" {
		response.Header().Set("Access-Control-Allow-Origin", "*")
		response.Header().Set("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE")
		response.Header().Set("Access-Control-Allow-Headers", "Action, Module")
	}
		
	if request.Method == "OPTIONS" {
		return
	}
	
	lr_ctx_nosafe := keeper.server.PeekLeaderContext()
	if lr_ctx_nosafe.IsLeader == false {
		if lr_ctx_nosafe.LeaderVal == "" {
			keeper.logger.Println("Internal Server Error leader status!")
			response.WriteHeader(500)
		} else {
			keeper.logger.Println("Request was redireted!")
			http.Redirect(response, request, GetRedirectRequestUrl(request, lr_ctx_nosafe.LeaderVal), 307)
		}
		return
	}

	cached_group_request := &CachedGroupRequest{}
	body, _ := ioutil.ReadAll(request.Body)
	if err := json.Unmarshal(body, cached_group_request); err != nil {
		response.WriteHeader(500)
		keeper.logger.Println("json.Unmarshal Request failed:", err)
		cached_group_response := &CachedGroupResponse{
			Succeed: false,
			Ecode:   Error_RequestParseFailed,
			Msg:     err.Error(),
		}
		if error_res, err := json.Marshal(cached_group_response); err == nil {
			response.Write(error_res)
		}
		return
	}

	keeper.DoCachedGroup(cached_group_request, response, request)
}

func (keeper *CachedGroupKeeper) DoCachedGroup(cached_group_request *CachedGroupRequest, response http.ResponseWriter, request *http.Request) {
	cached_group_response := &CachedGroupResponse{
		Succeed: false,
		Ecode:   Error_NoError,
		Msg:     "",
	}

	switch cached_group_request.Cmdtype {
	case CmdCreateCacheGroup:
		keeper.DoCreateCacheGroup(cached_group_request, cached_group_response)
	case CmdActivateGroup:
		keeper.DoActivateCacheGroup(cached_group_request, cached_group_response)
	case CmdDeleteCacheGroup:
		keeper.DoDeleteCacheGroup(cached_group_request, cached_group_response)
	default:
		cached_group_response.Msg = "Cmd not found!"
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_CmdNotFound
	}

	if error_res, err := json.Marshal(cached_group_response); err == nil {
		response.Write(error_res)
	} else {
		response.WriteHeader(500)
	}
}

func (keeper *CachedGroupKeeper) GetMaxGroupId() (uint64, error) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()
	res, err := keeper.etcd_client.Get(ctx, MaxGroupIdKey)
	if err != nil {
		keeper.logger.Println("Get max group id error:", err)
		return 0, err
	}

	if len(res.Kvs) == 0 {
		keeper.logger.Panicln("Can't find max group id!")
	}
	var ret uint64
	fmt.Sscanf(string(res.Kvs[0].Value), "%d", &ret)
	return ret, nil
}

func (keeper *CachedGroupKeeper) DoCreateCacheGroup(cached_group_request *CachedGroupRequest, cached_group_response *CachedGroupResponse) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()

	oldid, err := keeper.GetMaxGroupId()
	if err != nil {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_GetMaxGroupIdFailed
		cached_group_response.Msg = err.Error()
		return
	}
	newid := oldid + 1

	Item := &CacheGroupItem{
		GroupId:    newid,
		Activated:  false,
		SlotsIndex: make([]uint64, 0),
	}

	new_cached_group_prefix := BuildCacheGroupIdKey(newid)

	Item_byte, err := json.Marshal(Item)
	if err != nil {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_GroupJsonFormatErr
		cached_group_response.Msg = err.Error()
		return
	}

	old_id_str_value := fmt.Sprintf("%d", oldid)
	new_id_str_value := fmt.Sprintf("%d", newid)

	txn := keeper.etcd_client.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(new_cached_group_prefix), "=", 0),
		clientv3.Compare(clientv3.Value(MaxGroupIdKey), "=", old_id_str_value))
	txn.Then(clientv3.OpPut(new_cached_group_prefix, string(Item_byte)),
		clientv3.OpPut(MaxGroupIdKey, new_id_str_value))
	txn_res, err := txn.Commit()
	if err != nil {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_TxnCommitFailed
		cached_group_response.Msg = err.Error()
		return
	}

	if !txn_res.Succeeded {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_TxnCommitFailed
		cached_group_response.Msg = "Txn commit failed!"
		return
	}

	cached_group_response.Succeed = true
	cached_group_response.Ecode = Error_NoError
	cached_group_response.Msg = Msg_Ok_Value_Ext
	cached_group_response.Ext = new_id_str_value
	return
}

func (keeper *CachedGroupKeeper) DoActivateCacheGroup(cached_group_request *CachedGroupRequest, cached_group_response *CachedGroupResponse) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()

	group_id_key_prefix := BuildCacheGroupIdKey(cached_group_request.GroupId)
	log.Printf("Groupid[%d],key[%s]\n", cached_group_request.GroupId, group_id_key_prefix)
	txn := keeper.etcd_client.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(group_id_key_prefix), ">", 0))
	txn.Then(clientv3.OpGet(group_id_key_prefix))
	txn_res, err := txn.Commit()
	if err != nil {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_TxnCommitFailed
		cached_group_response.Msg = err.Error()
		return
	}

	if !txn_res.Succeeded {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_TxnCommitFailed
		cached_group_response.Msg = "Get group failed!"
		return
	}

	for i := 0; i < len(txn_res.Responses); i++ {
		for j := 0; j < len(txn_res.Responses[i].GetResponseRange().Kvs); j++ {
			item := txn_res.Responses[i].GetResponseRange().Kvs[j]
			fmt.Printf("key[%d][%d]:%s->%s\n", i, j, item.Key, item.Value)
		}
	}

	Item := &CacheGroupItem{}

	group_kv := txn_res.Responses[0].GetResponseRange().Kvs[0]
	if json.Unmarshal(group_kv.Value, &Item) != nil {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_GroupJsonFormatErr
		cached_group_response.Msg = "Unmarshal ETCD group value failed!"
		return
	}

	if Item.Activated {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_GroupIsActivated
		cached_group_response.Msg = "Group had Activated!"
		return
	}

	Item.Activated = true

	Item_byte, err := json.Marshal(&Item)
	if err != nil {
		keeper.logger.Panicln("Marshal Group slotid!")
	}

	ctxn := keeper.etcd_client.Txn(ctx)
	ctxn.If(clientv3.Compare(clientv3.ModRevision(group_id_key_prefix), "=", group_kv.ModRevision))
	ctxn.Then(clientv3.OpPut(group_id_key_prefix, string(Item_byte)))
	ctxn_res, err := ctxn.Commit()
	if err != nil {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_TxnCommitFailed
		cached_group_response.Msg = err.Error()
		return
	}

	if !ctxn_res.Succeeded {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_TxnCommitFailed
		cached_group_response.Msg = "Txn commit failed!"
		return
	}

	cached_group_response.Succeed = true
	cached_group_response.Ecode = Error_NoError
	cached_group_response.Msg = ""
}

func (keeper *CachedGroupKeeper) DoDeleteCacheGroup(cached_group_request *CachedGroupRequest, cached_group_response *CachedGroupResponse) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()
	if cached_group_request.GroupId == 0 {
		keeper.logger.Panicln("Group id is zero!")
	}

	key_prefix := BuildCacheGroupIdKey(cached_group_request.GroupId)

	judge_empty_txn := keeper.etcd_client.Txn(ctx)
	judge_empty_txn.If(clientv3.Compare(clientv3.CreateRevision(key_prefix), "!=", 0))
	judge_empty_txn.Then(clientv3.OpGet(key_prefix))
	jtxn_res, err := judge_empty_txn.Commit()
	if err != nil {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_TxnCommitFailed
		cached_group_response.Msg = err.Error()
		return
	}

	if !jtxn_res.Succeeded {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_TxnCommitFailed
		cached_group_response.Msg = "Judge group empty failed!"
		return
	}

	var Item CacheGroupItem
	if err := json.Unmarshal(jtxn_res.Responses[0].GetResponseRange().Kvs[0].Value, &Item); err != nil {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_GroupJsonFormatErr
		cached_group_response.Msg = err.Error()
		return
	}

	if len(Item.SlotsIndex) > 0 {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_GroupNotEmpty
		cached_group_response.Msg = "Group isn't empty!"
		return
	}

	txn := keeper.etcd_client.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(key_prefix), "!=", 0))
	txn.Then(clientv3.OpDelete(key_prefix))
	txn_res, err := txn.Commit()
	if err != nil {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_TxnCommitFailed
		cached_group_response.Msg = "Create group item unsucceed!"
		return
	}

	if !txn_res.Succeeded {
		cached_group_response.Succeed = false
		cached_group_response.Ecode = Error_TxnCommitFailed
		cached_group_response.Msg = "Txn commit failed!"
		return
	}

	cached_group_response.Succeed = true
	cached_group_response.Ecode = Error_NoError
	cached_group_response.Msg = ""
	return
}

func (keeper *CachedGroupKeeper) CheckMaxGroupIdInEtcd() (bool, error) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()
	res, err := keeper.etcd_client.Get(ctx, GroupIdPrefix, clientv3.WithCountOnly(), clientv3.WithPrefix())
	if err != nil {
		keeper.logger.Println("Checkout GroupId error:", err)
		return false, err
	}
	keeper.logger.Println("Groupid count:", res.Count)
	return res.Count >= 1, nil
}

func (keeper *CachedGroupKeeper) InitMaxGroupIdInEtcd(id int64) (bool, error) {
	init, err := keeper.CheckMaxGroupIdInEtcd()
	if err != nil {
		keeper.logger.Println("CheckGroupidIsBuild error:", err)
		return false, err
	}

	if init {
		return true, nil
	}

	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()

	val := fmt.Sprintf("%d", MaxGroupIdInitValue)
	txn := keeper.etcd_client.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(MaxGroupIdKey), "=", 0))
	txn.Then(clientv3.OpPut(MaxGroupIdKey, val))
	txn_res, err := txn.Commit()
	if err != nil {
		keeper.logger.Println("Create MaxGroupid error:", err)
		return false, err
	}

	if !txn_res.Succeeded {
		return false, nil
	}
	return true, nil
}
