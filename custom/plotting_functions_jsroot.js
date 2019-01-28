




var mdi = null, cnt = 0, drawopt = null, addr = null;

function initialize(){

  JSROOT.RegisterForResize('graphdiv');
  JSROOT.RegisterForResize('graphdiv21');
  JSROOT.RegisterForResize('graphdiv22');
  
}

function cleanJSDivs(){

  JSROOT.cleanup('graphdiv');
  JSROOT.cleanup('graphdiv21');
  JSROOT.cleanup('graphdiv22');
  JSROOT.RegisterForResize('graphdiv');
  JSROOT.RegisterForResize('graphdiv21');
  JSROOT.RegisterForResize('graphdiv22');
  
}


function xhrErrorResponse(listDirectories, errorString){

  console.log("Error ! " + errorString);
  document.getElementById("readstatus").innerHTML = "Couldn't get histogram data; request = "+ listDirectories + "; error = " + errorString + ". Did rootana httpserver die?";
  document.getElementById("readstatus").style.color = 'red';    
  // If we couldn't find histogram, try forcing re-find of rootana directory
  gFoundRootanaDir = false;
  xhrAlreadyInFligthJSROOT = false;
  find_active_root_directory();

}

var xhrAlreadyInFligthJSROOT = false;


// This method will create a plot in the requested 
// divs using the requested histograms.
// Use JSROOT plotting of data
function plotAllHistogramsJSROOT(plotType,divNames, histogramNameList, deleteDygraph){

  // Find the directory structure, if it is not yet found.
  if(!gFoundRootanaDir){
    find_active_root_directory();
    return;
  }

  // Don't make another request if last request isn't finished.
  if(xhrAlreadyInFligthJSROOT) return;
  xhrAlreadyInFligthJSROOT = true;

  // Wrap the request in promise; will combine multiple items (if there are more than 1) into single XHR request
  var listDirectories = "";
  for(var index = 0; index < histogramNameList.length; index++){

    var name = active_directory + "/" + histogramNameList[index];
    // If the histogram name has "Canvases/" then    
    // assume we don't need the active_directory
    if (histogramNameList[index].indexOf("Canvases/") >= 0){
      name = histogramNameList[index];
    }
    
    //name = "Canvases/Calibrated1VM4WithMagnetRamp";
    listDirectories += name + "/root.json\n";
    
    console.log(name + "/root.json");
  }    

  // compose URL
  var url = rootana_dir + "multi.json?number="+String(histogramNameList.length);
  console.log("url " + url); 
  getUrl(url, listDirectories).then(function(response) {
    
    xhrAlreadyInFligthJSROOT = false;
    
    var histo = JSON.parse(response);    
    // Little hack from Sergey to distinguish the different responses from THttpServer

    $("#graphdiv").html("");

    // Loop over the different histograms we got back.
    // Need to handle the overlay plots different from the single/multiple plots.
    for (var i=0;i<histo.length;++i){
      console.log(histo[i]);
      histo[i] = JSROOT.JSONR_unref(histo[i]);
      
      if(plotType == "overlay"){         
        if(i==0){
          JSROOT.draw(divNames[0], histo[i], "");
        } else {
          histo[i].fLineColor = i+2;
          JSROOT.draw(divNames[0], histo[i], "SAME");
        }
      }else{ // single or multiple
	  //        JSROOT.redraw(divNames[i], histo[i], "");
          JSROOT.redraw(divNames[i], histo[i]);
          console.log(divNames[i]);
      }
    }


     document.getElementById("readstatus").innerHTML = "Rootana data correctly read";
     document.getElementById("readstatus").style.color = 'black';      

   }).catch(function(error) {  // Handle exception if we didn't find the histogram...

     promiseAlreadyInFligth = false;
     xhrErrorResponse(listDirectories,error);
    
   });
  


}
