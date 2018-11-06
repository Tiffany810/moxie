var etcd_server_list_map = new Map()

function AddSlotItemToSlotList(slot, i) {
    var l = document.createElement("li")
    var lt = document.createTextNode("list-" + i)
    l.style["height"] = "80px"
    l.appendChild(lt)
    slot.appendChild(l)
}

function hide_right_body_box() {
    $("#right_slot_box").hide()
    $("#right_group_box").hide()
}

function show_right_body_box(box_id) {
    hide_right_body_box()
    $(box_id).show()
}

function set_right_box_show_header_title (title) {
    var header_title = document.getElementById("right_box_show_header_title")
    header_title.innerText = title
}

function index_document_ready_init() {
    $("#right_slot_box").hide()
    $("#right_group_box").hide()
    set_right_box_show_header_title("集群管理端")
    var init_host_list = ["192.168.56.1:8897", "192.168.56.1:8898", "192.168.56.1:8899"]
    for (i = 0; i < init_host_list.length; i++) {
        var ip_port = jQuery.trim(init_host_list[i])
        add_etcd_hosts_to_list(ip_port)
    }
}

function slot_document_ready_init() {
    $("#right_slot_box").hide()
    $("#right_group_box").hide()
    set_right_box_show_header_title("集群管理端")
    var init_host_list = ["192.168.56.1:8897", "192.168.56.1:8898", "192.168.56.1:8899"]
    for (i = 0; i < init_host_list.length; i++) {
        var ip_port = jQuery.trim(init_host_list[i])
        add_etcd_hosts_to_list(ip_port)
    }
}

function group_document_ready_init() {
    $("#right_slot_box").hide()
    $("#right_group_box").hide()
    set_right_box_show_header_title("集群管理端")
    var init_host_list = ["192.168.56.1:8897", "192.168.56.1:8898", "192.168.56.1:8899"]
    for (i = 0; i < init_host_list.length; i++) {
        var ip_port = jQuery.trim(init_host_list[i])
        add_etcd_hosts_to_list(ip_port)
    }
}

function slot_box_show() {
    set_right_box_show_header_title("Slot管理组件")
    show_right_body_box("#right_slot_box")
}

function group_box_show() {
    set_right_box_show_header_title("Group管理组件")
    show_right_body_box("#right_group_box")
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
