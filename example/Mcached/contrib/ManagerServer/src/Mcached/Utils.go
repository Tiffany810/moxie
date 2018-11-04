package Mcached

import (
	"fmt"
	"net/http"
	"errors"
)

func GetRedirectRequestUrl(request *http.Request, redirect_host string) string {
	if request == nil {
		return ""
	}
	return redirect_host + request.RequestURI
}

func BuildKeepAliveMasterKey(server_name string, groupid uint64, host string) string {
	key_prefix := fmt.Sprintf("%s%s/%d/master/%s", KeepAliveWorkRootDef, server_name, groupid, host)
	return key_prefix
}

func BuildKeepAliveSlaveKey(server_name string, groupid uint64, host string) string {
	key_prefix := fmt.Sprintf("%s%s/%d/slave/%s", KeepAliveWorkRootDef, server_name, groupid, host)
	return key_prefix
}

func BuildCacheGroupIdKey(id uint64) string {
	return fmt.Sprintf("%s%d", CachedGroupPrefix, id)
}

func BuildSlotIndexKey(index uint64) string {
	return fmt.Sprintf("%s%d", SlotsPrefix, index)
}


func LocalDeleteSlotFromGroup(slot *SlotItem, group *CacheGroupItem) (bool, error, int32) {
	if slot.Groupid != group.GroupId {
		return false, errors.New("The slot not belongs to this group"), Error_SlotHasOwnGroup
	} 
	
	index := -1

	for i := 0; i < len(group.SlotsIndex); i++ {
		if group.SlotsIndex[i] == slot.Index {
			index = i
			break
		}
	}

	if index == -1 {
		return false, errors.New("Slot can't found in this Group!"), Error_SlotNotInThisGroup
	}

	tmp := make([]uint64, 0)
	tmp = append(tmp, group.SlotsIndex[:index]...)
	tmp = append(tmp, group.SlotsIndex[index + 1:]...)

	group.SlotsIndex = tmp
	slot.Groupid = 0
	slot.IsAdjust = false
	slot.DstGroupid = 0

	return true, nil, Error_NoError
}

func LocalAddSlotToGroup(slot *SlotItem, group *CacheGroupItem) (bool, error,int32) {
	if slot.Groupid == group.GroupId {
		return false, errors.New("The slot belongs to this group"), Error_SlotHasInThisGroup
	} 
	
	index := -1

	for i := 0; i < len(group.SlotsIndex); i++ {
		if group.SlotsIndex[i] == slot.Index {
			index = i
			break
		}
	}

	if index != -1 {
		return false, errors.New("Slot found in this Group!"), Error_SlotHasInThisGroup
	}

	group.SlotsIndex = append(group.SlotsIndex, slot.Index)
	slot.Groupid = group.GroupId
	slot.IsAdjust = false
	slot.DstGroupid = 0

	return true, nil, Error_NoError
}

func SlotInGroup(slot *SlotItem, group *CacheGroupItem) (bool, error, int32) {
	if slot.Groupid != group.GroupId {
		return false, errors.New("The slot belongs to this group"), Error_SlotNotInThisGroup
	} 
	
	index := -1

	for i := 0; i < len(group.SlotsIndex); i++ {
		if group.SlotsIndex[i] == slot.Index {
			index = i
			break
		}
	}

	if index == -1 {
		return false, errors.New("Slot not found in this Group!"), Error_SlotNotInThisGroup
	}

	return true, nil, Error_NoError
}

// 交换数组总right和left下标所指向的元素
func Swap(arr []uint64, left, right uint64) {
    tmp := arr[left]
    arr[left] = arr[right]
    arr[right] = tmp 
}

func BuildMaxHeap(arr []uint64) {
	var index int64 = 0
	for index = int64(len(arr))/2 - 1; index >= 0; index-- {
        // 判断树index处的节点是否有两个子类
        maxChild := index
        if (2*index + 2) >= int64(len(arr)) {
            // 只有一个子类
            maxChild = index*2 + 1
        } else {
            // 两个子类
            if arr[index*2+1] >= arr[index*2+2] {
                maxChild = index*2 + 1
            } else {
                maxChild = index*2 + 2
            }
        }
        if arr[index] < arr[maxChild] {
            // 与较大的子节点交换一下位置
            Swap(arr, uint64(index), uint64(maxChild))
        }
    }
}

// 堆排序，非稳定排序
func HeapSort(arr []uint64) []uint64 {
    for i := len(arr) - 1; i > 0; i-- {
        BuildMaxHeap(arr[:i+1])
        Swap(arr, 0, uint64(i))
	}
	return arr
}

func Max(left, right int) int {
	if left >= right {
		return left
	}
	return right
}

func Min(left, right int) int {
	if left <= right {
		return left
	}
	return right
}