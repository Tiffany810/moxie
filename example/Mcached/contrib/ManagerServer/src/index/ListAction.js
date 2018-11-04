function SlotListShow() {
    var div = document.getElementById("RightBox")
    var Slot = document.createElement("ul")
    if (div) {
        for (var i = 0; i < 10; ++i) {
            var l = document.createElement("li")
            var lt = document.createTextNode("list-" + i)
            l.style["height"] = "80px"
            l.appendChild(lt)
            Slot.appendChild(l)
        }
        div.appendChild(Slot)
    }
}