var etcd_server_list_map = new Map()

function process_slot_metadata(response) {
    if (response.ext == "") {
        alert("No enough slot data")
        return
    }
    var slots = jQuery.parseJSON(response.ext)
    if (slots != undefined && slots != null 
        && slots.slots != undefined && slots.slots != null) {
        if (slots.slots.length > 0) {
            clear_slot_item_in_slot_list_body()
        }
        for (i = 0; i < slots.slots.length; i++) {
            add_slot_item_to_slot_list_body(slots.slots[i])
        }
    }
}

function ajax_request_slot_json_data(page_index) {
    $.ajax({
        type: "post",
        url: "http://127.0.0.1:8898/mcached/queryinfos/",
        dataType: "json",
        data: JSON.stringify( {
            "cmd_type":0,
             "request":"{\"page_size\":10, \"page_index\":" + page_index.toString() + "}"
        }),
        beforeSend: function(xhr) {
           // set header
        },
        success: function(data) {
            var response = data;
            var type = response.succeed
            if (type) {
                process_slot_metadata(response)
            } else {
                alert(response.msg)
            }
        },
        error: function(data){
            alert("error");
        },
    });
}

function process_group_metadata(response) {
    if (response.ext == "") {
        alert("No enough group data")
        return
    }
    var groups = jQuery.parseJSON(response.ext)
    if (groups != undefined && groups != null 
        && groups.groups != undefined && groups.groups != null) {
        if (groups.groups.length > 0) {
            clear_group_item_in_group_list_body()
        }
        for (i = 0; i < groups.groups.length; i++) {
            add_group_item_to_group_list_body(groups.groups[i])
        }
    }
}

function ajax_request_group_json_data(page_index) {
    $.ajax({
        type: "post",
        url: "http://127.0.0.1:8898/mcached/queryinfos/",
        dataType: "json",
        data: JSON.stringify( {
            "cmd_type":1,
             "request":"{\"page_size\":10, \"page_index\":" + page_index.toString() + "}"
        }),
        beforeSend: function(xhr) {
           // set header
        },
        success: function(data) {
            var response = data;
            var type = response.succeed
            if (type) {
                process_group_metadata(response)
            } else {
                alert(response.msg)
            }
        },
        error: function(data){
            alert("error");
        },
    });
}

function init_slot_page_split() {
    $("#page_split_slot").Page({
        totalPages: 103,//分页总数
        liNums: 7,//分页的数字按钮数(建议取奇数)
        activeClass: 'activP', //active 类样式定义
        callBack : function(page){
            ajax_request_slot_json_data(page)
        }
    });
}

function init_group_page_split() {
    $("#page_split_group").Page({
        totalPages: 5,//分页总数
        liNums: 7,//分页的数字按钮数(建议取奇数)
        activeClass: 'activP', //active 类样式定义
        callBack : function(page){
            ajax_request_group_json_data(page)
        }
    });
}

function hide_right_body_box() {
    $("#right_slot_box").hide()
    $("#right_group_box").hide()
    $("#right_manager_box").hide()
}

function show_right_body_box(box_id) {
    hide_right_body_box()
    $(box_id).show()
}

function set_right_box_show_header_title (title) {
    var header_title = document.getElementById("right_box_show_header_title")
    header_title.innerText = title
}

function manager_box_show() {
    set_right_box_show_header_title("集群管理端")
    show_right_body_box("#right_manager_box")
}

function slot_box_show() {
    set_right_box_show_header_title("Slot管理组件")
    show_right_body_box("#right_slot_box")
    init_slot_page_split()
    ajax_request_slot_json_data(1)
}

function group_box_show() {
    set_right_box_show_header_title("Group管理组件")
    show_right_body_box("#right_group_box")
    init_group_page_split()
    ajax_request_group_json_data(1)
}

function process_manager_metadata(json) {  
    var  str = "succeed:" + json.succeed + " ecode:" + json.ecode + " msg:" + json.msg;
    //alert(str)
}

function get_manager_metddata() {
    $.ajax({
        type: "post",
        url: "http://127.0.0.1:8696/observe/",
        dataType: "json",
        data: JSON.stringify( {
            "server_name": "Observe"
        }),
        beforeSend: function(xhr) {
           // set header
        },
        success: function(data) {
            var response = data;
            var type = response.succeed
            if (type) {
                process_manager_metadata(response)
            } else {
                alert(response.msg)
            }
        },
        error: function(data){
            alert("error");
        },
    });
}

function index_document_ready_init() {
    manager_box_show()
    var init_host_list = ["192.168.56.1:8897", "192.168.56.1:8898", "192.168.56.1:8899"]
    for (i = 0; i < init_host_list.length; i++) {
        var ip_port = jQuery.trim(init_host_list[i])
        add_etcd_hosts_to_list(ip_port)
    }
    get_manager_metddata()
    init_slot_operations_slot_dialog()
}

function clear_slot_item_in_slot_list_body() {
    $("#slot_metadata_table tr:not(:first)").html(""); 
}

