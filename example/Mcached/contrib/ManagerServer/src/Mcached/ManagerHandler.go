package Mcached;

import (
	"encoding/json"
	"os"
	"os/signal"
	"log"
	"time"
	"context"
	"sync"
	"fmt"
	"net/http"
	"errors"
	"go.etcd.io/etcd/clientv3"
	"go.etcd.io/etcd/clientv3/concurrency"
)

const (
	HtmlPath						= "../index/"
	IndexFileName					= "../index/index.html"
)

const (
	SlotsPrefix						= "/Mcached/Slots/"
	CachedGroupPrefix				= "/Mcached/CachedGroup/"
	GroupIdPrefix					= "/Mcached/GroupId/"		
	ManagerMasterPrefix				= "/Mcached/ManagerMaster/"
	MaxGroupIdKey					= "/Mcached/GroupId/MaxGroupId"		
	GroupRevisePrefix				= "/Mcached/GroupRevise/"
	CacheGroupKeepAlivePrefix		= "/Mcached/EndPoints/"
	CachedGroupMasterPrefix			= "/Mcached/EndPoints/master/"
	CachedGroupSlavePrefix			= "/Mcached/EndPoints/slave/"
)

const (
	ManagerServerTickTimeout		= 200 * time.Millisecond
	EtcdGetRequestTimeout			= 5 * time.Second
	UpdateSlotsCacheGroupDela		= 60 * time.Second
	LeaderSessionLeaseTTL			= 1
)

type McachedConf struct {
	LocalAddr 				string
	KeeperAddr				string
	EtcdAddr 				[]string
	LogFile 				string
}

type LeaderContext struct {
	ContextMutex 			sync.Mutex
	IsCampaign 				bool
	IsLeader 				bool
	Leader 					string
}

type ManagerServer struct {
	Mconfig						McachedConf
	quit 						chan os.Signal
	logger 						*log.Logger

	ComTick 					*time.Ticker
	cancel 						context.CancelFunc
	ctx 						context.Context

	EtcdClientv3 				*clientv3.Client

	LeaderCtx 					*LeaderContext
	session 					*concurrency.Session
	electc 						chan *concurrency.Election
	election 					*concurrency.Election
	elecerr 					chan error
	elecerrcount 				uint32

	HttpServer					*http.Server
	HttpRedirect 				uint64
	RedirectMutex 				sync.Mutex

	keeper						*SlotGroupKeeper

	SlotsGroupidChecked 		bool
	SgCheckmutex 				sync.Mutex
}

func (server *ManagerServer) Init(cfg *McachedConf) bool {
	if cfg == nil {
		log.Fatal("The arg of ManagerServer.Init cfg is nil!")
		return false;
	}

	server.Mconfig = *cfg;
	if server.Mconfig.LogFile != "" {
		logFile, err  := os.Create(server.Mconfig.LogFile)
		if err != nil {
			log.Fatal("Create logfile failed error:", err)
			return false
		}
		server.logger = log.New(logFile, "", log.Llongfile)
	}

	server.quit = make(chan os.Signal)
	if server.quit == nil {
		log.Fatal("Create Server quit channel failed!")
		return false;
	}
	signal.Notify(server.quit, os.Interrupt)

	server.ComTick = time.NewTicker(ManagerServerTickTimeout)
	server.ctx, server.cancel = context.WithCancel(context.Background())

	cli, err := clientv3.New(clientv3.Config{
					Endpoints:   cfg.EtcdAddr,
					DialTimeout: time.Second * 1,
				})
	if err != nil {
		server.logger.Println("Create etcdclientv3 error:", err)
	}
	server.EtcdClientv3 = cli

	server.LeaderCtx = &LeaderContext {
				IsCampaign 	: false,
				IsLeader 	: false,
				Leader		: "",
			}

	server.electc = make(chan *concurrency.Election, 2)
	els, err := concurrency.NewSession(server.EtcdClientv3, 
						concurrency.WithTTL(LeaderSessionLeaseTTL),
						concurrency.WithContext(server.ctx))
	if err != nil {
		server.logger.Println("NewSession error:", err)
		return false
	}
	server.session = els
	server.election = concurrency.NewElection(server.session, ManagerMasterPrefix)
	server.elecerr = make(chan error, 2)
	server.elecerrcount = 0

	kcfg := &SGConfig {
		EtcdEndpoints : server.Mconfig.EtcdAddr,
		HttpEndpoints : server.Mconfig.KeeperAddr,
		Klogger       : server.logger,
	}
	server.keeper, err = NewSlotGroupKeeper(server.ctx, kcfg)
	if err != nil {
		server.logger.Panicln("NewSlotGroupKeeper failed!")
	}

	server.RegisterHttpHandler() 
	server.HttpRedirect = 0

	server.SetSlotsGroupidChecked(false)
	return true;
}

