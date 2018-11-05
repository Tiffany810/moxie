package Mcached

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"go.etcd.io/etcd/clientv3"
	"go.etcd.io/etcd/clientv3/concurrency"
	"log"
	"net/http"
	"os"
	"os/signal"
	"sync"
	"time"
)

const (
	ManagerServerTickTimeout = 200 * time.Millisecond
	EtcdGetRequestTimeout    = 5 * time.Second
	LeaderSessionLeaseTTL    = 1
)

type McachedConf struct {
	LocalAddr  string
	KeeperAddr string
	EtcdAddr   []string
	LogFile    string
}

type LeaderContextNoSafe struct {
	IsLeader        bool
	LeaderVal       string
	LeaderKey       string
	LeaderCreateRev int64
	LeaderModRev    int64
}

type LeaderContext struct {
	ContextMutex sync.Mutex
	LeaderContextNoSafe
}

type ManagerServer struct {
	config McachedConf
	quit   chan os.Signal
	logger *log.Logger

	comTicker *time.Ticker
	cancel    context.CancelFunc
	ctx       context.Context

	etcdClient *clientv3.Client

	leaderCtx     *LeaderContext
	session       *concurrency.Session
	electChan     chan *concurrency.Election
	election      *concurrency.Election
	elecErrorChan chan error
	elecNotify    chan bool

	HttpServer *http.Server

	metaDataCheked      bool
	metaDataChekedMutex sync.Mutex

	campaign      bool
	campaignMutex sync.Mutex
}

func (server *ManagerServer) Init(cfg *McachedConf) bool {
	if cfg == nil {
		log.Fatal("The arg of ManagerServer.Init cfg is nil!")
		return false
	}

	server.config = *cfg
	if server.config.LogFile != "" {
		logFile, err := os.Create(server.config.LogFile)
		if err != nil {
			log.Fatal("Create logfile failed error:", err)
			return false
		}
		server.logger = log.New(logFile, "", log.Llongfile)
	}

	server.quit = make(chan os.Signal)
	if server.quit == nil {
		log.Fatal("Create Server quit channel failed!")
		return false
	}
	signal.Notify(server.quit, os.Interrupt)

	server.comTicker = time.NewTicker(ManagerServerTickTimeout)
	server.ctx, server.cancel = context.WithCancel(context.Background())

	cli, err := clientv3.New(clientv3.Config{
		Endpoints:   cfg.EtcdAddr,
		DialTimeout: time.Second * 1,
	})

	if err != nil {
		server.logger.Println("Create etcdclientv3 error:", err)
	}
	server.etcdClient = cli

	server.leaderCtx = &LeaderContext{
		LeaderContextNoSafe: LeaderContextNoSafe{
			LeaderCreateRev: 0,
			LeaderKey:       "",
			LeaderModRev:    0,
			LeaderVal:       "",
		},
	}

	server.electChan = make(chan *concurrency.Election, 2)
	els, err := concurrency.NewSession(server.etcdClient,
		concurrency.WithTTL(LeaderSessionLeaseTTL),
		concurrency.WithContext(server.ctx))
	if err != nil {
		server.logger.Println("NewSession error:", err)
		return false
	}
	server.session = els
	server.election = concurrency.NewElection(server.session, ManagerMasterPrefix)
	server.elecErrorChan = make(chan error, 2)

	server.elecNotify = make(chan bool)
	if server.elecNotify == nil {
		log.Fatal("Create Server elecNotify channel failed!")
	}

	server.RegisterHttpHandler()

	server.SetMetaDataChecked(false)
	return true
}

func (server *ManagerServer) RegisterHttpHandler() {
	mux := http.NewServeMux()
	group_keeper := &CachedGroupKeeper{
		server:      server,
		logger:      server.logger,
		etcd_client: server.etcdClient,
	}
	mux.Handle(CachedGroupKeeperPrefix, group_keeper)

	keep_alive := &HostsKeepAliveHandler{
		server:      server,
		logger:      server.logger,
		etcd_client: server.etcdClient,
	}
	mux.Handle(KeepAlivePrefix, keep_alive)

	slot_keeper := &SlotKeeper{
		server:      server,
		logger:      server.logger,
		etcd_client: server.etcdClient,
	}
	mux.Handle(SlotKeeperPrefix, slot_keeper)

	name_resolver := &NameResolver{
		WorkRoot:    KeepAliveWorkRootDef,
		server:      server,
		logger:      server.logger,
		etcd_client: server.etcdClient,
	}
	mux.Handle(NameResolverPrefix, name_resolver)

	html_handler := &HtmlHandler{
		server: server,
		logger: server.logger,
	}
	mux.Handle("/", html_handler)

	query_infos, _ := NewQueryInfos(server)
	mux.Handle(QueryInfosPrefix, query_infos)
	query_infos.Run()

	server.HttpServer = &http.Server{
		Addr:         server.config.LocalAddr,
		WriteTimeout: time.Second * 2,
		Handler:      mux,
	}
}

