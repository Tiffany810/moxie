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
	CmdAddSlotToGroup			= 	0
	CmdMoveSlotDone				= 	1
	CmdDelSlotFromGroup			= 	2
	CmdMoveSlotStart			= 	3
)

type SlotKeeper struct {
	ctx    context.Context
	server *ManagerServer
	// must be pointer to ManagerServer's logger
	logger      *log.Logger
	etcd_client *clientv3.Client
}

type SlotRequest struct {
	Cmdtype       uint32 `json:"cmd_type"`
	SourceGroupId uint64 `json:"source_id"`
	DestGroupId   uint64 `json:"dest_id"`
	SlotId        uint64 `json:"slot_id"`
}

type SlotResponse struct {
	Succeed bool   `json:"succeed"`
	Ecode   int32  `json:"ecode"`
	Msg     string `json:"msg"`
	Ext     string `json:"ext"`
}

func (keeper *SlotKeeper) ServeHTTP(response http.ResponseWriter, request *http.Request) {
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

	slot_request := &SlotRequest{}
	body, _ := ioutil.ReadAll(request.Body)
	log.Printf("slot_body:[%s]\n", body)
	if err := json.Unmarshal(body, slot_request); err != nil {
		response.WriteHeader(500)
		slot_response := &SlotResponse{
			Succeed: false,
			Ecode:   Error_InvaildArgs,
			Msg:     err.Error(),
		}
		if error_res, err := json.Marshal(slot_response); err == nil {
			response.Write(error_res)
		}
		return
	}
	log.Printf("slot_request[%v]\n", slot_request)
	keeper.DoSlotRequest(slot_request, response, request)
}

func (keeper *SlotKeeper) DoSlotRequest(slot_request *SlotRequest, response http.ResponseWriter, request *http.Request) {
	slot_response := &SlotResponse{
		Succeed: false,
		Ecode:   Error_NoError,
		Msg:     "",
	}

	switch slot_request.Cmdtype {
	case CmdAddSlotToGroup:
		keeper.DoAddSlot(slot_request, slot_response)
	case CmdDelSlotFromGroup:
		keeper.DoDelSlot(slot_request, slot_response)
	case CmdMoveSlotDone:
		keeper.DoDoneMoveSlot(slot_request, slot_response)
	case CmdMoveSlotStart:
		keeper.DoStartMoveSlot(slot_request, slot_response)
	default:
		slot_response.Msg = "Cmd not found!"
		slot_response.Succeed = false
		slot_response.Ecode = Error_CmdNotFound
	}

	if error_res, err := json.Marshal(slot_response); err == nil {
		response.Write(error_res)
	} else {
		response.WriteHeader(500)
	}
}