func (server *ManagerServer) RegisterHttpHandler() {
	mux := http.NewServeMux()
	gr := &GroupReviseHandler {
		server : server,
	}
	mux.Handle(GroupRevisePrefix, gr)

	ka := &GroupKeepAliveHandler {
		server : server,
	}
	mux.Handle(CacheGroupKeepAlivePrefix, ka)

	sh := &SlotsListHandler {
		Keeper : server.keeper,
	}
	mux.Handle(SlotsPrefix, sh)
	mux.Handle(SlotsPrefix + "/", sh)

	ch := &CachedGroupListHandler {
		Keeper : server.keeper,
	}
	mux.Handle(CachedGroupPrefix, ch)
	mux.Handle(CachedGroupPrefix + "/", ch)

	ih := &IndexHandler {
		Keeper : server.keeper,
	}
	mux.Handle("/", ih)

	server.HttpServer = &http.Server {
		Addr : server.Mconfig.LocalAddr,
		WriteTimeout : time.Second * 2,
		Handler : mux,
	}
}

func (server *ManagerServer) AddRedirectCount() {
	server.RedirectMutex.Lock()
	defer server.RedirectMutex.Unlock()
	server.HttpRedirect++
}

func (server *ManagerServer) GetRedirectCount() uint64 {
	server.RedirectMutex.Lock()
	defer server.RedirectMutex.Unlock()
	return server.HttpRedirect
}

func (server *ManagerServer) Run() {
	server.logger.Println("McachedManager server running!")
	go func() {
		<-server.quit
		server.cancel()
	}()

	go func () {
		server.keeper.Start()
	}()

	go func() {
		err := server.HttpServer.ListenAndServe()
		if err != nil {
			if err == http.ErrServerClosed {
				server.logger.Println("Server closed under request")
			} else {
				server.logger.Println("Server closed unexpected", err)
			}
		}
		server.logger.Println("SlotGroupKeeper Server exited!")
	}()

	for {
		select {
		case <-server.ComTick.C:
			server.Tick()
		case <-server.electc:
			server.MaybeLeader()
		case <-server.elecerr:
			server.elecerrcount++
		case <-server.ctx.Done():
			return ;
		}
	}
}

func (server *ManagerServer) Tick () {
	lr, err := server.election.Leader(server.ctx)
	if err != nil {
		if err == concurrency.ErrElectionNoLeader {
			server.Campaign()
		} else {
			server.logger.Println("Get leader error:", err)
		}
	} else {
		// maybe start later, the leader is exist! should update campaign!
		server.MaybeUpdateElectCtx(false, 
						string(lr.Kvs[0].Value) == server.Mconfig.LocalAddr, 
						string(lr.Kvs[0].Value))
	}
}

func (server *ManagerServer) MaybeLeader() {
	server.LeaderCtx.ContextMutex.Lock()
	defer server.LeaderCtx.ContextMutex.Unlock()

	if !server.LeaderCtx.IsCampaign {
		return
	}

	server.LeaderCtx.IsCampaign = false
	leader, err := server.election.Leader(server.ctx)
	if err != nil {
		server.logger.Println("Electtion get Leader error:", err)
		return
	}

	if string(leader.Kvs[0].Value) == "" {
		server.logger.Println("Leader msg is empty!")
	}

	server.LeaderCtx.IsLeader = bool(string(leader.Kvs[0].Value) == server.Mconfig.LocalAddr)
	server.LeaderCtx.Leader = string(leader.Kvs[0].Value)

	server.logger.Println("Leader:", server.LeaderCtx.Leader)

	if server.LeaderCtx.IsLeader && !server.GetCheckSlotGroupid() {
		b1, _ := server.MaybeInitSlots()
		b2, _  := server.MaybeUpdateMaxGroupid(0)
		if b1 && b2 {
			server.SetSlotsGroupidChecked(true)
		}
	}
}

