package main

import (
	"fmt"
	"os"
	"time"
	"log"
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

func TestMotifyTxn(EtcdAddr []string) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	EtcdClientv3, err := clientv3.New(clientv3.Config{
		Endpoints:  EtcdAddr,
		DialTimeout: time.Second * 1,
	})
	if err != nil {
		fmt.Println("Create etcdclientv3 error:", err)
	}

	key_prefix := "MotifyKey"
	group_byte := "Motifyvalue"
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

}

func ExampleLease_grant(endpoints []string) {
	cli, err := clientv3.New(clientv3.Config{
		Endpoints:   endpoints,
		DialTimeout: time.Second * 2,
	})
	if err != nil {
		log.Fatal(err)
	}
	defer cli.Close()

	// minimum lease TTL is 5-second
	resp, err := cli.Grant(context.TODO(), 5)
	if err != nil {
		log.Fatal(err)
	}

	// after 5 seconds, the key 'foo' will be removed
	_, err = cli.Put(context.TODO(), "foo", "bar", clientv3.WithLease(resp.ID))
	if err != nil {
		log.Fatal(err)
	}

	time.Sleep(time.Second * 2)

	getres , err := cli.Get(context.TODO(), "foo")
	if err != nil {
		log.Fatal(err)
	}
	log.Println(getres)

	time.Sleep(time.Second * 5)

	getres , err = cli.Get(context.TODO(), "foo")
	if err != nil {
		log.Fatal(err)
	}
	log.Println(getres)
}

func ExampleLease_revoke(endpoints []string) {
	cli, err := clientv3.New(clientv3.Config{
		Endpoints:   endpoints,
		DialTimeout: time.Second * 2,
	})
	if err != nil {
		log.Fatal(err)
	}
	defer cli.Close()

	resp, err := cli.Grant(context.TODO(), 5)
	if err != nil {
		log.Fatal(err)
	}

	_, err = cli.Put(context.TODO(), "fooRevoke", "bar", clientv3.WithLease(resp.ID))
	if err != nil {
		log.Fatal(err)
	}

	gresp, err := cli.Get(context.TODO(), "fooRevoke")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("number of keys:", len(gresp.Kvs))

	// revoking lease expires the key attached to its lease ID
	_, err = cli.Revoke(context.TODO(), resp.ID)
	if err != nil {
		log.Fatal(err)
	}

	gresp, err = cli.Get(context.TODO(), "fooRevoke")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("After revoke number of keys:", len(gresp.Kvs))
	// Output: number of keys: 0
}

func ExampleLease_keepAlive(endpoints []string) {
	cli, err := clientv3.New(clientv3.Config{
		Endpoints:   endpoints,
		DialTimeout: time.Second * 2,
	})
	if err != nil {
		log.Fatal(err)
	}

	resp, err := cli.Grant(context.TODO(), 5)
	if err != nil {
		log.Fatal(err)
	}

	_, err = cli.Put(context.TODO(), "fookeepAlive", "bar", clientv3.WithLease(resp.ID))
	if err != nil {
		log.Fatal(err)
	}

	gresp, err := cli.Get(context.TODO(), "fookeepAlive")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("keys:", len(gresp.Kvs))

	// the key 'foo' will be kept forever
	ch, kaerr := cli.KeepAlive(context.TODO(), resp.ID)
	if kaerr != nil {
		log.Fatal(kaerr)
	}

	ka := <-ch
	fmt.Println("ttl:", ka.TTL)
	// Output: ttl: 5

	time.Sleep(time.Second * 10)
	gresp, err = cli.Get(context.TODO(), "fookeepAlive")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("afetr sleep keys:", gresp.Kvs)

	cli.Close()

	time.Sleep(time.Second * 1)

	cli1, err := clientv3.New(clientv3.Config{
		Endpoints:   endpoints,
		DialTimeout: time.Second * 2,
	})
	if err != nil {
		log.Fatal(err)
	}

	gresp, err = cli1.Get(context.TODO(), "fookeepAlive")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("afetr close 1s client keys:", gresp.Kvs)

	time.Sleep(2 * time.Second)
	gresp, err = cli1.Get(context.TODO(), "fookeepAlive")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("afetr close 3s client keys:", gresp.Kvs)

	time.Sleep(2 * time.Second)
	gresp, err = cli1.Get(context.TODO(), "fookeepAlive")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("afetr close 5s client keys:", gresp.Kvs)

	time.Sleep(2 * time.Second)
	gresp, err = cli1.Get(context.TODO(), "fookeepAlive")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("afetr close 7s client keys:", gresp.Kvs)
}

func ExampleLease_keepAliveOnce(endpoints []string) {
	cli, err := clientv3.New(clientv3.Config{
		Endpoints:   endpoints,
		DialTimeout: time.Second * 2,
	})
	if err != nil {
		log.Fatal(err)
	}
	defer cli.Close()

	resp, err := cli.Grant(context.TODO(), 5)
	if err != nil {
		log.Fatal(err)
	}

	_, err = cli.Put(context.TODO(), "fookeepAliveOnce", "bar", clientv3.WithLease(resp.ID))
	if err != nil {
		log.Fatal(err)
	}

	// to renew the lease only once
	ka, kaerr := cli.KeepAliveOnce(context.TODO(), resp.ID)
	if kaerr != nil {
		log.Fatal(kaerr)
	}

	fmt.Println("ttl:", ka.TTL)
	// Output: ttl: 5

	cli1, err := clientv3.New(clientv3.Config{
		Endpoints:   endpoints,
		DialTimeout: time.Second * 2,
	})
	if err != nil {
		log.Fatal(err)
	}

	gresp, err := cli1.Get(context.TODO(), "fookeepAliveOnce")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("afetr 1s client keys:", gresp.Kvs)

	time.Sleep(2 * time.Second)
	gresp, err = cli1.Get(context.TODO(), "fookeepAliveOnce")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("afetr 3s client keys:", gresp.Kvs)

	time.Sleep(2 * time.Second)
	gresp, err = cli1.Get(context.TODO(), "fookeepAliveOnce")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("afetr 5s client keys:", gresp.Kvs)

	time.Sleep(2 * time.Second)
	gresp, err = cli1.Get(context.TODO(), "fookeepAliveOnce")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("afetr 7s client keys:", gresp.Kvs)
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

	ExampleLease_keepAliveOnce(EtcdAddr)
}
