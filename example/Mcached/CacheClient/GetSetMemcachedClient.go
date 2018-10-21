package main

import (
	"flag"
	"fmt"
	"github.com/bradfitz/gomemcache/memcache"
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

func main() {
	routinenum := flag.Int("rn", 4, "Go routine Num!")
	addr := flag.String("ip", "127.0.0.1:11211", "The address of redis server!")
	keylen := flag.Uint("keylen", 30, "The default length of key!")
	datalen := flag.Uint("datalen", 128, "The default length of data!")
	reqs := flag.Int("reqs", 10000, "The times of this test!")
	flag.Parse()

	if *routinenum <= 0 || *reqs <= 0 {
		fmt.Println("Args Error!")
		return
	}

	fmt.Printf("rn = %d, ip = %s, keylen = %d, datalen = %d, reqs = %d\n", *routinenum, *addr, *keylen, *datalen, *reqs)

	var wg sync.WaitGroup
	wg.Add(*routinenum)

	var print_mutex sync.Mutex
	var total_reqs uint64 = 0
    var total_set_time int64 = 0
    var total_get_time int64 = 0
	for i := 0; i < *routinenum; i++ {
		go func(id int, addr string, kenlen, datalen uint, reqs int) {
			memcachedclient := memcache.New(addr)
            memcachedclient.Timeout = 5000 * time.Millisecond
			if memcachedclient == nil {
				wg.Add(-1)
				return
			}
			defer wg.Add(-1)
			// do request
			valueprefix, _ := GetStringInSize(datalen, "D")
			keyprefix, _ := GetStringInSize(kenlen, "K")
			var rq int
	        start_set_time := time.Now()
			for rq = 0; rq < reqs; rq++ {
				key := strconv.Itoa(id) + keyprefix + strconv.Itoa(rq)
				value := strconv.Itoa(id) + valueprefix + strconv.Itoa(rq)
				err := memcachedclient.Set(&memcache.Item{Key: key, Value: []byte(value)})
				if err != nil {
                    fmt.Println("Set Error:", err)
					break
				}
                //if rq == 0 {
                //    fmt.Println("Set key:", key)
                //}
			}
            end_set_time := time.Now()
            /*
			for grq := 0; grq < rq; grq++ {
				key := strconv.Itoa(id) + keyprefix + strconv.Itoa(grq)
				_, err := memcachedclient.Get(key)
				if err != nil {
					fmt.Println("Get Error:", err, " key:", key)
					break
				}
			}
            */
            end_get_time := time.Now()
			print_mutex.Lock()
			total_reqs += uint64(rq)
			total_set_time += GetTimeDelaUs(end_set_time.UnixNano(), start_set_time.UnixNano())
			total_get_time += GetTimeDelaUs(end_get_time.UnixNano(), end_set_time.UnixNano())
            print_mutex.Unlock()
		}(i, *addr, *keylen, *datalen, *reqs)
	}

	wg.Wait()
    set_per_sec := float64(total_reqs) / float64(total_set_time) * 1000
    get_per_sec := float64(total_reqs) / float64(total_get_time) * 1000
	fmt.Printf("total_reqs:[%d] set_per_sec:[%f] get_per_sec:[%f]\n", total_reqs, set_per_sec, get_per_sec)
}

func GetTimeDelaUs(end, start int64) int64 {
    return (end - start) / int64(time.Millisecond)
}