func (server *ManagerServer) MaybeUpdateElectCtx(iscam, isleader bool, leader string) {
	server.LeaderCtx.ContextMutex.Lock()
	defer server.LeaderCtx.ContextMutex.Unlock()

	if server.LeaderCtx.IsCampaign {
		server.logger.Println("IsCampaign:^_^")
		return
	}

	server.LeaderCtx.IsCampaign = iscam
	server.LeaderCtx.IsLeader = isleader
	server.LeaderCtx.Leader = leader
}

func (server *ManagerServer) Campaign() {
	server.LeaderCtx.ContextMutex.Lock()
	defer server.LeaderCtx.ContextMutex.Unlock()

	if server.LeaderCtx.IsCampaign {
		return
	}

	server.LeaderCtx.IsCampaign = true
	server.LeaderCtx.IsLeader = false
	server.LeaderCtx.Leader = ""

	go func() {
		ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
		defer cancel()
		if err := server.election.Campaign(ctx, server.Mconfig.LocalAddr); err != nil {
			server.logger.Println("Election Campaign error:", err)
			server.elecerr <- err
		}
		server.electc <- server.election
	}()
}

func (server *ManagerServer) IsLeader() (bool, string, error) {
	server.LeaderCtx.ContextMutex.Lock()
	defer server.LeaderCtx.ContextMutex.Unlock()

	if server.LeaderCtx.IsCampaign {
		return false, "", concurrency.ErrElectionNoLeader
	}

	return server.LeaderCtx.IsLeader, server.LeaderCtx.Leader, nil
}

func (server *ManagerServer) GetCheckSlotGroupid() bool {
	server.SgCheckmutex.Lock()
	defer server.SgCheckmutex.Unlock()
	return server.SlotsGroupidChecked
}

func (server *ManagerServer) SetSlotsGroupidChecked(val bool) {
	server.SgCheckmutex.Lock()
	defer server.SgCheckmutex.Unlock()
	server.SlotsGroupidChecked = val
}

func (server *ManagerServer) CheckGroupidIsBuild() (bool, error) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()
	res, err := server.EtcdClientv3.Get(ctx, GroupIdPrefix, clientv3.WithCountOnly(), clientv3.WithPrefix())
	if err != nil {
		server.logger.Println("Checkout GroupId error:", err)
		return false, err
	}
	server.logger.Println("Groupid count:", res.Count)
	return res.Count >= 1, nil
}

func (server *ManagerServer) MaybeUpdateMaxGroupid(id int64) (bool, error) {
	init, err := server.CheckGroupidIsBuild()
	if err != nil {
		server.logger.Println("CheckGroupidIsBuild error:", err)
	}
	if init {
		return true, nil
	}
	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()
	// create MaxGroupid
	val := fmt.Sprintf("%d", id)
	_, err = server.EtcdClientv3.Put(ctx, MaxGroupIdKey, val)
	if err != nil {
		server.logger.Println("Create MaxGroupid error:", err)
		return false, err
	}
	return true, nil
}

func (server *ManagerServer) CheckSlotIsBuild() (bool, error) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()
	res, err := server.EtcdClientv3.Get(ctx, SlotsPrefix, clientv3.WithCountOnly(), clientv3.WithPrefix())
	if err != nil {
		server.logger.Println("Checkout Slots error:", err)
		return false, err
	}
	server.logger.Println("Slots count:", res.Count)
	return res.Count >= McachedSlotsEnd - McachedSlotsStart, nil
}

func (server *ManagerServer) MaybeInitSlots() (bool, error) {
	init, err := server.CheckSlotIsBuild()
	if err != nil {
		server.logger.Println("CheckSlotIsBuild error:", err)
	}

	if init {
		return true, nil
	}

	// createslots
	for i := McachedSlotsStart; i < McachedSlotsEnd; i++ {
		if succ, err := server.CreateSlot(i); succ != true {
			return succ, err
		}
	}
	return true, nil
}

func (server *ManagerServer) CreateSlot(index int) (bool, error) {
	if index < 0 {
		server.logger.Println("Create negative slot!")
	}
	slot := &SlotItem {
		Index : uint64(index),
		Groupid : 0,
		IsAdjust: false,
		DstGroupid:0,
	}
	return server.CreateSlotItem(slot)
}

func (server *ManagerServer) GetSlotsIndexKey(index uint64) string {
	key_prefix := fmt.Sprintf("%s%d", SlotsPrefix, index)
	return key_prefix
}

