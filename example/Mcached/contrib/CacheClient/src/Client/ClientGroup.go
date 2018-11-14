package Client

import (
	"log"
	"sync"

	"github.com/deckarep/golang-set"
	"github.com/gomodule/redigo/redis"
)

type ClientGroup struct {
	slavesMutex   sync.Mutex
	masterMutex   sync.Mutex
	slavesClient  map[string]redis.Conn
	masterClient  redis.Conn
	masterAddress string
	slavesAddress mapset.Set
}

func (group *ClientGroup) OnMasterUpdate(master string) {
	group.masterMutex.Lock()
	defer group.masterMutex.Unlock()
	if master == group.masterAddress {
		return
	}
	client, err := redis.Dial("tcp", master)
	if err != nil {
		log.Println("Create new redis client error:", err)
		return
	}
	group.masterClient.Close()
	group.masterClient = client
}

func (group *ClientGroup) OnSlavesUpdate(slaves mapset.Set) {
	group.slavesMutex.Lock()
	defer group.slavesMutex.Unlock()
	adds := slaves.Difference(group.slavesAddress)
	removes := group.slavesAddress.Difference(slaves)

	group.removeSlavesClient(removes)
	group.createSlavesClient(adds)
}

func (group *ClientGroup) createSlavesClient(adds mapset.Set) {
	adds.Each(func(elem interface{}) bool {
		client, err := redis.Dial("tcp", elem.(string))
		if err != nil {
			log.Println("Create new redis client error:", err)
			return false
		}
		if _, ok := group.slavesClient[elem.(string)]; !ok {
			group.slavesClient[elem.(string)] = client
		}
		return true
	})
}

func (group *ClientGroup) removeSlavesClient(adds mapset.Set) {
	adds.Each(func(elem interface{}) bool {
		if c, ok := group.slavesClient[elem.(string)]; ok {
			c.Close()
			delete(group.slavesClient, elem.(string))
		}
		return true
	})
}
