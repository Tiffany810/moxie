package main

import (
	"flag"
)

func main() {
	etcd_client := flag.String("etcd-client", "localhost:32379", "The client listen addr of etcd!")
	flag.Parse()

	etcd_client_addr := make([]string, 0)
	etcd_client_addr =append(etcd_client_addr, *etcd_client)
	
	//TestTxnReversion(etcd_client_addr)
	ExampleElection_CampaignWithNotify(etcd_client_addr)
	// ExampleElection_Campaign(etcd_client_addr)
}