func (server *ManagerServer) CreateSlotItem (slot *SlotItem) (bool, error) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()
	if (slot.Index == 0) {
		server.logger.Println("Slot Index is zero!")
	}

	key_prefix := server.GetSlotsIndexKey(slot.Index)
	slot_byte, err := json.Marshal(slot)
	if err != nil {
		server.logger.Println("Json Marshal slot item error:", err)
		return false, err
	}
	
	_, err = server.EtcdClientv3.Put(ctx, key_prefix, string(slot_byte))

	if err != nil {
		server.logger.Println("Create slot item unsucceed!")
		return false, err
	}

	return true, nil
}

func (server *ManagerServer) GetMaxGroupId() (uint64, error) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()
	res, err := server.EtcdClientv3.Get(ctx, MaxGroupIdKey)
	if err != nil {
		server.logger.Println("Checkout GroupId error:", err)
		return 0, err
	}
	if len(res.Kvs) == 0 {
		server.logger.Panicln("Can't found max group id!")
	}
	var ret uint64
	fmt.Sscanf(string(res.Kvs[0].Value), "%d", &ret)
	return ret, nil
}

func (server *ManagerServer) GetCacheGroupIdKey(id uint64) string {
	key_prefix := fmt.Sprintf("%s%d", CachedGroupPrefix, id)
	return key_prefix
}

func (server *ManagerServer) ActivateCachedGroup (group *CacheGroupItem) (bool, error, int32) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()

	key_prefix := server.GetCacheGroupIdKey(group.GroupId)
	server.logger.Println("New groupid key:", key_prefix)

	txn := server.EtcdClientv3.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(key_prefix), ">", 0))
	txn.Then(clientv3.OpGet(key_prefix))
	tres, err := txn.Commit()
	if !tres.Succeeded {
		return false, errors.New("Get group failed!"), Error_TxnCommitFailed
	}

	for i := 0; i < len(tres.Responses); i++ {
		for j := 0; j < len(tres.Responses[i].GetResponseRange().Kvs); j++ {
			item := tres.Responses[i].GetResponseRange().Kvs[j]
			fmt.Printf("key[%d][%d]:%s->%s\n", i, j, item.Key, item.Value)
		}
	}

	groupkv := tres.Responses[0].GetResponseRange().Kvs[0]
	if json.Unmarshal(groupkv.Value, &group) != nil {
		return false, errors.New("Unmarshal group value is error!"), Error_GroupJsonFormatErr
	}

	if (group.Activated) {
		return false, errors.New("Group isn't Activated!"), Error_GroupIsActivated
	}

	group.Activated = true

	group_byte, err := json.Marshal(&group)
	if err != nil {
		server.logger.Panicln("Marshal Group slotid!")
	}

	ctxn := server.EtcdClientv3.Txn(ctx)
	ctxn.If(clientv3.Compare(clientv3.ModRevision(key_prefix), "=", groupkv.ModRevision))
	ctxn.Then(clientv3.OpPut(key_prefix, string(group_byte)))
	ctres, err := ctxn.Commit()
	if err != nil {
		server.logger.Println("ActivateCachedGroup Commit error:", err)
		return false, err, Error_TxnCommitFailed
	}

	if !ctres.Succeeded {
		server.logger.Println("ActivateCachedGroup Commit Unsucceed:", err)
		return false, errors.New("ActivateCachedGroup Commit Unsucceed"), Error_TxnCommitFailed
	}

	return true, nil, Error_NoError
}

func (server *ManagerServer) CreateCachedGroup (group *CacheGroupItem) (bool, error, int32) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()

	oldid, err := server.GetMaxGroupId()
	if err != nil {
		return false, err, Error_GetMaxGroupIdFailed
	}
	newid := oldid + 1

	key_prefix := server.GetCacheGroupIdKey(newid)
	server.logger.Println("New groupid key:", key_prefix)
	group.GroupId = newid
	group.Activated = false;
	group_byte, err := json.Marshal(group)
	if err != nil {
		server.logger.Println("Json Marshal group item error:", err)
		return false, err, Error_GroupJsonFormatErr
	}

	OldIdPlusValue := fmt.Sprintf("%d", oldid)
	NewIdPlusValue := fmt.Sprintf("%d", newid)

	txn := server.EtcdClientv3.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(key_prefix), "=", 0),
			clientv3.Compare(clientv3.Value(MaxGroupIdKey), "=", OldIdPlusValue))
	txn.Then(clientv3.OpPut(key_prefix, string(group_byte)),
			clientv3.OpPut(MaxGroupIdKey, NewIdPlusValue))
	tres, err := txn.Commit()
	if err != nil {
		server.logger.Println("Create group item unsucceed!")
		return false, err, Error_TxnCommitFailed
	}

	if !tres.Succeeded {
		server.logger.Println(tres.OpResponse())
		server.logger.Println(tres.Header)
	}

	return tres.Succeeded, nil, Error_NoError
}