func (keeper *SlotKeeper) DoAddSlot(slot_request *SlotRequest, slot_response *SlotResponse) {
	if slot_request.SourceGroupId == 0 || slot_request.SlotId == 0 {
		slot_response.Succeed = false
		slot_response.Msg = "Group id or slot id is zero!"
		slot_response.Ecode = Error_SlotOrGroupIsZero
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()

	slot_key := BuildSlotIndexKey(slot_request.SlotId)
	group_key := BuildCacheGroupIdKey(slot_request.SourceGroupId)

	txn := keeper.etcd_client.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(group_key), ">", 0))
	txn.Then(clientv3.OpGet(slot_key), clientv3.OpGet(group_key))
	txn_res, err := txn.Commit()
	if !txn_res.Succeeded {
		slot_response.Succeed = false
		slot_response.Msg = "Get group failed!"
		slot_response.Ecode = Error_TxnCommitFailed
		return
	}

	for i := 0; i < len(txn_res.Responses); i++ {
		for j := 0; j < len(txn_res.Responses[i].GetResponseRange().Kvs); j++ {
			item := txn_res.Responses[i].GetResponseRange().Kvs[j]
			fmt.Printf("key[%d][%d]:%s->%s\n", i, j, item.Key, item.Value)
		}
	}

	slot_kv := txn_res.Responses[0].GetResponseRange().Kvs[0]
	var slot SlotItem
	if json.Unmarshal(slot_kv.Value, &slot) != nil {
		slot_response.Succeed = false
		slot_response.Ecode = Error_SlotJsonFormatErr
		slot_response.Msg = "Unmarshal slot value is error!"
		return
	}

	if slot.IsAdjust {
		slot_response.Succeed = false
		slot_response.Ecode = Error_SlotInAdjusting
		slot_response.Msg = "Slot is moving!"
		return
	}

	if slot.Groupid != 0 {
		slot_response.Succeed = false
		slot_response.Ecode = Error_SlotHasOwnGroup
		slot_response.Msg = "Slot has own group!"
		return
	}

	group_kv := txn_res.Responses[1].GetResponseRange().Kvs[0]
	var group CacheGroupItem
	if json.Unmarshal(group_kv.Value, &group) != nil {
		slot_response.Succeed = false
		slot_response.Ecode = Error_GroupJsonFormatErr
		slot_response.Msg = "Unmarshal group value is error!"
		return
	}

	if !group.Activated {
		slot_response.Succeed = false
		slot_response.Ecode = Error_GroupIsNotActivated
		slot_response.Msg = "Group isn't activated!"
		return
	}

	if ret, err, ecode := LocalAddSlotToGroup(&slot, &group); err != nil {
		slot_response.Succeed = ret
		slot_response.Ecode = ecode
		slot_response.Msg = err.Error()
		return
	}

	group_byte, err := json.Marshal(&group)
	if err != nil {
		keeper.logger.Panicln("Marshal Group slotid!")
	}

	slot_byte, err := json.Marshal(&slot)
	if err != nil {
		keeper.logger.Panicln("Marshal slot slotid!")
	}

	ctxn := keeper.etcd_client.Txn(ctx)
	ctxn.If(clientv3.Compare(clientv3.ModRevision(slot_key), "=", slot_kv.ModRevision),
		clientv3.Compare(clientv3.ModRevision(group_key), "=", group_kv.ModRevision))
	ctxn.Then(clientv3.OpPut(slot_key, string(slot_byte)), clientv3.OpPut(group_key, string(group_byte)))
	ctres, err := ctxn.Commit()
	if err != nil {
		slot_response.Succeed = false
		slot_response.Ecode = Error_TxnCommitFailed
		slot_response.Msg = err.Error()
		return
	}

	if !ctres.Succeeded {
		slot_response.Succeed = false
		slot_response.Ecode = Error_TxnCommitFailed
		slot_response.Msg = "Add slot to group commit unsucceed!"
		return
	}

	slot_response.Succeed = true
	slot_response.Ecode = Error_NoError
	slot_response.Msg = ""
	return
}

