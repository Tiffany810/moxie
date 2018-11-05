package Observe

import (
	"encoding/json"
	"log"
	"os"
	"io/ioutil"
	"time"
	"os/signal"
	"net/http"
	"context"
	"strings"
)

const (
	WorkPath			= "/observe/"
	ThisServerName		= "Observe"
)

type ObserveConf struct {
	UpstreamList	[]string
	LocalHost		string
	LogFile			string
	ServerNameList	[]string
}

type Observe struct {
	config 				ObserveConf
	quit   				chan os.Signal
	logger 				*log.Logger

	keepAliveBodyCache	string
	httpClient			http.Client
	
	comTicker			*time.Ticker

	cancel    			context.CancelFunc
	ctx 				context.Context
	HttpServer 			*http.Server
	resolver			*ResolverWorker
}

func (observe *Observe) Init(conf *ObserveConf) {
	if conf == nil {
		log.Panicln("ObserveConf is nil!")
	}
	observe.config = *conf
	observe.quit = make(chan os.Signal, 1)
	observe.quit = make(chan os.Signal)
	if observe.quit == nil {
		log.Fatal("Create Server quit channel failed!")
	}
	signal.Notify(observe.quit, os.Interrupt)
	
	kq := &KeepAliveRequest {
		Cmdtype : 0,
		Master : false,
		Addr : observe.config.LocalHost,
		Groupid : 0,
		ServerName : ThisServerName,
	}
	if kq_byte, err := json.Marshal(kq); err == nil {
		observe.keepAliveBodyCache = string(kq_byte)
	} else {
		log.Fatal("Marshal KeepAliveRequest error:", err.Error())
	}

	observe.comTicker = time.NewTicker(3 * time.Second)

	observe.ctx, observe.cancel = context.WithCancel(context.Background())

	if observe.config.LogFile != "" {
		logFile, err := os.Create(observe.config.LogFile)
		if err != nil {
			log.Fatal("Create logfile failed error:", err)
		}
		observe.logger = log.New(logFile, "", log.Llongfile)
	}

	observe.resolver = &ResolverWorker {
		ServerListMap : make(map[string]*NameResolverResponse),
		Ctx : observe.ctx,
		UpstreamList : observe.config.UpstreamList,
		Works : make(map[string]*Worker),
		logger : observe.logger,
	}

	for i := 0; i < len(observe.config.ServerNameList); i++ {
		observe.resolver.RegisterServerName(observe.config.ServerNameList[i])
	}

	observe.RegisterHttpHandler()
}	

func (observe *Observe) RegisterHttpHandler() {
	mux := http.NewServeMux()
	service := &ResolverService {
		resolver : observe.resolver,
		logger : observe.logger,
	}

	mux.Handle(WorkPath, service)

	observe.HttpServer = &http.Server{
		Addr:         observe.config.LocalHost,
		WriteTimeout: time.Second * 2,
		Handler:      mux,
	}
}

func (observe *Observe) Run() {
	observe.resolver.StartResolver()

	go func() {
		err := observe.HttpServer.ListenAndServe()
		if err != nil {
			if err == http.ErrServerClosed {
				observe.logger.Println("Server closed under request.")
			} else {
				observe.logger.Println("Server closed unexpected error:", err)
			}
		}
		observe.logger.Println("SlotGroupKeeper Server exited!")
	}()

	for {
		select {
		case <-observe.comTicker.C:
			go observe.KeepObserveAlive()
		case <-observe.quit:
			observe.cancel()
		case <-observe.ctx.Done():
			return
		}
	}
}

func (observe *Observe) BuildPostHttpRequest (url string) (*http.Request, error) {
	req, err := http.NewRequest("POST", url, strings.NewReader(observe.keepAliveBodyCache))
    if err != nil {
        return nil, err
	}
	req.Header.Set("Content-Type", "application/json")
	return req, err
}

func (observe *Observe) KeepObserveAlive() {
	for i := 0; i < len(observe.config.UpstreamList); i++ {
		req, err := observe.BuildPostHttpRequest(observe.config.UpstreamList[i] + KeepAlivePrefix)
		if err != nil {
			observe.logger.Panicln("BuildPostHttpRequest failed with error:", err)
		}

		resp, err := observe.httpClient.Do(req)
		if err != nil {
			observe.logger.Println("Client do request error:", err)
			continue
		}
		body, err := ioutil.ReadAll(resp.Body)
		if err != nil {
			observe.logger.Println("ReadAll response error:", err)
			continue
		}

		karesp := &KeepAliveResponse {}
		if json.Unmarshal(body, karesp) != nil {
			continue
		}

		if karesp.Succeed != true {
			observe.logger.Println("Observe keep alive failed with msg:", karesp.Msg)
		} else {
			break;
		}
	}
}