func (server *ManagerServer) DeleteCachedGroup (id uint64) (bool, error, int32) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()
	if (id == 0) {
		server.logger.Panicln("Group id is zero!")
	}

	key_prefix := server.GetCacheGroupIdKey(id)

	judge_empty := server.EtcdClientv3.Txn(ctx)
	judge_empty.If(clientv3.Compare(clientv3.CreateRevision(key_prefix), "!=", 0))
	judge_empty.Then(clientv3.OpGet(key_prefix))
	jres, err := judge_empty.Commit()
	if err != nil {
		return false, err, Error_TxnCommitFailed
	}

	if !jres.Succeeded {
		return false, errors.New("Judge group empty failed!"), Error_TxnCommitFailed
	}

	var cache CacheGroupItem
	if err := json.Unmarshal(jres.Responses[0].GetResponseRange().Kvs[0].Value, &cache); err != nil {
		return false, err, Error_GroupJsonFormatErr
	}

	if len(cache.SlotsIndex) > 0 {
		return false, errors.New("Group isn't empty!"), Error_GroupNotEmpty
	}

	txn := server.EtcdClientv3.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(key_prefix), "!=", 0))
	txn.Then(clientv3.OpDelete(key_prefix))
	tres, err := txn.Commit()
	if err != nil {
		server.logger.Println("Create group item unsucceed!")
		return false, err, Error_TxnCommitFailed
	}

	if !tres.Succeeded {
		return false, errors.New("Transaction execute failed!"), Error_TxnCommitFailed
	}

	return true, nil, Error_NoError
}

func (server *ManagerServer) AddSlotToGroup (slotid, groupid uint64) (bool, error, int32) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()
	if (groupid == 0 || slotid == 0) {
		return false, errors.New("Group id or slot id is zero!"), Error_SlotOrGroupIsZero
	}

	slot_key := server.GetSlotsIndexKey(slotid)
	group_key := server.GetCacheGroupIdKey(groupid)

	txn := server.EtcdClientv3.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(group_key), ">", 0))
	txn.Then(clientv3.OpGet(slot_key), clientv3.OpGet(group_key))
	tres, err := txn.Commit()
	if !tres.Succeeded {
		return false, errors.New("Get group failed!"), Error_TxnCommitFailed
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
		return false, errors.New("Unmarshal slot value is error!"), Error_SlotJsonFormatErr
	}

	if slot.IsAdjust {
		return false, errors.New("Slot is moving!"), Error_SlotInAdjusting
	}

	if slot.Groupid != 0 {
		return false, errors.New("Slot has own group!"), Error_SlotHasOwnGroup
	}

	groupkv := tres.Responses[1].GetResponseRange().Kvs[0]
	var group CacheGroupItem
	if json.Unmarshal(groupkv.Value, &group) != nil {
		return false, errors.New("Unmarshal group value is error!"), Error_GroupJsonFormatErr
	}

	if (!group.Activated) {
		return false, errors.New("Group isn't Activated!"), Error_GroupIsNotActivated
	}
	
	if ret, err, ecode := LocalAddSlotToGroup(&slot, &group); err != nil {
		return ret, err, ecode
	}

	group_byte , err := json.Marshal(&group)
	if err != nil {
		server.logger.Panicln("Marshal Group slotid!")
	}

	slot_byte, err := json.Marshal(&slot)
	if err != nil {
		server.logger.Panicln("Marshal slot slotid!")
	}

	ctxn := server.EtcdClientv3.Txn(ctx)
	ctxn.If(clientv3.Compare(clientv3.ModRevision(slot_key) , "=", slotkv.ModRevision),
			clientv3.Compare(clientv3.ModRevision(group_key), "=", groupkv.ModRevision))
	ctxn.Then(clientv3.OpPut(slot_key, string(slot_byte)), clientv3.OpPut(group_key, string(group_byte)))
	ctres, err := ctxn.Commit()
	if err != nil {
		server.logger.Println("AddSlotToGroup Commit error:", err)
		return false, err, Error_TxnCommitFailed
	}

	if !ctres.Succeeded {
		server.logger.Println("AddSlotToGroup Commit Unsucceed:", err)
		return false, errors.New("AddSlotToGroup Commit Unsucceed"), Error_TxnCommitFailed
	}

	return true, nil, Error_NoError
}