func (keeper *SlotKeeper) DoDelSlot(slot_request *SlotRequest, slot_response *SlotResponse) {
	if slot_request.SourceGroupId == 0 || slot_request.SlotId == 0 {
		slot_response.Succeed = false
		slot_response.Ecode = Error_SlotOrGroupIsZero
		slot_response.Msg = "Group id or slot id is zero!"
		return
	}

	slot_key := BuildSlotIndexKey(slot_request.SlotId)
	group_key := BuildCacheGroupIdKey(slot_request.SourceGroupId)

	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()

	txn := keeper.etcd_client.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(group_key), ">", 0))
	txn.Then(clientv3.OpGet(slot_key), clientv3.OpGet(group_key))
	tres, err := txn.Commit()
	if !tres.Succeeded {
		slot_response.Succeed = false
		slot_response.Ecode = Error_TxnCommitFailed
		slot_response.Msg = "Get group failed!"
		return
	}

	slotkv := tres.Responses[0].GetResponseRange().Kvs[0]
	var slot SlotItem
	if json.Unmarshal(slotkv.Value, &slot) != nil {
		slot_response.Succeed = false
		slot_response.Ecode = Error_SlotJsonFormatErr
		slot_response.Msg = "Unmarshal slot value is error!"
		return
	}

	if slot.IsAdjust {
		slot_response.Succeed = false
		slot_response.Ecode = Error_SlotInAdjusting
		slot_response.Msg = "Slot is moving!"
		return
	}

	groupkv := tres.Responses[1].GetResponseRange().Kvs[0]
	var group CacheGroupItem
	if json.Unmarshal(groupkv.Value, &group) != nil {
		slot_response.Succeed = false
		slot_response.Ecode = Error_GroupJsonFormatErr
		slot_response.Msg = "Unmarshal group value is error!"
		return
	}

	if ret, err, ecode := LocalDeleteSlotFromGroup(&slot, &group); err != nil {
		slot_response.Succeed = ret
		slot_response.Ecode = ecode
		slot_response.Msg = err.Error()
		return
	}

	group_byte, err := json.Marshal(&group)
	if err != nil {
		keeper.logger.Panicln("Marshal Group slotid!")
	}

	slot_byte, err := json.Marshal(&slot)
	if err != nil {
		keeper.logger.Panicln("Marshal slot slotid!")
	}

	ctxn := keeper.etcd_client.Txn(ctx)
	ctxn.If(clientv3.Compare(clientv3.ModRevision(slot_key), "=", slotkv.ModRevision),
		clientv3.Compare(clientv3.ModRevision(group_key), "=", groupkv.ModRevision))
	ctxn.Then(clientv3.OpPut(slot_key, string(slot_byte)),
		clientv3.OpPut(group_key, string(group_byte)))
	ctres, err := ctxn.Commit()
	if err != nil {
		slot_response.Succeed = false
		slot_response.Ecode = Error_TxnCommitFailed
		slot_response.Msg = err.Error()
		return
	}

	if !ctres.Succeeded {
		slot_response.Succeed = false
		slot_response.Ecode = Error_TxnCommitFailed
		slot_response.Msg = "Del slot from group txn commit failed!"
		return
	}

	slot_response.Succeed = true
	slot_response.Ecode = Error_NoError
	slot_response.Msg = ""
	return
}

