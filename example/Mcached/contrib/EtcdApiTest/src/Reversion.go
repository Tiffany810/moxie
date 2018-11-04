package main

import (
	"fmt"
	"context"
	"strconv"
	"go.etcd.io/etcd/clientv3"
)

func TestCreateReversion(endpoints []string) {
	cli, err := clientv3.New(clientv3.Config{Endpoints: endpoints})
	if err != nil {
		fmt.Println(err)
		return
	}
	defer cli.Close()

	test_key_prefix := "/test/keyprefix/"

	ctx, cancel := context.WithCancel(context.TODO())
	defer cancel()

	for i := 1; i < 5; i++ {
		key := test_key_prefix + strconv.Itoa(i)
		pres, err := cli.Put(ctx, key, key)
		if err != nil {
			fmt.Println(err)
			return
		}
	
		fmt.Printf("reversion:%d ", pres.Header.Revision)
		if pres.PrevKv != nil {
			fmt.Printf(" PrevKey:%s PrevValue:%s\n", pres.PrevKv.Key, pres.PrevKv.Value)
		}
	}
	gres, err := cli.Get(ctx, test_key_prefix, clientv3.WithPrefix())
	fmt.Printf("key_prefix_reversion:%d\n", gres.Header.Revision)
	gres, err = cli.Get(ctx, test_key_prefix, clientv3.WithPrefix())
	fmt.Printf("key_prefix_reversion:%d\n", gres.Header.Revision)
}

func TestTxnReversion(endpoints []string) {
	cli, err := clientv3.New(clientv3.Config{Endpoints: endpoints})
	if err != nil {
		fmt.Println(err)
		return
	}
	defer cli.Close()

	test_key_prefix := "/test/txn/keyprefix/"

	ctx, cancel := context.WithCancel(context.TODO())
	defer cancel()

	for i := 1; i < 10; i++ {
		key := test_key_prefix + strconv.Itoa(i)
		txn := cli.Txn(ctx)
		txn.If(clientv3.Compare(clientv3.CreateRevision(key), "=", 0))
		txn.Then(clientv3.OpPut(key, key))
		txn.Else(clientv3.OpGet(key))
		tres, err := txn.Commit()
		if err != nil {
			fmt.Println(err)
			return
		}
		if !tres.Succeeded {
			fmt.Printf("[Succeed]Reversion:%d,", tres.Header.Revision)
			if tres.Responses == nil {
				continue
			}
			for j := 0; j < len(tres.Responses); j++ {
				fmt.Printf("[%d]", i)
				if tres.Responses[j].GetResponseRange() != nil && tres.Responses[j].GetResponseRange().Header != nil  {
					fmt.Printf("Reversion:%d,", tres.Responses[j].GetResponseRange().Header.Revision)
				}
				if tres.Responses[j].GetResponseRange() == nil && tres.Responses[j].GetResponseRange().Kvs == nil {
					continue
				}
				for k := 0; k < len(tres.Responses[j].GetResponseRange().Kvs); k++ {
					fmt.Printf("[%s->%s,cv:%d,mv:%d]", tres.Responses[j].GetResponseRange().Kvs[k].Key, tres.Responses[j].GetResponseRange().Kvs[k].Value, tres.Responses[j].GetResponseRange().Kvs[k].CreateRevision, tres.Responses[j].GetResponseRange().Kvs[k].ModRevision)
				}
				fmt.Println("")
			}
		} else {
			fmt.Printf("[Succeed]Reversion:%d,", tres.Header.Revision)
			if tres.Responses == nil {
				continue
			}
			for j := 0; j < len(tres.Responses); j++ {
				fmt.Printf("[%d]", j)
				if tres.Responses[j].GetResponseRange() != nil && tres.Responses[j].GetResponseRange().Header != nil {
					fmt.Printf("Reversion:%d,", tres.Responses[j].GetResponseRange().Header.Revision)
				}
				if tres.Responses[j].GetResponseRange() == nil || tres.Responses[j].GetResponseRange().Kvs == nil {
					continue
				}
				for k := 0; k < len(tres.Responses[j].GetResponseRange().Kvs); k++ {
					fmt.Printf("[%s->%s,cv:%d,mv:%d]", tres.Responses[j].GetResponseRange().Kvs[k].Key, tres.Responses[j].GetResponseRange().Kvs[k].Value, tres.Responses[j].GetResponseRange().Kvs[k].CreateRevision, tres.Responses[j].GetResponseRange().Kvs[k].ModRevision)
				}
				fmt.Println("")
			}
		}
	}
	gres, err := cli.Get(ctx, test_key_prefix, clientv3.WithPrefix())
	fmt.Printf("key_prefix_reversion:%d\n", gres.Header.Revision)

	getOpts := append(clientv3.WithLastCreate(), clientv3.WithMaxCreateRev(19))
	resp, err := cli.Get(ctx, test_key_prefix, getOpts...)
	if err != nil {
		fmt.Println(err)
		return
	}
	
	for i := 0; i < len(resp.Kvs); i++ {
		fmt.Printf("Reversion:%d,CreateReversion:%d,ModReversion:%d,kv:%s->%s\n", resp.Header.Revision, resp.Kvs[0].CreateRevision, resp.Kvs[0].ModRevision, resp.Kvs[0].Key, resp.Kvs[0].Value)
	}

	resp, err = cli.Get(ctx, test_key_prefix, clientv3.WithFirstCreate()...)
	for i := 0; i < len(resp.Kvs); i++ {
		fmt.Printf("Reversion:%d,CreateReversion:%d,ModReversion:%d,kv:%s->%s\n", resp.Header.Revision, resp.Kvs[0].CreateRevision, resp.Kvs[0].ModRevision, resp.Kvs[0].Key, resp.Kvs[0].Value)
	}
}