func (server *ManagerServer) DeleteSlotfromGroup (slotid, groupid uint64) (bool, error, int32) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()
	if (groupid == 0 || slotid == 0) {
		return false, errors.New("Group id or slot id is zero!"), Error_SlotOrGroupIsZero	
	}

	slot_key := server.GetSlotsIndexKey(slotid)
	group_key := server.GetCacheGroupIdKey(groupid)

	txn := server.EtcdClientv3.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(group_key), ">", 0))
	txn.Then(clientv3.OpGet(slot_key), clientv3.OpGet(group_key))
	tres, err := txn.Commit()
	if !tres.Succeeded {
		return false, errors.New("Get group failed!"), Error_TxnCommitFailed
	}

	slotkv := tres.Responses[0].GetResponseRange().Kvs[0]
	var slot SlotItem
	if json.Unmarshal(slotkv.Value, &slot) != nil {
		return false, errors.New("Unmarshal slot value is error!"), Error_SlotJsonFormatErr
	}

	if slot.IsAdjust {
		return false, errors.New("Slot is moving!"), Error_SlotNotInAdjusting
	}

	groupkv := tres.Responses[1].GetResponseRange().Kvs[0]
	var group CacheGroupItem
	if json.Unmarshal(groupkv.Value, &group) != nil {
		return false, errors.New("Unmarshal group value is error!"), Error_GroupJsonFormatErr
	}

	if ret, err , ecode:= DeleteSlotFromGroup(&slot, &group); err != nil {
		return ret, err, ecode
	}

	group_byte , err := json.Marshal(&group)
	if err != nil {
		server.logger.Panicln("Marshal Group slotid!")
	}

	slot_byte, err := json.Marshal(&slot)
	if err != nil {
		server.logger.Panicln("Marshal slot slotid!")
	}

	ctxn := server.EtcdClientv3.Txn(ctx)
	ctxn.If(clientv3.Compare(clientv3.ModRevision(slot_key) , "=", slotkv.ModRevision),
			clientv3.Compare(clientv3.ModRevision(group_key), "=", groupkv.ModRevision))
	ctxn.Then(clientv3.OpPut(slot_key, string(slot_byte)), 
			clientv3.OpPut(group_key, string(group_byte)))
	ctres, err := ctxn.Commit()
	if err != nil {
		server.logger.Println("AddSlotToGroup Commit error:", err)
		return false, err, Error_TxnCommitFailed
	}

	if !ctres.Succeeded {
		server.logger.Println("AddSlotToGroup Commit Unsucceed:", err)
		return false, errors.New("AddSlotToGroup Commit Unsucceed"), Error_TxnCommitFailed
	}

	return true, nil, Error_NoError
}

