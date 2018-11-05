package main;

import (
	"flag"
	"strings"
	"Observe"
)

func main() {
	serever_host := flag.String("service_host", "0.0.0.0:8696", "Listen on for resolver request!")
	manager_server_list := flag.String("manager_list", "http://127.0.0.1:8898", "Manager server host!")
	log_file := flag.String("log_file", "./log/observe.log", "The log object file!")
	server_name_list := flag.String("server_name_list", "DataSerevr,Observe", "The default server list!")
	flag.Parse()
	
	var conf Observe.ObserveConf
	conf.LocalHost = *serever_host
	conf.LogFile = *log_file
	conf.ServerNameList = strings.Split(*server_name_list, ",")
	conf.UpstreamList = strings.Split(*manager_server_list, ",")

	var observe Observe.Observe
	observe.Init(&conf)
	observe.Run()
}