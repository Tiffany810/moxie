package main

import (
	"fmt"
	"os"
	"time"
	"context"
	"strings"
	"go.etcd.io/etcd/clientv3"
)

func TestTxn(EtcdAddr []string) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	EtcdClientv3, err := clientv3.New(clientv3.Config{
		Endpoints:  EtcdAddr,
		DialTimeout: time.Second * 1,
	})
	if err != nil {
		fmt.Println("Create etcdclientv3 error:", err)
	}

	key_prefix := "mkey1"
	group_byte := "mvalue1"
	txn := EtcdClientv3.Txn(ctx)
	txn.If(clientv3.Compare(clientv3.CreateRevision(key_prefix), "=", 0))
	txn.Then(clientv3.OpPut(key_prefix, string(group_byte)))
	tres, err := txn.Commit()
	if err != nil {
		fmt.Println(err)
	}
	if tres.Succeeded {
		fmt.Println("Txn succeed!")
		fmt.Println(tres.Header)
		fmt.Println(tres.OpResponse())
	} else {
		fmt.Println("Txn unsucceed!")
		fmt.Println(tres.Header)
		fmt.Println(tres.OpResponse())
	}

	txn1 := EtcdClientv3.Txn(ctx)
	txn1.If(clientv3.Compare(clientv3.CreateRevision(key_prefix), "=", 0))
	txn1.Then(clientv3.OpPut(key_prefix, string(group_byte)))
	tres, err = txn1.Commit()
	if err != nil {
		fmt.Println(err)
	}
	if tres.Succeeded {
		if dres, err := EtcdClientv3.Delete(ctx, key_prefix); err == nil {
			for i := 0; i < len(dres.PrevKvs); i++ {
				fmt.Println(dres.PrevKvs[i].Key, "->", dres.PrevKvs[i].Value)
			}
			fmt.Println("Delete succeed!")
		} else {
			fmt.Println("Delete error!", dres.OpResponse())
		}
		fmt.Println(tres.Header)
		fmt.Println(tres.OpResponse())
	} else {
		fmt.Println("Txn unsucceed!")
		fmt.Println(tres.Header)
		fmt.Println(tres.OpResponse())
	}

	if gres, err := EtcdClientv3.Get(ctx, key_prefix); err == nil {
		fmt.Println("get succeed!")
		fmt.Println(gres.Header)
		fmt.Println(gres.OpResponse())
		fmt.Println(gres.Kvs)
	} else {
		fmt.Println("get unsucceed!")
		fmt.Println(gres.Header)
		fmt.Println(gres.OpResponse())
		fmt.Println(gres.Kvs)
	}

	if gres, err := EtcdClientv3.Delete(ctx, key_prefix); err == nil {
		fmt.Println("put succeed!")
		fmt.Println(gres.Header)
		fmt.Println(gres.OpResponse())
		fmt.Println(gres.PrevKvs)
	} else {
		fmt.Println("put unsucceed!")
		fmt.Println(gres.Header)
		fmt.Println(gres.OpResponse())
		fmt.Println(gres.PrevKvs)
	}

	if gres, err := EtcdClientv3.Get(ctx, key_prefix); err == nil {
		fmt.Println("get succeed!")
		fmt.Println(gres.Header)
		fmt.Println(gres.OpResponse())
		fmt.Println(gres.Kvs)
	} else {
		fmt.Println("get unsucceed!")
		fmt.Println(gres.Header)
		fmt.Println(gres.OpResponse())
		fmt.Println(gres.Kvs)
	}
}

func main() {
	// --advertise-client-urls 'http://localhost:32379'
	EtcdAddr := make([]string, 0, 1)
	for i := 0; i < len(os.Args); i++ {
		if os.Args[i] == "--advertise-client-urls" {
			if i+1 < len(os.Args) {
				url := os.Args[i+1]
				urls := strings.Split(url, ",")
				for j := 0; j < len(urls); j++ { // http://
					EtcdAddr = append(EtcdAddr, urls[j][7:])
				}
			} else {
				fmt.Println("The --advertise-client-urls value format is incorrect!")
			}
		}
	}
	fmt.Println("EtcdAddr", EtcdAddr)
	TestTxn(EtcdAddr)
}
