package Observe

import (
	"io/ioutil"
	"log"
	"encoding/json"
	"net/http"
)

type ResolverService struct {
	resolver 			*ResolverWorker
	logger				*log.Logger
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

func (resolver *ResolverService) ServeHTTP(response http.ResponseWriter, request *http.Request) {
	if origin := request.Header.Get("Origin"); origin != "" {
		response.Header().Set("Access-Control-Allow-Origin", "*")
		response.Header().Set("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE")
		response.Header().Set("Access-Control-Allow-Headers", "Action, Module")
	}
		
	if request.Method == "OPTIONS" {
		return
	}
	
	name_resolver_request := &NameResolverRequest{}
	body, _ := ioutil.ReadAll(request.Body)
	log.Println("Body:", string(body))
	if err := json.Unmarshal(body, name_resolver_request); err != nil {
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

func (resolver *ResolverService) DoNameResolver(name_resolver_request *NameResolverRequest, response http.ResponseWriter, request *http.Request) {
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
		resolver.PeekServerList(name_resolver_request, resolver_response)
	}
	if error_res, err := json.Marshal(resolver_response); err == nil {
		response.Write(error_res)
	}
}

func (resolver *ResolverService) PeekServerList (name_resolver_request *NameResolverRequest, name_resolver_response *NameResolverResponse) {
	response, _ := resolver.resolver.PeekNameResolverResponse(name_resolver_request.ServerName)
	*name_resolver_response = response
}