function add_slot_item_to_slot_list_body(json) {
    var slot_list_content_body = document.getElementById("slot_list_content_body")
    var tr = document.createElement("tr")
    var td_slot_id = document.createElement("td")
    var td_is_adjust = document.createElement("td")
    var td_group_id = document.createElement("td")
    var td_dest_id = document.createElement("td")
    var td_operations = document.createElement("td")

    td_slot_id.innerText = json.index
    td_is_adjust.innerText = json.is_adjust
    td_group_id.innerText = json.group_id
    td_dest_id.innerText = json.dst_froup_id

    var use_btn = document.createElement("button")
    var move_btn = document.createElement("button")
    var del_btn = document.createElement("button")

    use_btn.type = "button"
    use_btn.className = "btn btn-success"
    use_btn.innerText = "Add"
    use_btn.onclick = function() {
        //add_slot_to_group(use_btn)
        show_add_slot_to_group_dialog()
    }

    move_btn.type = "button"
    move_btn.className = "btn btn-warning"
    move_btn.innerText = "Mov"
    move_btn.onclick = function() {
        move_slot_from_group_to_dest(move_btn)
    }

    del_btn.type = "button"
    del_btn.className = "btn btn-danger"
    del_btn.innerText = "Del"
    del_btn.onclick = function() {
        del_slot_from_group(del_btn)
    }

    td_operations.appendChild(use_btn)
    td_operations.appendChild(document.createTextNode(" "))
    td_operations.appendChild(move_btn)
    td_operations.appendChild(document.createTextNode(" "))
    td_operations.appendChild(del_btn)

    tr.appendChild(td_slot_id)
    tr.appendChild(td_is_adjust)
    tr.appendChild(td_group_id)
    tr.appendChild(td_dest_id)
    tr.appendChild(td_operations)

    slot_list_content_body.appendChild(tr)
}

function clear_group_item_in_group_list_body() {
    $("#group_metadata_table tr:not(:first)").html(""); 
}

function get_combine_of_group_slots(slots_array) {
    var ret_str = ""

    if (slots_array.length <= 0) {
        return ret_str
    }

    var ls = 0
    var arr_len = slots_array.length
    var s = 1
    for (; s < slots_array.length; s++) {
        if (slots_array[s] - slots_array[s - 1] == 1) {
            continue  
        }
        if (s - 1 == ls) {
            ret_str = ret_str + slots_array[ls]
        } else {
            ret_str = ret_str + "[" + slots_array[ls] + "-" + slots_array[s - 1] + "]"
        }

        if (ls != arr_len - 1) {
            ret_str = ret_str + ","
        }

        ls = s
    }

    if (s - 1 == ls) {
        ret_str = ret_str + slots_array[ls]
    } else {
        ret_str = ret_str + "[" + slots_array[ls] + "-" + slots_array[s - 1] + "]"
    }

    return ret_str
}

function add_group_item_to_group_list_body(json) {
    var group_list_content_body = document.getElementById("group_list_content_body")
    var tr = document.createElement("tr")
    var td_group_id = document.createElement("td")
    var td_is_activated = document.createElement("td")
    var td_slots = document.createElement("td")
    var td_operations = document.createElement("td")

    td_group_id.innerText = json.group_id
    td_is_activated.innerText = json.is_activated
    td_slots.innerText = get_combine_of_group_slots(json.slots)

    var del_btn = document.createElement("button")

    del_btn.type = "button"
    del_btn.className = "btn btn-danger"
    del_btn.innerText = "Del"
    del_btn.onclick = function() {
        del_group(del_btn)
    }

    td_operations.appendChild(del_btn)

    tr.appendChild(td_group_id)
    tr.appendChild(td_is_activated)
    tr.appendChild(td_slots)
    tr.appendChild(td_operations)

    group_list_content_body.appendChild(tr)
}

function add_etcd_hosts_to_list(ip_port) {
    var etcd_server_hosts_list = document.getElementById("etcd_server_hosts_list")
    var l = document.createElement("li")
    var host_text = document.createTextNode(ip_port)
    var span = document.createElement("span")
    var motify_btn = document.createElement("button")
    var delete_btn = document.createElement("button")

    l.className = "list-group-item"
    span.className = "left_delete_padding_button"

    motify_btn.type = "button"
    motify_btn.className = "btn btn-warning"
    motify_btn.innerText = "Motify"
    motify_btn.onclick = function() {
        hosts_list_on_motify(motify_btn)
    }

    delete_btn.type = "button"
    delete_btn.className = "btn btn-warning"
    delete_btn.innerText = "Delete"
    delete_btn.onclick = function() {
        hosts_list_on_delete(delete_btn)
    }

    span.style["float"] = "right"
    span.appendChild(delete_btn)
    span.appendChild(document.createTextNode(" "))
    span.appendChild(motify_btn)

    l.style["text-align"] = "left"
    l.style["height"] = 50
    l.id = ip_port

    l.appendChild(host_text)
    l.appendChild(span)
    etcd_server_hosts_list.appendChild(l)
    etcd_server_list_map.set(ip_port, l)
}

function add_etcd_server_hosts() {
    var ip_port_input = document.getElementById("ip_port_input").value
    var hosts = new Array();
    ip_port_input = jQuery.trim(ip_port_input)
    if (ip_port_input == "") {
        alert("Input empty!");
        return
    }

    var succ_count = 0
    hosts = ip_port_input.split(",")
    for (i = 0; i < hosts.length; i++) {
        if (!etcd_server_list_map.has(hosts[i]) ) {
            add_etcd_hosts_to_list(hosts[i]);
            succ_count++
        }
    }

    alert("Add [" + succ_count.toString() + "] ip:port")
}

function hosts_list_on_motify(obj) {
    document.getElementById(obj.parentElement.parentElement.id).style.display = "none"
}

function hosts_list_on_delete(obj) {
    document.getElementById(obj.parentElement.parentElement.id).style.display = "none"
    if (etcd_server_list_map.has(obj.parentElement.parentElement.id)) {
        etcd_server_list_map.delete(obj.parentElement.parentElement.id)
    }
}

function show_add_slot_to_group_dialog() {
    $("#add_to_group_slot_operations_dialog").dialog("open");
}

function init_slot_operations_slot_dialog() {
    $("#add_to_group_slot_operations_dialog").dialog({
      autoOpen: false,
      show: {
        effect: "blind",
        duration: 1000
      },
      hide: {
        effect: "explode",
        duration: 1000
      }
    });
}