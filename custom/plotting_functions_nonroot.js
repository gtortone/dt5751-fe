
// Global pointer to dygraph objects
// Currently supports 4 dygraph objects
gDygraphPointer = [0,0,0,0];


gPlotlyPointer = 0;

var data = 'undefined';

function deleteOldPlots(){


  for(var i = 0; i < 4; i++){
    if(gDygraphPointer[i]){
      gDygraphPointer[i].destroy();
      gDygraphPointer[i] = 0;
    }
  }

  data = 'undefined';

  Plotly.purge("graphdiv");
  Plotly.purge("graphdiv21");
  Plotly.purge("graphdiv22");


}

function makePlot2D(divName, histoObject,histoInfoJSON, dygraphIndex,deleteDygraph){


  var redraw = ($("#"+divName).html() != "");

  var title = histoObject["_title"];

  // Need to recalculate the bin centers.
  var bin_widthX = (histoInfoJSON["fXaxis"]["fXmax"] - histoInfoJSON["fXaxis"]["fXmin"]) / histoInfoJSON["fXaxis"]["fNbins"];
  var bin_widthY = (histoInfoJSON["fYaxis"]["fXmax"] - histoInfoJSON["fYaxis"]["fXmin"]) / histoInfoJSON["fYaxis"]["fNbins"];

  var xarr = new Array(histoInfoJSON["fXaxis"]["fNbins"]);
  var yarr = new Array(histoInfoJSON["fYaxis"]["fNbins"]);
  var zarr = new Array(histoInfoJSON["fYaxis"]["fNbins"]);
  for (var i=0; i<histoInfoJSON["fYaxis"]["fNbins"]; i++){
    zarr[i] = new Array(histoInfoJSON["fXaxis"]);
  }

  // Urgh, plot.ly seems to define arrays opposite to ROOT
  for (var iyy = 1; iyy <= histoInfoJSON["fYaxis"]["fNbins"]; iyy++){
    yarr[iyy - 1] = Number(histoInfoJSON["fYaxis"]["fXmin"] + (iyy - 0.5) * bin_widthY);
    
    for (var ixx = 1; ixx <= histoInfoJSON["fXaxis"]["fNbins"]; ixx++){
      var index = iyy*(histoInfoJSON["fXaxis"]["fNbins"] +2) + ixx;
      zarr[iyy-1][ixx-1] = Number(histoInfoJSON["fArray"][index]);
      
      if (iyy == 1) {
        xarr[ixx - 1] = Number(histoInfoJSON["fXaxis"]["fXmin"] + (ixx - 0.5) * bin_widthX);
      }
    }
  }

  if (data != 'undefined' && redraw) {
    data[0]['z'] = zarr;
    Plotly.redraw(divName);
    return;
  }

  data = [
      {
          x: xarr,
          y: yarr,
          z: zarr,
          colorscale: [
              ['0.0', 'rgb(255,255,255)'],
              ['0.0000001', 'rgb(171,217,233)'],
              ['0.333333333333', 'rgb(116,173,209)'],
              ['0.666666666667', 'rgb(69,117,180)'],
              ['1.0', 'rgb(49,54,149)']
          ],
          type: 'heatmap'
      }
  ];

  var layout = {
      title: histoObject["_title"],
      xaxis: {
	  title: histoInfoJSON["fXaxis"]["fTitle"],
      },
      yaxis: {
	  title: histoInfoJSON["fYaxis"]["fTitle"],
      },
      showlegend: false
  };


  Plotly.newPlot(divName, data,layout);

}


function makePlot1D(histoInfoJSONFirst,plotType,divName,csv_array,deleteDygraph,dygraphIndex){  

  var title = histoInfoJSONFirst["fName"];
  if(plotType == "overlay"){
    title = "Overlay Histograms";
  }

  //  g = new Dygraph(document.getElementById(divName),csv_array_multi,{title: 'multiple histograms'});
  if(deleteDygraph || gDygraphPointer[dygraphIndex] == 0){
    delete gDygraphPointer[dygraphIndex] ;
    gDygraphPointer[dygraphIndex]  = new Dygraph(document.getElementById(divName),csv_array,
                                                 {title: title, 'labelsSeparateLines' : true, 'legend' : 'always', ylabel : histoInfoJSONFirst["fYaxis"]["fTitle"], xlabel : histoInfoJSONFirst["fXaxis"]["fTitle"]});
  }else{
    gDygraphPointer[dygraphIndex].updateOptions( { 'file': csv_array} );
  }
  document.getElementById("readstatus").innerHTML = "Rootana data correctly read";
  document.getElementById("readstatus").style.color = 'black';

}



