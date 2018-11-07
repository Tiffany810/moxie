package Mcached

import (
	"log"
	"net/http"
)

type HtmlHandler struct {
	server				*ManagerServer
	logger				*log.Logger
}

func (handler *HtmlHandler) ServeHTTP(response http.ResponseWriter, request *http.Request) {
	if origin := request.Header.Get("Origin"); origin != "" {
		response.Header().Set("Access-Control-Allow-Origin", "*")
		response.Header().Set("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE")
		response.Header().Set("Access-Control-Allow-Headers", "Action, Module")
	}
		
	if request.Method == "OPTIONS" {
		return
	}
	
	if request.URL.Path == "/" {
		http.ServeFile(response, request, IndexHtmlFile)
	} else {
		file_path := "." + request.URL.Path
		log.Println("request path:", file_path)
		http.ServeFile(response, request, file_path)
	}
}