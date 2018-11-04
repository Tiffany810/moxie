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
	if request.URL.Path == "/" {
		http.ServeFile(response, request, IndexHtmlFile)
	} else {
		file_path := "." + request.URL.Path
		http.ServeFile(response, request, file_path)
	}
}