// This function handles making a CSV object for use in dygraph 1D plot...
// The process for making the CSV is different for a single plot vs overlay plot.
function makeCSVArray(plotType,histoInfoJSON,histoObject,dataIndex){

  if (typeof(dataIndex)==='undefined') dataIndex = 0;
  
  if(plotType == "single" || plotType == "multiple"){
      
    // Fill the CSV array to make the histogram.

    // Need to recalculate the bin centers.
    var bin_width = (histoInfoJSON[dataIndex]["fXaxis"]["fXmax"] - histoInfoJSON[dataIndex]["fXaxis"]["fXmin"]) / histoInfoJSON[dataIndex]["fXaxis"]["fNbins"]
    var csv_array = histoInfoJSON[dataIndex]["fXaxis"]["fTitle"] + ", " + histoInfoJSON[dataIndex]["fYaxis"]["fTitle"] + "\n";

    for (i = 0; i < histoInfoJSON[dataIndex]["fXaxis"]["fNbins"]; i++){
      // remember, we skip the first bin of data, since it is the underflow bin...
      var bin_center = (i*bin_width) + bin_width/2.0;
      csv_array += bin_center + "," +   histoInfoJSON[dataIndex]["fArray"][i+1] + "\n";    
    }
    return csv_array;
    
  }else{ // handle consolidating data in N-dimensional CSV array...

    histoNames = [];
    histoValues = [];
    histoBinCenters = [];
    histoXTitle = ""


    // loop over all the results, consolidating them into CSV
    var title = histoObject["_title"];
    for(var index = 0; index < histoInfoJSON.length; index++){
      histoNames.push(histoInfoJSON[index]["fTitle"]);
      
      // Need to recalculate the bin centers.
      var bin_width = (histoInfoJSON[index]["fXaxis"]["fXmax"] - histoInfoJSON[index]["fXaxis"]["fXmin"]) / histoInfoJSON[index]["fXaxis"]["fNbins"];
      
      if(index == 0)
        histoXTitle = histoInfoJSON[index]["fXaxis"]["fTitle"]
      arrayValues = [];
      for (i = 0; i < histoInfoJSON[index]["fXaxis"]["fNbins"]; i++){
        // remember, we skip the first bin of data, since it is the underflow bin...
        var bin_center = (i*bin_width) + bin_width/2.0;

        // same the bin centers using first histogram
        if(index == 0){ histoBinCenters.push(bin_center);}      
        arrayValues.push(histoInfoJSON[index]["fArray"][i+1]);
      }
    
      histoValues.push(arrayValues);

    }

    // Create the CVS array for the combined information.
    var csv_array_multi = histoXTitle;
    for(var ii = 0; ii < histoNames.length; ii++){
      csv_array_multi += ", " + histoNames[ii];
    }
    csv_array_multi += "\n";
    for(var ii = 0; ii < histoValues[0].length; ii++){
      csv_array_multi += histoBinCenters[ii];
      for(var jj = 0; jj < histoValues.length; jj++){
        csv_array_multi += ", " + histoValues[jj][ii];
      }
      csv_array_multi += "\n";    
    }

    return csv_array_multi;
    
  }

}

var promiseAlreadyInFligth = false;

// This method will create a plot in the requested 
// divs using the requested histograms.
// We use a multi.json to get the data for all the histograms we want to plot;
// we wrap this in a promise to provide async response.
// Then handle the data we get back differently for single histogram vs overlay vs multiple,
// as well as handling 1D vs 2D...
function plotAllHistogramsNonROOT(plotType,divNames, histogramNameList, deleteDygraph){

  // Don't make another request if last request isn't finished.
  if(promiseAlreadyInFligth) return;
  promiseAlreadyInFligth = true;

  // Find the directory structure, if it is not yet found.
  if(!gFoundRootanaDir){
    find_active_root_directory();
  }
  
  // Wrap the request in promise; will combine multiple items (if there are more than 1) into single XHR request
  var listDirectories = "";
  for(var index = 0; index < histogramNameList.length; index++){
    var name = active_directory + "/" + histogramNameList[index];
    listDirectories += name + "/root.json\n";
  }    

  // Make the promise XHR
  var url = rootana_dir + "multi.json?number="+String(histogramNameList.length);
  console.log("XHR: " + url + " , " +listDirectories);
  getUrl(url, listDirectories).then(function(response) {

    promiseAlreadyInFligth = false;
    var histoInfoJSON = JSON.parse(response);

    console.log("Got directories: " + listDirectories);
    // Check that we can find this histogram in current directory list
    if(histoObject == false){
      document.getElementById("readstatus").innerHTML = "Failed to find histogram with name " 
        + histogramName + " in current ROOT directory";
      document.getElementById("readstatus").style.color = 'red';
  
      return;
    }


    console.log("Looping: " + divNames.length);
    // Loop over the divs
    for(var index = 0; index < divNames.length; index++){
            
      var histoObject = findHistogram(histoInfoJSON[0]["fName"]);
      
      // handle 1D vs 2D histograms differently; check first histogram for type
      if(histoObject["_kind"].search("ROOT.TH2") != -1){
        
        // Make the plot-ly 2D plot
        makePlot2D(divNames[index], histoObject,histoInfoJSON[index], index,deleteDygraph);
        
      }else if(histoObject["_kind"].search("ROOT.TH1") != -1){
        
        // Create the CSV array; this happens differently for single/multiple vs overlay histograms.
        var csv_array = makeCSVArray(plotType,histoInfoJSON,histoObject,index);
        // Make the dygraph 1D plot
        makePlot1D(histoInfoJSON[index],plotType,divNames[index],csv_array,deleteDygraph,index)
        
      }else{
        
        document.getElementById("readstatus").innerHTML = "Histogram with name " 
          + histoInfoJSON[0]["fName"] + " is not TH1 or TH2... don't know how to handle.";
        document.getElementById("readstatus").style.color = 'red';
        
      }
               
    }

    
  }).catch(function(error) { // Handle exception if we didn't find the histogram...
    
    document.getElementById("readstatus").innerHTML = "Couldn't get histogram data; request = "+ listDirectories + "; error = " + error + ". Did rootana httpserver die?";
    document.getElementById("readstatus").style.color = 'red';    
    console.log("Couldn't get histogram data; request = "+ listDirectories + "; error = " + error + ". Did rootana httpserver die?");
    // If we couldn't find histogram, try forcing re-find of rootana directory
    gFoundRootanaDir = false;
    find_active_root_directory();
  });

}