func (server *ManagerServer) Run() {
	server.logger.Println("McachedManager server running!")
	go func() {
		<-server.quit
		server.cancel()
	}()

	go func() {
		err := server.HttpServer.ListenAndServe()
		if err != nil {
			if err == http.ErrServerClosed {
				server.logger.Println("Server closed under request.")
			} else {
				server.logger.Println("Server closed unexpected error:", err)
			}
		}
		server.logger.Println("SlotGroupKeeper Server exited!")
	}()

	for {
		select {
		case <-server.comTicker.C:
			server.Tick()
		case <-server.electChan:
			server.logger.Println("<-server.electChan")
			server.UpdateLeaderContext()
			if server.CheckIsLeader() {
				server.MaybeMetaDataInitialize()
			}
		case <-server.elecErrorChan:
			server.logger.Println("<-server.elecErrorChan")
			server.UpdateLeaderContext()
		case items := <-server.elecNotify:
			server.logger.Println("<-server.elecNotify:", items)
			server.UpdateLeaderContext()
		case <-server.ctx.Done():
			return
		}
	}
}

func (server *ManagerServer) Tick() {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()

	lr, err := server.election.Leader(ctx)
	if err != nil {
		if err == concurrency.ErrElectionNoLeader {
			server.CampaignForLeader()
		} else {
			server.logger.Println("Get leader error:", err)
		}
	} else {
		if (lr != nil) && (lr.Kvs != nil) && len(lr.Kvs) > 0 && string(lr.Kvs[0].Value) != "" {
			server.leaderCtx.ContextMutex.Lock()
			server.leaderCtx.IsLeader = (string(lr.Kvs[0].Value) == server.config.LocalAddr)
			server.leaderCtx.LeaderKey = string(lr.Kvs[0].Key)
			server.leaderCtx.LeaderVal = string(lr.Kvs[0].Value)
			server.leaderCtx.LeaderCreateRev = lr.Kvs[0].CreateRevision
			server.leaderCtx.LeaderModRev = lr.Kvs[0].ModRevision
			server.leaderCtx.ContextMutex.Unlock()
		} else {
			server.logger.Println("Leader msg is empty!")
		}

		if string(lr.Kvs[0].Value) != server.config.LocalAddr {
			// watch leader by elecNotify
			server.CampaignForLeader()
		}
	}
}

func (server *ManagerServer) UpdateLeaderContext() {
	lr, err := server.election.Leader(server.ctx)
	if err != nil {
		server.logger.Println("Election get Leader error:", err)
		return
	}

	if (lr != nil) && (lr.Kvs != nil) && len(lr.Kvs) > 0 && string(lr.Kvs[0].Value) != "" {
		server.leaderCtx.ContextMutex.Lock()
		server.leaderCtx.IsLeader = (string(lr.Kvs[0].Value) == server.config.LocalAddr)
		server.leaderCtx.LeaderKey = string(lr.Kvs[0].Key)
		server.leaderCtx.LeaderVal = string(lr.Kvs[0].Value)
		server.leaderCtx.LeaderCreateRev = lr.Kvs[0].CreateRevision
		server.leaderCtx.LeaderModRev = lr.Kvs[0].ModRevision
		server.leaderCtx.ContextMutex.Unlock()

		server.logger.Println("Leader:", server.leaderCtx.LeaderVal)
	} else {
		server.logger.Println("Leader msg is empty!")
	}
}

func (server *ManagerServer) CampaignForLeader() {
	server.campaignMutex.Lock()
	defer server.campaignMutex.Unlock()
	if server.campaign {
		return
	}

	server.campaign = true

	go func() {
		ctx, cancel := context.WithCancel(context.TODO())
		defer cancel()
		if err := server.election.CampaignWaitNotify(ctx, server.config.LocalAddr, server.elecNotify); err != nil {
			server.logger.Println("Election Campaign error:", err)
			server.elecErrorChan <- err
		} else {
			server.electChan <- server.election
		}

		server.campaignMutex.Lock()
		defer server.campaignMutex.Unlock()
		server.campaign = false
	}()
}

func (server *ManagerServer) PeekLeaderContext() (lrc LeaderContextNoSafe) {
	server.leaderCtx.ContextMutex.Lock()
	defer server.leaderCtx.ContextMutex.Unlock()
	return server.leaderCtx.LeaderContextNoSafe
}

func (server *ManagerServer) CheckIsLeader() bool {
	server.leaderCtx.ContextMutex.Lock()
	defer server.leaderCtx.ContextMutex.Unlock()
	return server.leaderCtx.IsLeader
}