func (server *ManagerServer) MoveSlotToGroup (slotid, srcid, destid uint64) (bool, error, int32) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()
	if (srcid == 0 || destid == 0 || slotid == 0) {
		return false, errors.New("Group id or slot id is zero!"), Error_SlotOrGroupIsZero
	}

	slot_key := server.GetSlotsIndexKey(slotid)
	src_group_key := server.GetCacheGroupIdKey(srcid)
	dest_group_key := server.GetCacheGroupIdKey(destid)

	txn := server.EtcdClientv3.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(src_group_key), ">", 0),
			clientv3.Compare(clientv3.CreateRevision(dest_group_key), ">", 0))
	txn.Then(clientv3.OpGet(slot_key), clientv3.OpGet(src_group_key), clientv3.OpGet(dest_group_key))
	tres, err := txn.Commit()
	if !tres.Succeeded {
		return false, errors.New("Get slot and groups failed!"), Error_TxnCommitFailed
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
		return false, errors.New("Unmarshal slot value is error!"), Error_SlotJsonFormatErr
	}

	if !slot.IsAdjust {
		return false, errors.New("Slot is not moving!"), Error_SlotNotInAdjusting
	}

	groupkv := tres.Responses[1].GetResponseRange().Kvs[0]
	var group CacheGroupItem
	if json.Unmarshal(groupkv.Value, &group) != nil {
		return false, errors.New("Unmarshal src group value is error!"), Error_GroupJsonFormatErr
	}

	destgroupkv := tres.Responses[2].GetResponseRange().Kvs[0]
	var destgroup CacheGroupItem
	if json.Unmarshal(destgroupkv.Value, &destgroup) != nil {
		return false, errors.New("Unmarshal dest group value is error!"), Error_GroupJsonFormatErr
	}

	if slot.Groupid != group.GroupId {
		return false, errors.New("The slot source groupid is incorrect!"), Error_SlotSourceIdError
	}

	if slot.DstGroupid != destgroup.GroupId {
		return false, errors.New("The slot dest groupid is incorrect!"), Error_SlotDstIdError
	}

	if ret, err, ecode := DeleteSlotFromGroup(&slot, &group); err != nil {
		return ret, err, ecode
	}

	if ret, err, ecode := LocalAddSlotToGroup(&slot, &destgroup); err != nil {
		return ret, err, ecode
	}

	group_byte , err := json.Marshal(&group)
	if err != nil {
		server.logger.Panicln("Marshal Group slotid!")
	}

	destgroup_byte , err := json.Marshal(&destgroup)
	if err != nil {
		server.logger.Panicln("Marshal Group slotid!")
	}

	slot_byte, err := json.Marshal(&slot)
	if err != nil {
		server.logger.Panicln("Marshal slot slotid!")
	}

	ctxn := server.EtcdClientv3.Txn(ctx)
	ctxn.If(clientv3.Compare(clientv3.ModRevision(slot_key) , "=", slotkv.ModRevision),
			clientv3.Compare(clientv3.ModRevision(src_group_key), "=", groupkv.ModRevision),
			clientv3.Compare(clientv3.ModRevision(dest_group_key), "=", destgroupkv.ModRevision))
	ctxn.Then(clientv3.OpPut(slot_key, string(slot_byte)),
			 clientv3.OpPut(src_group_key, string(group_byte)),
			 clientv3.OpPut(dest_group_key, string(destgroup_byte)))
	ctres, err := ctxn.Commit()
	if err != nil {
		server.logger.Println("MoveSlotToGroup Commit error:", err)
		return false, err, Error_TxnCommitFailed
	}

	if !ctres.Succeeded {
		server.logger.Println("MoveSlotToGroup Commit Unsucceed:", err)
		return false, errors.New("MoveSlotToGroup Commit Unsucceed"), Error_TxnCommitFailed
	}

	return true, nil, Error_NoError
}

func (server *ManagerServer) StartMoveSlotToGroup (slotid, srcid, destid uint64) (bool, error, int32) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()
	if (srcid == 0 || destid == 0 || slotid == 0) {
		return false, errors.New("Group id or slot id is zero!"), Error_SlotOrGroupIsZero
	}

	slot_key := server.GetSlotsIndexKey(slotid)
	src_group_key := server.GetCacheGroupIdKey(srcid)
	dest_group_key := server.GetCacheGroupIdKey(destid)

	txn := server.EtcdClientv3.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(src_group_key), ">", 0),
			clientv3.Compare(clientv3.CreateRevision(dest_group_key), ">", 0))
	txn.Then(clientv3.OpGet(slot_key), clientv3.OpGet(src_group_key), clientv3.OpGet(dest_group_key))
	tres, err := txn.Commit()
	if !tres.Succeeded {
		return false, errors.New("Get slot and groups failed!"), Error_TxnCommitFailed
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
		return false, errors.New("Unmarshal slot value is error!"), Error_SlotJsonFormatErr
	}

	if slot.IsAdjust {
		return false, errors.New("Slot is moving!"), Error_SlotInAdjusting
	}

	groupkv := tres.Responses[1].GetResponseRange().Kvs[0]
	var group CacheGroupItem
	if json.Unmarshal(groupkv.Value, &group) != nil {
		return false, errors.New("Unmarshal src group value is error!"), Error_GroupJsonFormatErr
	}

	destgroupkv := tres.Responses[2].GetResponseRange().Kvs[0]
	var destgroup CacheGroupItem
	if json.Unmarshal(destgroupkv.Value, &destgroup) != nil {
		return false, errors.New("Unmarshal dst group value is error!"), Error_GroupJsonFormatErr
	}

	if ret, err, ecode:= SlotInGroup(&slot, &group); err != nil {
		return ret, err, ecode
	}

	slot.IsAdjust = true
	slot.DstGroupid = destgroup.GroupId

	slot_byte, err := json.Marshal(&slot)
	if err != nil {
		server.logger.Panicln("Marshal slot slotid!")
	}

	ctxn := server.EtcdClientv3.Txn(ctx)
	ctxn.If(clientv3.Compare(clientv3.ModRevision(slot_key) , "=", slotkv.ModRevision))
	ctxn.Then(clientv3.OpPut(slot_key, string(slot_byte)))
	ctres, err := ctxn.Commit()
	if err != nil {
		server.logger.Println("StartMoveSlotToGroup Commit error:", err)
		return false, err, Error_TxnCommitFailed
	}

	if !ctres.Succeeded {
		server.logger.Println("StartMoveSlotToGroup Commit Unsucceed:", err)
		return false, errors.New("StartMoveSlotToGroup Commit Unsucceed"), Error_TxnCommitFailed
	}

	return true, nil, Error_NoError
}

