function filter() {

    // alert(document.getElementById("table"))
    var table = document.getElementById("table");
    if (document.getElementById("table") != null)
    {
        alert('Table purged !')
        table.parentNode.removeChild(table)
    }
    else
    {
        alert('Nothing to purge !')
    }


    table = document.createElement("table");
    table.id = "table";

    var textb = document.getElementById("txtFilter");
    // var table = document.getElementById("jsontable");

    if(textb.value == "")
    {
        var row = table.insertRow(0);
        var cell00 = row.insertCell(0);
        cell00.innerHTML = "hoho"
    }
    else
    {
        var row = table.insertRow(0);
        var cell00 = row.insertCell(0);
        cell00.innerHTML = "hihi"
        // alert('Filter: ' + textb.value +' ' + myJSON.type)
    }

    // var combo = document.getElementById("combo");

    // var option = document.createElement("option");
    // option.text = textb.value;
    // option.value = textb.value;
    // try {
    //     combo.add(option, null); //Standard 
    // }catch(error) {
    //     combo.add(option); // IE only
    // }
    // textb.value = "";
    
    document.body.appendChild(table);

}


// var myJSON = {
//     "type": "Signal",
//     "time": {
//         "sec": 1343660321,
//         "usec": 774229
//     },
//     "time (human)": {
//         "hour": 14,
//         "min": 58,
//         "sec": 41
//     },
//     "serial": 3,
//     "path": "/org/freedesktop/DBus",
//     "interface": "org.freedesktop.DBus",
//     "member": "NameAcquired"
// }

// var myJSON2 = {
//     "type": "Signal",
//     "time": {
//         "sec": 1343660328,
//         "usec": 109260
//     },
//     "time (human)": {
//         "hour": 14,
//         "min": 58,
//         "sec": 48
//     },
//     "serial": 81,
//     "path": "/im/pidgin/purple/PurpleObject",
//     "interface": "im.pidgin.purple.PurpleInterface",
//     "member": "PluginLoad"
// }

// var myJSON_hour = myJSON["time (human)"].hour
// var myJSON2_hour = myJSON2["time (human)"].hour
// var qwe = myJSON_hour + myJSON2_hour
// document.body.appendChild(document.createTextNode("<b>Hello World!</b>"));
// document.body.appendChild(document.createTextNode(Date()));
// document.body.appendChild(document.createTextNode(myJSON_hour));
// document.write("Diff: <b>"+myJSON_hour-myJSON2hour+"</b>");
// document.write("Diff: <b>"+qwe+"</b>");

// document.write("Type of qwe: <b>"+typeof qwe+"</b><br>");
// document.write("Value of qwe: <b>"+qwe+"</b><br>");
// document.write("Type of myJSON2hour: <b>"+typeof myJSON2_hour+"</b><br>");
// document.write("Value of myJSON2_hour: <b>"+myJSON2_hour+"</b><br>");
// document.write("Value of myJSON2_hour + myJSON_hour: <b>"+myJSON2_hour+myJSON_hour+"</b>");

// var xobj = new XMLHttpRequest();
// xobj.overrideMimeType("application/json");
// xobj.open('GET', 'datos.json', true);
// xobj.onreadystatechange = function () {
//     if (xobj.readyState == 4) {
//         var jsonTexto = xobj.responseText;
//         document.write(jsonTexto);
//     }
// }
// xobj.send(null);

// function fibo(n)
// {
//     if(n==0 || n ==1)
//     {
//         return n
//     }
//     var prev=0
//     var res=1
//     var buf=0
//     for(i=2;i<=n; i++)
//     {
//         buf=res
//         res=res+prev
//         prev=buf
//     }
//     return res
// }

// document.body.appendChild(document.createTextNode(fibo(5) ));
// document.body.appendChild(document.createTextNode(fibo(0) ));
// document.body.appendChild(document.createTextNode(fibo(1) ));
// document.body.appendChild(document.createTextNode(fibo(10) ));

// holds a reference to the <h1> tag
// var h1 = document.getElementById("header");
// accessing the same <h1> element
// h1 = document.getElementsByTagName("h1")[0];
