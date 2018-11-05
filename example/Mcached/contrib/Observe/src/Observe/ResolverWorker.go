package Observe

import (
	"encoding/json"
	"sync"
	"time"
	"io/ioutil"
	"context"
	"net/http"
	"strings"
	"log"
)

type ResolverWorker struct {
	ServerListMapNutex			sync.Mutex
	ServerListMap				map[string]*NameResolverResponse
	Ctx							context.Context
	UpstreamList				[]string
	Works						map[string]*Worker
	logger						*log.Logger
}

func (resolver *ResolverWorker) RegisterServerName(ServerName string) {
	work := &Worker {
		ServerName : ServerName,
		Ctx	:resolver.Ctx,
		logger : resolver.logger,
		UpstreamList : resolver.UpstreamList,
		resolver : resolver,
		DoResolverTicker : time.NewTicker(time.Second),
	}
	work.InitResolverRequest()
	resolver.Works[ServerName] = work
}

func (resolver *ResolverWorker) StartResolver() {
	for _, v := range resolver.Works {
		go v.DoWork()
	}
} 

func (resolver *ResolverWorker) PeekNameResolverResponse(server_name string) (NameResolverResponse, bool) {
	resolver.ServerListMapNutex.Lock()
	defer resolver.ServerListMapNutex.Unlock()
	if ret, ok := resolver.ServerListMap[server_name]; ok {
		return *ret, ok
	}
	return NameResolverResponse{
		Succeed : false,
		Ecode : Error_NoNameServerMsg,
		Msg : "ServerName not found!",
	}, false
} 

func (resolver *ResolverWorker) UpdateNameResolverResponse(server_name string, result *NameResolverResponse) {
	resolver.ServerListMapNutex.Lock()
	defer resolver.ServerListMapNutex.Unlock()
	resolver.ServerListMap[server_name] = result
} 

type Worker struct {
	ServerName				string
	DoResolverTicker		*time.Ticker
	Ctx						context.Context
	client 					http.Client
	request_string_cache	string
	
	logger					*log.Logger
	UpstreamList			[]string
	resolver				*ResolverWorker
}

func (worker *Worker) InitResolverRequest() (bool, error) {
	name_request := &NameResolverRequest {
		ServerName : worker.ServerName,
	}
	if ret_bytes, err := json.Marshal(name_request); err != nil {
		return false, err
	} else {
		worker.request_string_cache = string(ret_bytes)
	}

	return true, nil
}

func (worker *Worker) BuildPostHttpRequest (url string) (*http.Request, error) {
	req, err := http.NewRequest("POST", url, strings.NewReader(worker.request_string_cache))
    if err != nil {
        return nil, err
	}
	req.Header.Set("Content-Type", "application/json")
	return req, err
}

func (worker *Worker) DoResolver () {
	for i := 0; i < len(worker.UpstreamList); i++ {
		req, err := worker.BuildPostHttpRequest(worker.UpstreamList[i] + NameResolverPrefix)
		if err != nil {
			worker.logger.Panicln("BuildPostHttpRequest failed with error:", err)
		}

		resp, err := worker.client.Do(req)
		if err != nil {
			worker.logger.Println("Client do request error:", err)
			continue
		}
		body, err := ioutil.ReadAll(resp.Body)
		if err != nil {
			worker.logger.Println("ReadAll response error:", err)
			continue
		}

		response := &NameResolverResponse {}
		if json.Unmarshal([]byte(body), response) != nil {
			worker.logger.Println("Unmarshal body error:", err)
			continue
		}

		if response.Succeed == false {
			worker.logger.Printf("Request server name failed with response[%v]", response)
			break
		}

		worker.resolver.UpdateNameResolverResponse(worker.ServerName, response)
	}
}

func (worker *Worker) DoWork () {
	for {
		select {
		case <-worker.Ctx.Done():
			return
		case <-worker.DoResolverTicker.C:
			go worker.DoResolver()
		}
	}
}