func (server *ManagerServer) GetGroupKeepAliveMasterKey(groupid uint64, host string) string {
	key_prefix := fmt.Sprintf("%s%d/%s", CachedGroupMasterPrefix, groupid, host)
	return key_prefix
}

func (server *ManagerServer) GetGroupKeepAliveSlaveKey(groupid uint64, host string) string {
	key_prefix := fmt.Sprintf("%s%d/%s", CachedGroupSlavePrefix, groupid, host)
	return key_prefix
}

func (server *ManagerServer) GetGroupKeepAliveMasterPrefix(groupid uint64) string {
	key_prefix := fmt.Sprintf("%s%d", CachedGroupMasterPrefix, groupid)
	return key_prefix
}

func (server *ManagerServer) GetGroupKeepAliveSlavePrefix(groupid uint64) string {
	key_prefix := fmt.Sprintf("%s%d", CachedGroupSlavePrefix, groupid)
	return key_prefix
}

func (server *ManagerServer) KeepAliveCachedGroup(request *GroupKeepAliveRequest) (bool, error, int32) {
	if request == nil {
		server.logger.Panicln("Request is nil!")
	}
	var key_prefix string
	if request.Master {
		key_prefix = server.GetGroupKeepAliveMasterKey(request.Groupid, request.Addr)
	} else {
		key_prefix = server.GetGroupKeepAliveSlaveKey(request.Groupid, request.Addr)
	}
	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()

	resp, err := server.EtcdClientv3.Grant(ctx, 5)
	if err != nil {
		server.logger.Println("Grant lease failed:", err)
		return false, err, Error_ServerInternelError
	}

	_, err = server.EtcdClientv3.Put(ctx, key_prefix, request.Addr, clientv3.WithLease(resp.ID))
	if err != nil {
		server.logger.Println("Upadte key lease failed:", err)
		return false, err, Error_ServerInternelError
	}
	return true, nil, Error_NoError
}

func (server *ManagerServer) PeeksCachedGroup(request *GroupReviseRequest, response *GroupReviseResponse) (bool, error, int32) {
	if request == nil || response == nil {
		server.logger.Panicln("Request or response is nil!")
	}

	master_key_prefix := server.GetGroupKeepAliveMasterPrefix(request.SourceGroupId)
	slave_key_prefix := server.GetGroupKeepAliveSlavePrefix(request.SourceGroupId)

	ctx, cancel := context.WithTimeout(context.Background(), EtcdGetRequestTimeout)
	defer cancel()

	mres , err := server.EtcdClientv3.Get(ctx, master_key_prefix, clientv3.WithPrefix())
	if err != nil {
		server.logger.Printf("Get key %s error:%v", master_key_prefix, err)
		return false, err, Error_ServerInternelError
	}

	sres , err := server.EtcdClientv3.Get(ctx, slave_key_prefix, clientv3.WithPrefix())
	if err != nil {
		server.logger.Printf("Get key %s error:%v", slave_key_prefix, err)
		return false, err, Error_ServerInternelError
	}

	ext := ""
	// add master add to ext
	for i := 0; i < len(mres.Kvs); i++ {
		ext += string(mres.Kvs[i].Value) + ";"
	}

	ext += "|"

	for i := 0; i < len(sres.Kvs); i++ {
		ext += string(sres.Kvs[i].Value) + ";"
	}

	ext += "|"

	response.Ext = ext

	return true, nil, Error_NoError
}





