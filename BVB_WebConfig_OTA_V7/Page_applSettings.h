//
//  HTML PAGE
//
const char PAGE_ApplicationConfiguration[] PROGMEM = R"=====(
<meta name="viewport" content="width=device-width, initial-scale=1" />
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<a href="/"  class="btn btn--s"><</a>&nbsp;&nbsp;<strong>Application Configuration</strong>
<hr>
Connect to Router with these settings:<br>
<form action="" method="get">
<table border="0"  cellspacing="0" cellpadding="3" style="width:350px" >
<tr><td align="right">Home:</td><td><input type="text" id="base" name="base" value=""></td></tr>
<tr><td align="right">Left:</td><td><input type="text" id="left" name="left" value=""></td></tr>
<tr><td align="right">Right:</td><td><input type="text" id="right" name="right" value=""></td></tr>
<tr><td align="right">Way to station (min):</td><td><input type="text" id="wayToStation" name="wayToStation" value=""></td></tr>
<tr><td align="right">Warning starts (min):</td><td><input type="text" id="warningBegin" name="warningBegin" value=""></td></tr>

<tr><td colspan="2" align="center"><input type="submit" style="width:150px" class="btn btn--m btn--blue" value="Save"></td></tr>
</table>
</form>

<script>

function GetState()
{
  setValues("/admin/applvalues");
}


window.onload = function ()
{
  load("style.css","css", function() 
  {
    load("microajax.js","js", function() 
    {
          setValues("/admin/applvalues");

    });
  });
}
function load(e,t,n){if("js"==t){var a=document.createElement("script");a.src=e,a.type="text/javascript",a.async=!1,a.onload=function(){n()},document.getElementsByTagName("head")[0].appendChild(a)}else if("css"==t){var a=document.createElement("link");a.href=e,a.rel="stylesheet",a.type="text/css",a.async=!1,a.onload=function(){n()},document.getElementsByTagName("head")[0].appendChild(a)}}

</script>


)=====";

//
//  SEND HTML PAGE OR IF A FORM SUMBITTED VALUES, PROCESS THESE VALUES
// 

void send_application_configuration_html()
{
  if (server.args() > 0 )  // Save Settings
  {
    for ( uint8_t i = 0; i < server.args(); i++ ) {
      if (server.argName(i) == "base") config.base =   urldecode(server.arg(i));
      if (server.argName(i) == "left") config.left =    urldecode(server.arg(i));
      if (server.argName(i) == "right") config.right =    urldecode(server.arg(i));
      if (server.argName(i) == "wayToStation") config.wayToStation =  urldecode(server.arg(i)).toInt();
      if (server.argName(i) == "warningBegin") config.warningBegin =  urldecode(server.arg(i)).toInt();
      }
    if (config.wayToStation>20) config.wayToStation=20;
    if (config.wayToStation<0) config.wayToStation=0;

    if (config.warningBegin>10) config.warningBegin=10;
    if (config.warningBegin<0) config.warningBegin=0;
    
    WriteConfig();
  }
   server.send ( 200, "text/html", PAGE_ApplicationConfiguration ); 
  Serial.println(__FUNCTION__); 
}



//
//   FILL THE PAGE WITH VALUES
//

void send_application_configuration_values_html()
{

  String values ="";

  values += "base|" + (String) config.base + "|input\n";
  values += "left|" +  (String) config.left + "|input\n";
  values += "right|" +  (String) config.right + "|input\n";
  values += "wayToStation|" +  (String) config.wayToStation + "|input\n";
   values += "warningBegin|" +  (String) config.warningBegin + "|input\n";
  
  server.send ( 200, "text/plain", values);
  Serial.print("1 ");
  Serial.println(__FUNCTION__); 
  AdminTimeOutCounter=0;
  
}