func (keeper *SlotKeeper) DoDoneMoveSlot(slot_request *SlotRequest, slot_response *SlotResponse) {
	if slot_request.SourceGroupId == 0 || slot_request.DestGroupId == 0 || slot_request.SlotId == 0 {
		slot_response.Succeed = false
		slot_response.Ecode = Error_SlotOrGroupIsZero
		slot_response.Msg = "Group id or slot id is zero!"
		return
	}

	slot_key := BuildSlotIndexKey(slot_request.SlotId)
	src_group_key := BuildCacheGroupIdKey(slot_request.SourceGroupId)
	dest_group_key := BuildCacheGroupIdKey(slot_request.DestGroupId)

	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()

	txn := keeper.etcd_client.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(src_group_key), ">", 0),
		clientv3.Compare(clientv3.CreateRevision(dest_group_key), ">", 0))
	txn.Then(clientv3.OpGet(slot_key), clientv3.OpGet(src_group_key), clientv3.OpGet(dest_group_key))
	tres, err := txn.Commit()
	if !tres.Succeeded {
		slot_response.Succeed = false
		slot_response.Ecode = Error_TxnCommitFailed
		slot_response.Msg = "Get slot and groups failed!"
		return
	}

	for i := 0; i < len(tres.Responses); i++ {
		for j := 0; j < len(tres.Responses[i].GetResponseRange().Kvs); j++ {
			item := tres.Responses[i].GetResponseRange().Kvs[j]
			fmt.Printf("key[%d][%d]:%s->%s\n", i, j, item.Key, item.Value)
		}
	}

	slotkv := tres.Responses[0].GetResponseRange().Kvs[0]
	var slot SlotItem
	if json.Unmarshal(slotkv.Value, &slot) != nil {
		slot_response.Succeed = false
		slot_response.Ecode = Error_SlotJsonFormatErr
		slot_response.Msg = "Unmarshal slot value is error!"
		return
	}

	if !slot.IsAdjust {
		slot_response.Succeed = false
		slot_response.Ecode = Error_SlotNotInAdjusting
		slot_response.Msg = "Slot is not moving!"
		return
	}

	groupkv := tres.Responses[1].GetResponseRange().Kvs[0]
	var group CacheGroupItem
	if json.Unmarshal(groupkv.Value, &group) != nil {
		slot_response.Succeed = false
		slot_response.Ecode = Error_GroupJsonFormatErr
		slot_response.Msg = "Unmarshal src group value is error!"
		return
	}

	destgroupkv := tres.Responses[2].GetResponseRange().Kvs[0]
	var destgroup CacheGroupItem
	if json.Unmarshal(destgroupkv.Value, &destgroup) != nil {
		slot_response.Succeed = false
		slot_response.Ecode = Error_GroupJsonFormatErr
		slot_response.Msg = "Unmarshal dest group value is error!"
		return
	}

	if slot.Groupid != group.GroupId {
		slot_response.Succeed = false
		slot_response.Ecode = Error_SlotSourceIdError
		slot_response.Msg = "The slot source groupid is incorrect!"
		return
	}

	if slot.DstGroupid != destgroup.GroupId {
		slot_response.Succeed = false
		slot_response.Ecode = Error_SlotDstIdError
		slot_response.Msg = "The slot dest groupid is incorrect!"
		return
	}

	if ret, err, ecode := LocalDeleteSlotFromGroup(&slot, &group); err != nil {
		slot_response.Succeed = ret
		slot_response.Ecode = ecode
		slot_response.Msg = err.Error()
		return
	}

	if ret, err, ecode := LocalAddSlotToGroup(&slot, &destgroup); err != nil {
		slot_response.Succeed = ret
		slot_response.Ecode = ecode
		slot_response.Msg = err.Error()
		return
	}

	group_byte, err := json.Marshal(&group)
	if err != nil {
		keeper.logger.Panicln("Marshal Group slotid!")
	}

	destgroup_byte, err := json.Marshal(&destgroup)
	if err != nil {
		keeper.logger.Panicln("Marshal Group slotid!")
	}

	slot_byte, err := json.Marshal(&slot)
	if err != nil {
		keeper.logger.Panicln("Marshal slot slotid!")
	}

	ctxn := keeper.etcd_client.Txn(ctx)
	ctxn.If(clientv3.Compare(clientv3.ModRevision(slot_key), "=", slotkv.ModRevision),
		clientv3.Compare(clientv3.ModRevision(src_group_key), "=", groupkv.ModRevision),
		clientv3.Compare(clientv3.ModRevision(dest_group_key), "=", destgroupkv.ModRevision))
	ctxn.Then(clientv3.OpPut(slot_key, string(slot_byte)),
		clientv3.OpPut(src_group_key, string(group_byte)),
		clientv3.OpPut(dest_group_key, string(destgroup_byte)))
	ctres, err := ctxn.Commit()
	if err != nil {
		slot_response.Succeed = false
		slot_response.Ecode = Error_TxnCommitFailed
		slot_response.Msg = err.Error()
		return
	}

	if !ctres.Succeeded {
		keeper.logger.Println("MoveSlotToGroup Commit Unsucceed:", err)
		slot_response.Succeed = false
		slot_response.Ecode = Error_TxnCommitFailed
		slot_response.Msg = "Done move slot to group commit unsucceed!"
		return
	}

	slot_response.Succeed = true
	slot_response.Ecode = Error_NoError
	slot_response.Msg = ""
	return
}

