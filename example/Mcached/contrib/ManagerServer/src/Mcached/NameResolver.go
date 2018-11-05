package Mcached

import (
	"fmt"
	"encoding/json"
	"go.etcd.io/etcd/clientv3"
	"io/ioutil"
	"log"
	"context"
	"net/http"
	"strings"
)

type NameResolver struct {
	WorkRoot				string

	server					*ManagerServer
	etcd_client				*clientv3.Client
	logger					*log.Logger
}

type NameResolverRequest struct {
	ServerName 				string `json:"server_name"`
}

type EndPoints struct {
	Master					string		`json:"master"`
	Slaves					[]string	`json:"slaves"`
}

type NameResolverResponse struct {
	Succeed 				bool				`json:"succeed"`
	Ecode   				int32				`json:"ecode"`
	Msg     				string				`json:"msg"`
	Hosts					map[int64]EndPoints	`json:"hosts"`
}

func (resolver *NameResolver) ServeHTTP(response http.ResponseWriter, request *http.Request) {
	name_resolver_request := &NameResolverRequest{}
	body, _ := ioutil.ReadAll(request.Body)
	if err := json.Unmarshal(body, name_resolver_request); err != nil {
		response.WriteHeader(500)
		resolver.logger.Println("json.Unmarshal Request failed:", err)
		name_resolver_response := &NameResolverResponse{
			Succeed: false,
			Ecode:   Error_RequestParseFailed,
			Msg:     err.Error(),
		}
		if error_res, err := json.Marshal(name_resolver_response); err == nil {
			response.Write(error_res)
		}
		return
	}

	resolver.DoNameResolver(name_resolver_request, response, request)
}

func (resolver *NameResolver) DoNameResolver(name_resolver_request *NameResolverRequest, response http.ResponseWriter, request *http.Request) {
	resolver_response := &NameResolverResponse{
		Succeed	: false,
		Ecode	: Error_NoError,
		Msg		: "",
		Hosts	: make(map[int64]EndPoints),
	}
	log.Println("SerevrName:", name_resolver_request.ServerName)
	if name_resolver_request.ServerName == "" {
		resolver_response.Succeed = false
		resolver_response.Msg = "Server name is empty!"
		resolver_response.Ecode = Error_InvaildServerName
	} else {
		resolver.ParseServerList(name_resolver_request, resolver_response)
	}
	if error_res, err := json.Marshal(resolver_response); err == nil {
		response.Write(error_res)
	}
}

func (resolver *NameResolver) ParseServerList (name_resolver_request *NameResolverRequest, name_resolver_response *NameResolverResponse) {
	server_list_path := ""
	if resolver.WorkRoot != "" {
		server_list_path = resolver.WorkRoot + name_resolver_request.ServerName + "/"
	} else {
		server_list_path = KeepAliveWorkRootDef + name_resolver_request.ServerName + "/"
	}

	log.Println("server_list_path:", server_list_path)
	ctx, cancel := context.WithTimeout(context.Background(), EtcdTxnRequestTimeout)
	defer cancel()
	get_res , err := resolver.etcd_client.Get(ctx, server_list_path, clientv3.WithPrefix())
	if err != nil {
		name_resolver_response.Ecode = Error_ServerInternelError
		name_resolver_response.Succeed = false
		name_resolver_response.Msg = err.Error()
		return
	}

	log.Println("len_of_kvs:", len(get_res.Kvs))

	if get_res != nil && get_res.Kvs != nil && len(get_res.Kvs) > 0 {
		for i := 0; i < len(get_res.Kvs); i++ {
			ret_item := strings.Split(string(get_res.Kvs[i].Key), "/")
			log.Printf("item_ret[%v]", ret_item)
			item_len := len(ret_item)
			if item_len < 3 || ret_item[item_len - 1] != string(get_res.Kvs[i].Value) {
				resolver.logger.Println("Resolver meta data format error!")
				continue
			}
			var id int64 = 0
			fmt.Sscanf(ret_item[item_len - 3], "%d", &id)
			
			host := EndPoints {
				Master : "",
				Slaves : make([]string, 0),
			}
			
			if ret_item[item_len - 2] == "master" {
				host.Master = ret_item[item_len - 1]
			} else {
				host.Slaves = append(host.Slaves, ret_item[item_len - 1])
			}

			if ret, ok := name_resolver_response.Hosts[id]; !ok {
				name_resolver_response.Hosts[id] = host
			} else {
				if ret.Master != "" && host.Master == "" {
					host.Master = ret.Master
				}
				host.Slaves = append(host.Slaves, ret.Slaves...)
				name_resolver_response.Hosts[id] = host
			}
		}
	}
	name_resolver_response.Succeed = true
	name_resolver_response.Msg = Msg_OK
	name_resolver_response.Ecode = Error_NoError
}