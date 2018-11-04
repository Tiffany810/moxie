package Mcached

import (
	"context"
	"encoding/json"
	"go.etcd.io/etcd/clientv3"
	"io/ioutil"
	"log"
	"net/http"
)

const (
	GroupKeepAlive				= 	0
)

type HostsKeepAliveHandler struct {
	server *ManagerServer
	// must be pointer to ManagerServer's logger
	logger      *log.Logger
	etcd_client *clientv3.Client
}

type KeepAliveRequest struct {
	Cmdtype				uint32 `json:"cmd_type"`
	Master				bool   `json:"is_master"`
	Addr				string `json:"hosts"`
	Groupid				uint64 `json:"source_id"`
	ServerName			string `json:"server_name"`
}

type KeepAliveResponse struct {
	Succeed bool   `json:"succeed"`
	Ecode   int32  `json:"ecode"`
	Msg     string `json:"msg"`
}

func (handler *HostsKeepAliveHandler) ServeHTTP(response http.ResponseWriter, request *http.Request) {
	lr_ctx_nosafe := handler.server.PeekLeaderContext()
	if lr_ctx_nosafe.IsLeader == false {
		if lr_ctx_nosafe.LeaderVal == "" {
			handler.logger.Println("Internal Server Error leader status!")
			response.WriteHeader(500)
		} else {
			handler.logger.Println("Request was redireted!")
			http.Redirect(response, request, GetRedirectRequestUrl(request, lr_ctx_nosafe.LeaderVal), 307)
		}
		return
	}

	keep_alive_request := &KeepAliveRequest{}
	body, _ := ioutil.ReadAll(request.Body)
	if err := json.Unmarshal(body, keep_alive_request); err != nil {
		response.WriteHeader(400)
		error_response := &KeepAliveResponse{
			Succeed: false,
			Ecode:   Error_RequestParseFailed,
			Msg:     err.Error(),
		}
		if error_res, err := json.Marshal(error_response); err == nil {
			response.Write(error_res)
		}
		return
	} else if keep_alive_request.Groupid == 0 || keep_alive_request.Addr == "" {
		response.WriteHeader(400)
		error_response := &KeepAliveResponse{
			Succeed: false,
			Ecode:   Error_InvaildArgs,
			Msg:     "",
		}
		if error_res, err := json.Marshal(error_response); err == nil {
			response.Write(error_res)
		}
	}

	handler.DoRequest(keep_alive_request, response, request)
}

func (handler *HostsKeepAliveHandler) DoRequest(keep_alive_request *KeepAliveRequest, response http.ResponseWriter, request *http.Request) {
	keep_alive_response := &KeepAliveResponse{
		Succeed: true,
		Ecode:   Error_NoError,
		Msg:     "",
	}

	switch keep_alive_request.Cmdtype {
	case GroupKeepAlive:
		handler.DoHostKeepAlive(keep_alive_request, keep_alive_response)
	default:
		keep_alive_response.Msg = "Cmd not found!"
		keep_alive_response.Succeed = false
		keep_alive_response.Ecode = Error_CmdNotFound
	}

	if keep_alive_tobytes, err := json.Marshal(keep_alive_response); err == nil {
		response.Write(keep_alive_tobytes)
	} else {
		response.WriteHeader(500)
	}
}

func (handler *HostsKeepAliveHandler) DoHostKeepAlive(keep_alive_request *KeepAliveRequest, keep_alive_response *KeepAliveResponse) {
	if keep_alive_request == nil {
		handler.logger.Panicln("Request is nil!")
	}
	var keep_alive_key_prefix string
	if keep_alive_request.Master {
		keep_alive_key_prefix = BuildKeepAliveMasterKey(keep_alive_request.ServerName, keep_alive_request.Groupid, keep_alive_request.Addr)
	} else {
		keep_alive_key_prefix = BuildKeepAliveSlaveKey(keep_alive_request.ServerName, keep_alive_request.Groupid, keep_alive_request.Addr)
	}
	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()

	resp, err := handler.etcd_client.Grant(ctx, int64(KeepAliveLease))
	if err != nil {
		handler.logger.Println("Grant lease failed:", err)
		keep_alive_response.Succeed = false
		keep_alive_response.Ecode = Error_ServerInternelError
		keep_alive_response.Msg = err.Error()
		return
	}
	log.Println("keep_alive_key_prefix:",keep_alive_key_prefix)
	_, err = handler.etcd_client.Put(ctx, keep_alive_key_prefix, keep_alive_request.Addr, clientv3.WithLease(resp.ID))
	if err != nil {
		handler.logger.Println("Upadte key lease failed:", err)
		keep_alive_response.Succeed = false
		keep_alive_response.Ecode = Error_ServerInternelError
		keep_alive_response.Msg = err.Error()
		return
	}
	keep_alive_response.Succeed = true
	keep_alive_response.Ecode = Error_NoError
	keep_alive_response.Msg = Msg_OK
}