func (keeper *SlotKeeper) DoStartMoveSlot(slot_request *SlotRequest, slot_response *SlotResponse) {
	if slot_request.DestGroupId == 0 || slot_request.SlotId == 0 || slot_request.SourceGroupId == 0 {
		slot_response.Succeed = false
		slot_response.Msg = "Group id or slot id is zero!"
		slot_response.Ecode = Error_SlotOrGroupIsZero
		return
	}

	slot_key := BuildSlotIndexKey(slot_request.SlotId)
	src_group_key := BuildCacheGroupIdKey(slot_request.SourceGroupId)
	dest_group_key := BuildCacheGroupIdKey(slot_request.DestGroupId)

	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()
	txn := keeper.etcd_client.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(src_group_key), ">", 0),
		clientv3.Compare(clientv3.CreateRevision(dest_group_key), ">", 0))
	txn.Then(clientv3.OpGet(slot_key), clientv3.OpGet(src_group_key), clientv3.OpGet(dest_group_key))
	tres, err := txn.Commit()
	if !tres.Succeeded {
		slot_response.Succeed = false
		slot_response.Msg = "Get slot and groups failed!"
		slot_response.Ecode = Error_TxnCommitFailed
		return
	}

	for i := 0; i < len(tres.Responses); i++ {
		for j := 0; j < len(tres.Responses[i].GetResponseRange().Kvs); j++ {
			item := tres.Responses[i].GetResponseRange().Kvs[j]
			fmt.Printf("key[%d][%d]:%s->%s\n", i, j, item.Key, item.Value)
		}
	}

	slotkv := tres.Responses[0].GetResponseRange().Kvs[0]
	var slot SlotItem
	if json.Unmarshal(slotkv.Value, &slot) != nil {
		slot_response.Succeed = false
		slot_response.Msg = "Unmarshal slot value is error!"
		slot_response.Ecode = Error_SlotJsonFormatErr
		return
	}

	if slot.IsAdjust {
		slot_response.Succeed = false
		slot_response.Msg = "Slot is moving!"
		slot_response.Ecode = Error_SlotInAdjusting
		return
	}

	groupkv := tres.Responses[1].GetResponseRange().Kvs[0]
	var group CacheGroupItem
	if json.Unmarshal(groupkv.Value, &group) != nil {
		slot_response.Succeed = false
		slot_response.Msg = "Unmarshal src group value is error!"
		slot_response.Ecode = Error_GroupJsonFormatErr
		return
	}

	destgroupkv := tres.Responses[2].GetResponseRange().Kvs[0]
	var destgroup CacheGroupItem
	if json.Unmarshal(destgroupkv.Value, &destgroup) != nil {
		slot_response.Succeed = false
		slot_response.Msg = "Unmarshal dst group value is error!"
		slot_response.Ecode = Error_GroupJsonFormatErr
		return
	}

	if ret, err, ecode := SlotInGroup(&slot, &group); err != nil {
		slot_response.Succeed = ret
		slot_response.Msg = err.Error()
		slot_response.Ecode = ecode
		return
	}

	slot.IsAdjust = true
	slot.DstGroupid = destgroup.GroupId

	slot_byte, err := json.Marshal(&slot)
	if err != nil {
		keeper.logger.Panicln("Marshal slot slotid!")
	}

	ctxn := keeper.etcd_client.Txn(ctx)
	ctxn.If(clientv3.Compare(clientv3.ModRevision(slot_key), "=", slotkv.ModRevision))
	ctxn.Then(clientv3.OpPut(slot_key, string(slot_byte)))
	ctres, err := ctxn.Commit()
	if err != nil {
		slot_response.Succeed = false
		slot_response.Msg = err.Error()
		slot_response.Ecode = Error_TxnCommitFailed
		return
	}

	if !ctres.Succeeded {
		slot_response.Succeed = false
		slot_response.Msg = "StartMoveSlotToGroup Commit Unsucceed"
		slot_response.Ecode = Error_TxnCommitFailed
		return
	}

	slot_response.Succeed = true
	slot_response.Msg = ""
	slot_response.Ecode = Error_NoError
	return
}