func (server *ManagerServer) MaybeMetaDataInitialize() {
	if server.GetMetaDataChecked() {
		return
	}

	slot_init, err := server.CheckSlotInfoInitialized()
	if err != nil {
		server.logger.Panicln("Check slot info failed!")
	}

	group_init, err := server.CheckMaxGroupIdInitialized()
	if err != nil {
		server.logger.Panicln("Check max group id info failed!")
	}

	if slot_init && group_init {
		server.SetMetaDataChecked(true)
		return
	}

	if !slot_init {
		server.MaybeInitializeSlots()
	}

	if !group_init {
		server.MaybeInitializeMaxGroupId()
	}
}

func (server *ManagerServer) GetMetaDataChecked() bool {
	server.metaDataChekedMutex.Lock()
	defer server.metaDataChekedMutex.Unlock()
	return server.metaDataCheked
}

func (server *ManagerServer) SetMetaDataChecked(val bool) {
	server.metaDataChekedMutex.Lock()
	defer server.metaDataChekedMutex.Unlock()
	server.metaDataCheked = val
}

func (server *ManagerServer) CheckSlotInfoInitialized() (bool, error) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()

	res, err := server.etcdClient.Get(ctx, SlotsPrefix, clientv3.WithCountOnly(), clientv3.WithPrefix())
	if err != nil {
		server.logger.Println("Etcd get slots info error:", err)
		return false, err
	}
	server.logger.Println("Etcd get slot info items:", res.Count)
	return res.Count == McachedSlotsEnd-McachedSlotsStart, nil
}

func (server *ManagerServer) MaybeInitializeSlots() (bool, error) {
	for i := McachedSlotsStart; i < McachedSlotsEnd; i++ {
		if succ, err := server.CreateSlotInTxn(i); succ != true {
			server.logger.Printf("Create slot[%d] in txn error[%s]!\n", i, err.Error())
			continue
		}
	}
	return true, nil
}

func (server *ManagerServer) CreateSlotInTxn(index int) (bool, error) {
	if index < McachedSlotsStart || index >= McachedSlotsEnd {
		server.logger.Panicln("Can't create illegal slot!")
	}

	slot := &SlotItem{
		Index:      uint64(index),
		Groupid:    0,
		IsAdjust:   false,
		DstGroupid: 0,
	}

	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()

	slot_key_prefix := BuildSlotIndexKey(slot.Index)
	slot_item_byte, err := json.Marshal(slot)
	if err != nil {
		server.logger.Println("Json Marshal slot item error:", err)
		return false, err
	}

	lr_nosafe := server.PeekLeaderContext()
	if !lr_nosafe.IsLeader {
		return false, errors.New("Not leader!")
	}

	txn := server.etcdClient.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(slot_key_prefix), "=", 0),
		clientv3.Compare(clientv3.CreateRevision(lr_nosafe.LeaderKey), "=", lr_nosafe.LeaderCreateRev),
		clientv3.Compare(clientv3.ModRevision(lr_nosafe.LeaderKey), "=", lr_nosafe.LeaderModRev))
	txn.Then(clientv3.OpPut(slot_key_prefix, string(slot_item_byte)))
	txn_res, err := txn.Commit()

	if err != nil {
		return false, err
	}

	if !txn_res.Succeeded {
		return false, errors.New("The txn of create slot failed!")
	}

	return txn_res.Succeeded, nil
}

func (server *ManagerServer) CheckMaxGroupIdInitialized() (bool, error) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()
	res, err := server.etcdClient.Get(ctx, GroupIdPrefix, clientv3.WithCountOnly(), clientv3.WithPrefix())
	if err != nil {
		server.logger.Println("Checkout GroupId error:", err)
		return false, err
	}
	server.logger.Println("Groupid count:", res.Count)
	return res.Count >= 1, nil
}

func (server *ManagerServer) MaybeInitializeMaxGroupId() (bool, error) {
	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()

	lr_nosafe := server.PeekLeaderContext()
	if !lr_nosafe.IsLeader {
		return false, errors.New("Not leader!")
	}

	max_group_id := fmt.Sprintf("%d", MaxGroupIdInitValue)

	txn := server.etcdClient.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(MaxGroupIdKey), "=", 0),
		clientv3.Compare(clientv3.CreateRevision(lr_nosafe.LeaderKey), "=", lr_nosafe.LeaderCreateRev),
		clientv3.Compare(clientv3.ModRevision(lr_nosafe.LeaderKey), "=", lr_nosafe.LeaderModRev))
	txn.Then(clientv3.OpPut(MaxGroupIdKey, max_group_id))
	txn_res, err := txn.Commit()

	if err != nil {
		return false, err
	}

	if !txn_res.Succeeded {
		return false, errors.New("The txn of create max group id failed!")
	}

	return txn_res.Succeeded, nil
}
