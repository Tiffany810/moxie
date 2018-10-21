package main

import (
	"flag"
	"fmt"
	"github.com/gomodule/redigo/redis"
	"strconv"
	"sync"
	"time"
)

func GetStringInSize(size uint, char string) (string, error) {
	ret := char
	for uint(len(ret)) < size {
		ret += ret
	}
	return ret[:size], nil
}

type Task struct {
	client redis.Conn
	datalen uint
	keylen uint
	addr string
	reqs int
}

func main() {
	routinenum := flag.Int("rn", 4, "Go routine Num!")
	addr := flag.String("ip", "127.0.0.1:6379", "The address of redis server!")
	keylen := flag.Uint("keylen", 30, "The default length of key!")
	datalen := flag.Uint("datalen", 128, "The default length of data!")
	reqs := flag.Int("reqs", 10000, "The times of this test!")
	flag.Parse()

	if *routinenum <= 0 || *reqs <= 0 {
		fmt.Println("Args Error!")
		return
	}

	fmt.Printf("rn = %d, ip = %s, keylen = %d, datalen = %d, reqs = %d\n", *routinenum, *addr, *keylen, *datalen, *reqs)
	var total_reqs uint64 = 0
	var wg sync.WaitGroup

	ctx := make([]Task, 0, *routinenum)
	for c := 0; c < *routinenum; c++ {
		redisclient, err := redis.Dial("tcp", *addr)
		if err != nil {
			fmt.Println("Build memcachedclient failed! error:", err)
			return
		}
		ctx = append(ctx, Task{
			client : redisclient,
			datalen : *datalen,
			keylen : *keylen,
			addr : *addr,
			reqs : *reqs,
		})
		total_reqs += uint64(*reqs)
	}

	wg.Add(*routinenum)
	start_set_time := time.Now()
	for i := 0; i < len(ctx); i++ {
		go func(t *Task, id int) {
			defer wg.Add(-1)
			valueprefix, _ := GetStringInSize(t.datalen, "D")
			keyprefix, _ := GetStringInSize(t.keylen, "K")
			var rq int
			for rq = 0; rq < t.reqs; rq++ {
				key := strconv.Itoa(id) + keyprefix + strconv.Itoa(rq)
				value := strconv.Itoa(id) + valueprefix + strconv.Itoa(rq)
				_, err := t.client.Do("set", key, value)
				if err != nil {
					fmt.Println("Set Error:", err)
					break
				}
			}
		}(&ctx[i], i)
	}
	wg.Wait()
	end_set_time := time.Now()

	wg.Add(*routinenum)
	start_get_time := time.Now()
	for i := 0; i < len(ctx); i++ {
		go func(t *Task, id int) {
			defer wg.Add(-1)
			keyprefix, _ := GetStringInSize(t.keylen, "K")
			var rq int
			for rq = 0; rq < t.reqs; rq++ {
				key := strconv.Itoa(id) + keyprefix + strconv.Itoa(rq)
				_, err := t.client.Do("get", key)
                if err != nil {
					fmt.Println("Get Error:", err, " key:", key)
					break
				}
			}
		}(&ctx[i], i)
	}
	wg.Wait()
	end_get_time := time.Now()
	total_set_time := GetTimeDelaUs(end_set_time.UnixNano(), start_set_time.UnixNano())
	total_get_time := GetTimeDelaUs(end_get_time.UnixNano(), start_get_time.UnixNano())
	set_per_sec := float64(total_reqs) / float64(total_set_time) * 1000
    get_per_sec := float64(total_reqs) / float64(total_get_time) * 1000
	fmt.Printf("total_reqs:[%d] set_per_sec:[%f] get_per_sec:[%f]\n", total_reqs, set_per_sec, get_per_sec)
}

func GetTimeDelaUs(end, start int64) int64 {
    return (end - start) / int64(time.Millisecond)
}
