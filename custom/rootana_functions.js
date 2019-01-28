// This is a set of JS functions for dealing with the communication
// with the ROOT THttpServer; in particular, this file contains
// functions to figure out what the current set of available histograms is,
// as well as functions to delete the histogram.
// Also include Promisified XHR
// T. Lindner (Jan 2017)


// Promisified XLMHttpRequest wrapper...
// stolen from http://www.html5rocks.com/en/tutorials/es6/promises/
function getUrl(url, postData) {

  if (typeof(postData)==='undefined') postData = false;

  // Return a new promise.
  return new Promise(function(resolve, reject) {

    // Do the usual XHR stuff
    var req = new XMLHttpRequest();

    if(postData != false){
      req.open('POST', url);
    }else{
      req.open('GET', url);
    }


    req.onload = function() {
      // This is called even on 404 etc
      // so check the status
      if (req.status == 200) {
        // Resolve the promise with the response text
        resolve(req.response);
      }
      else {
        // Otherwise reject with the status text
        // which will hopefully be a meaningful error
        reject(req.statusText);
      }
    };

    // Handle network errors
    req.onerror = function() {
      reject("Network Error");
    };

    // Make the request
    if(postData != false){
      req.send(postData);
    }else{
      req.send();
    }
  });
}

// This variable keeps track of the webdirectory that the rootana THttpServer is posting to
rootana_dir = "";
gFoundRootanaDir = false;
// This is the ROOT directory where we are looking for histograms.  Form is either 'rootana' or 'Files/somenameXXX.root'
active_directory = "";
// This is the full address to the current ROOT directory
histo_address = "";
// This is the list of objects in current ROOT directory.  This is a local copy of information that the rootana server provides.
gHistogramList = {};


// Find the correct histogram in current ROOT directory list
function findHistogram(histogramName){
    

  // Try stripping a directory part of histogram name...
  histogramName2 = histogramName.split("/").pop();

  for(var j = 0; j < gHistogramList.length; j++){
    var object = gHistogramList[j];
    
    if(object["_name"] == histogramName){
      return object;      
    }
    // Check the DirectoryFile sub-directories too... 
    if(object["_kind"] == "ROOT.TDirectoryFile"){     
      for(var k = 0; k < object["_childs"].length; k++){
	var object2 = object["_childs"][k]; 
	if(object2["_name"] == histogramName2 ){
	  return object2;
	}
      }
    }                                                                                                                  
  }

  return false;
  
}


// Check the objects in a ROOT directory tree; return true if one of them is a TH1D
// Also check for sub-directories in file 
function check_for_histograms(subdir_tree){

  if(! '_child' in subdir_tree){
    document.getElementById("readstatus").innerHTML = "Don't have child" ;
    return false;
  }
  
  histList = []
  for(var j = 0; j < subdir_tree["_childs"].length; j++){
    var object = subdir_tree["_childs"][j];
    if(object["_kind"] == "ROOT.TH1D"){
      // Found a histogram object.
      return true;
    }

    // Check the DirectoryFile sub-directories too...
    if(object["_kind"] == "ROOT.TDirectoryFile"){    
      for(var k = 0; k < object["_childs"].length; k++){                                                                             
	var object2 = object["_childs"][k];
	if(object2["_kind"] == "ROOT.TH1D"){ 
	  // Found a histogram object. 
	  return true; 
	}          
      }
    }
  }
    
};

function parseRootDirectory(response){

    var rootStructureJSON;

    try {
      rootStructureJSON = JSON.parse(response);

    }
    catch(err) {
      document.getElementById("readstatus").innerHTML = "err " + err.message;
      return;
    }
    
  
    // Loop over the objects in child object, looking for either the rootana directory
    // or Files directory
    
    console.log("Parsing");
    for(var i = 0; i < rootStructureJSON["_childs"].length; i++){
      var testDir = rootStructureJSON["_childs"][i];
      var name = testDir["_name"];
      

      // Check for histograms in the root directory
      if(testDir["_kind"] == "ROOT.TH1D"){ 
        active_directory = "";
        histo_address = rootana_dir  + active_directory;
	gFoundRootanaDir = true;
	gHistogramList = rootStructureJSON["_childs"];
      }
      
      // Check for histograms in the Files directory
      if( name == "Files"){
        for(var j = 0; j < testDir["_childs"].length; j++){
          var fileDir = testDir["_childs"][j];
          var foundHistograms = check_for_histograms(fileDir);
          // If we found a valid ROOT directory, save information for later plotting
          if(foundHistograms){
            active_directory = "Files/" + fileDir["_name"];
            histo_address = rootana_dir  + active_directory;
            //document.getElementById("readstatus").innerHTML = "Getting list of available histograms...";
            //document.getElementById("readstatus").style.color = 'black';    
            // Get the full list of histograms, so we can check them later
            gHistogramList = fileDir["_childs"];
            gFoundRootanaDir = true;
          }
        }
      }
    }


}


// This method will search the root directory structure.
// Start by looking for histograms in the rootana directory.
// If we don't find any histograms there, then look in the file directories.
// Can request to make the call asynchronously or synchronously; default is async with promise 
function find_active_root_directory(async){

  if (typeof(async)==='undefined') async = true;
  
  if(!async){

    // Get the JSON description of current ROOT directory
    var request = new XMLHttpRequest();
    request.open('GET', rootana_dir + "h.json", false);
    request.send(null);
  
    if(request.status != 200){
      document.getElementById("readstatus").innerHTML = "Couldn't get basic ROOT structure back; status = " + request.status + ". Is rootana httpserver running?"; 
      document.getElementById("readstatus").style.color = 'red';
      return;
    }
    parseRootDirectory(request.responseText);

    return
  }


  // Wrap the XHR request in promise.
  getUrl(rootana_dir + "h.json").then(function(response) {

    parseRootDirectory(response);

  }).catch(function(error) { // Handle exception if we didn't find ROOT structure

    document.getElementById("readstatus").innerHTML = "ASYNC Couldn't get basic ROOT structure back; status = " + error + ". Is rootana httpserver running?";   
    document.getElementById("readstatus").style.color = 'red';

  });
}

function rootanaResetHistogram(histogramName){

  // Find the directory structure, if it is not yet found.
  if(!gFoundRootanaDir){
    find_active_root_directory();
  }

  // Send the request for data for this plot
  var request = new XMLHttpRequest();
  request.open('GET', histo_address + "/" + histogramName +"/exe.json?method=Reset", false);
  console.log("Sending : " + histo_address + "/" + histogramName +"/exe.json?method=Reset");
  request.send(null);
  if(request.status != 200){ 
    document.getElementById("readstatus").innerHTML = "Failed to reset histogram " + histogramName;
    document.getElementById("readstatus").style.color = 'red';
    return false;
  }
  document.getElementById("readstatus").innerHTML = "Reset histogram " + histogramName;

  return true;

}

function rootanaResetAllHistograms(){

  for(var j = 0; j < gHistogramList.length; j++){
    var object = gHistogramList[j];
    
    rootanaResetHistogram(object["_name"]);

  }
}

